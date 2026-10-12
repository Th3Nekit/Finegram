#include "v2wasm/SignalingFraming.h"

#include "v2wasm/CoreGzip.h"

namespace tgcalls {
namespace v2wasm {

namespace {

constexpr size_t kV2DecompressSizeLimit = 2 * 1024 * 1024;

void appendSeq(std::vector<uint8_t> &buffer, uint32_t seq) {
    buffer.push_back(uint8_t((seq >> 24) & 0xff));
    buffer.push_back(uint8_t((seq >> 16) & 0xff));
    buffer.push_back(uint8_t((seq >> 8) & 0xff));
    buffer.push_back(uint8_t(seq & 0xff));
}

}

SignalingFraming::SignalingFraming(Delegate delegate) :
_delegate(std::move(delegate)) {
}

void SignalingFraming::sendMessage(std::string const &message) {

    std::vector<uint8_t> body(message.begin(), message.end());
    auto compressed = coreGzipData(body);
    if (!compressed) {
        _delegate.log("ERROR! Could not gzip signaling message");
        return;
    }
    const uint32_t seq = ++_counter;
    std::vector<uint8_t> packet;
    packet.reserve(4 + compressed->size());
    appendSeq(packet, seq);
    packet.insert(packet.end(), compressed->begin(), compressed->end());
    _delegate.sendPacket(std::move(packet));
}

void SignalingFraming::receivePacket(std::vector<uint8_t> const &packet) {
    if (packet.size() < 5) {
        return;
    }

    std::vector<uint8_t> body(packet.begin() + 4, packet.end());
    if (coreIsGzip(body)) {
        auto decompressed = coreGunzipData(body, kV2DecompressSizeLimit);
        if (!decompressed) {
            _delegate.log("ERROR! Could not decompress signaling data");
            return;
        }
        body = std::move(*decompressed);
    }
    _delegate.deliverMessage(std::string(body.begin(), body.end()));
}

}
}
