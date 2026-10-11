// It is licensed under GNU GPL v. 2 or later.

#include "art_symbols.h"

#include <elf.h>
#include <fcntl.h>
#include <link.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <android/log.h>

#include <cstring>
#include <mutex>
#include <string>
#include <vector>

extern "C" {
#include "xz.h"
}

#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, "FGHook", __VA_ARGS__)

namespace fghook {
namespace {

#if defined(__LP64__)
using Ehdr = Elf64_Ehdr;
using Shdr = Elf64_Shdr;
using Sym = Elf64_Sym;
#else
using Ehdr = Elf32_Ehdr;
using Shdr = Elf32_Shdr;
using Sym = Elf32_Sym;
#endif

constexpr size_t kMaxUnpacked = 256u << 20;

struct ArtImage {
    uintptr_t bias = 0;
    std::string path;
    const uint8_t *file = nullptr;
    size_t file_size = 0;
    std::vector<uint8_t> debugdata;

    ~ArtImage() {
        if (file != nullptr) {
            munmap(const_cast<uint8_t *>(file), file_size);
        }
    }
};

std::mutex g_lock;
ArtImage *g_image = nullptr;
bool g_tried = false;

bool Inside(size_t size, uint64_t offset, uint64_t length) {
    return offset <= size && length <= size - offset;
}

const Shdr *Sections(const uint8_t *data, size_t size, size_t *count) {
    if (size < sizeof(Ehdr)) return nullptr;
    auto *header = reinterpret_cast<const Ehdr *>(data);
    if (memcmp(header->e_ident, ELFMAG, SELFMAG) != 0) return nullptr;
    if (header->e_shentsize != sizeof(Shdr) || header->e_shnum == 0) return nullptr;
    if (!Inside(size, header->e_shoff, uint64_t(header->e_shnum) * sizeof(Shdr))) return nullptr;
    *count = header->e_shnum;
    return reinterpret_cast<const Shdr *>(data + header->e_shoff);
}

uint64_t Scan(const uint8_t *data, size_t size, std::string_view prefix) {
    size_t count = 0;
    const Shdr *sections = Sections(data, size, &count);
    if (sections == nullptr) return 0;
    for (size_t i = 0; i < count; ++i) {
        const Shdr &table = sections[i];
        if (table.sh_type != SHT_SYMTAB && table.sh_type != SHT_DYNSYM) continue;
        if (table.sh_link >= count || table.sh_entsize != sizeof(Sym)) continue;
        const Shdr &strings = sections[table.sh_link];
        if (!Inside(size, table.sh_offset, table.sh_size)) continue;
        if (!Inside(size, strings.sh_offset, strings.sh_size)) continue;

        auto *symbols = reinterpret_cast<const Sym *>(data + table.sh_offset);
        auto *names = reinterpret_cast<const char *>(data + strings.sh_offset);
        const size_t total = table.sh_size / sizeof(Sym);
        for (size_t j = 0; j < total; ++j) {
            const Sym &symbol = symbols[j];
            if (symbol.st_value == 0 || symbol.st_shndx == SHN_UNDEF) continue;
            if (symbol.st_name >= strings.sh_size) continue;
            const size_t room = strings.sh_size - symbol.st_name;
            if (room <= prefix.size()) continue;
            if (memcmp(names + symbol.st_name, prefix.data(), prefix.size()) == 0) {
                return symbol.st_value;
            }
        }
    }
    return 0;
}

bool Unxz(const uint8_t *input, size_t size, std::vector<uint8_t> *out) {
    xz_crc32_init();
    xz_crc64_init();
    xz_dec *decoder = xz_dec_init(XZ_DYNALLOC, 1u << 26);
    if (decoder == nullptr) return false;

    out->resize(size * 8 > (1u << 20) ? size * 8 : (1u << 20));
    xz_buf buffer{};
    buffer.in = input;
    buffer.in_size = size;
    buffer.out = out->data();
    buffer.out_size = out->size();

    bool done = false;
    for (;;) {
        const xz_ret ret = xz_dec_run(decoder, &buffer);
        if (ret == XZ_STREAM_END) {
            done = true;
            break;
        }

        if (ret == XZ_UNSUPPORTED_CHECK) continue;
        if (ret != XZ_OK) break;
        if (buffer.out_pos == buffer.out_size) {
            if (out->size() >= kMaxUnpacked) break;
            out->resize(out->size() * 2);
            buffer.out = out->data();
            buffer.out_size = out->size();
        }
    }
    xz_dec_end(decoder);
    if (done) {
        out->resize(buffer.out_pos);
    } else {
        out->clear();
    }
    return done;
}

void UnpackDebugData(ArtImage *image) {
    size_t count = 0;
    const Shdr *sections = Sections(image->file, image->file_size, &count);
    if (sections == nullptr) return;
    auto *header = reinterpret_cast<const Ehdr *>(image->file);
    if (header->e_shstrndx >= count) return;
    const Shdr &names = sections[header->e_shstrndx];
    if (!Inside(image->file_size, names.sh_offset, names.sh_size)) return;

    constexpr std::string_view kName = ".gnu_debugdata";
    for (size_t i = 0; i < count; ++i) {
        const Shdr &section = sections[i];
        if (section.sh_name >= names.sh_size) continue;
        if (names.sh_size - section.sh_name <= kName.size()) continue;
        auto *name = reinterpret_cast<const char *>(image->file + names.sh_offset + section.sh_name);
        if (memcmp(name, kName.data(), kName.size()) != 0 || name[kName.size()] != '\0') continue;
        if (!Inside(image->file_size, section.sh_offset, section.sh_size)) return;
        if (!Unxz(image->file + section.sh_offset, section.sh_size, &image->debugdata)) {
            LOGW("не удалось распаковать .gnu_debugdata у %s", image->path.c_str());
        }
        return;
    }
}

int FindLoadedArt(dl_phdr_info *info, size_t, void *data) {
    if (info->dlpi_name == nullptr) return 0;
    const std::string_view name(info->dlpi_name);
    constexpr std::string_view kSuffix = "/libart.so";
    if (name.size() < kSuffix.size() || name.substr(name.size() - kSuffix.size()) != kSuffix) return 0;
    auto *image = static_cast<ArtImage *>(data);
    image->bias = info->dlpi_addr;
    image->path = info->dlpi_name;
    return 1;
}

ArtImage *Load() {
    auto *image = new ArtImage();
    dl_iterate_phdr(FindLoadedArt, image);
    if (image->bias == 0 || image->path.empty() || image->path[0] != '/') {
        LOGW("libart не найдена среди загруженных библиотек");
        delete image;
        return nullptr;
    }
    const int fd = open(image->path.c_str(), O_RDONLY | O_CLOEXEC);
    if (fd < 0) {
        LOGW("не открыть %s", image->path.c_str());
        delete image;
        return nullptr;
    }
    struct stat info {};
    if (fstat(fd, &info) == 0 && info.st_size > 0) {
        void *mapped = mmap(nullptr, info.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
        if (mapped != MAP_FAILED) {
            image->file = static_cast<const uint8_t *>(mapped);
            image->file_size = info.st_size;
        }
    }
    close(fd);
    if (image->file == nullptr) {
        delete image;
        return nullptr;
    }
    UnpackDebugData(image);
    return image;
}

}

void *FindArtSymbolByPrefix(std::string_view prefix) {
    if (prefix.empty()) return nullptr;
    std::lock_guard<std::mutex> guard(g_lock);
    if (!g_tried) {
        g_tried = true;
        g_image = Load();
    }
    if (g_image == nullptr) return nullptr;
    uint64_t value = Scan(g_image->file, g_image->file_size, prefix);
    if (value == 0 && !g_image->debugdata.empty()) {
        value = Scan(g_image->debugdata.data(), g_image->debugdata.size(), prefix);
    }
    return value == 0 ? nullptr : reinterpret_cast<void *>(g_image->bias + value);
}

void ReleaseArtSymbols() {
    std::lock_guard<std::mutex> guard(g_lock);
    delete g_image;
    g_image = nullptr;
    g_tried = false;
}

}
