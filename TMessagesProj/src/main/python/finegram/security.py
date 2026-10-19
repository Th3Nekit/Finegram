import ast
import hashlib
import io
import json
import os
import stat
import struct
import threading
import zipfile
import zlib

_MAX_FILE = 2 * 1024 * 1024
_MAX_TOTAL = 32 * 1024 * 1024
_MAX_FILES = 512
_MAX_NODES = 60000
_MAX_RATIO = 1000
_MAX_PATH = 1024
_approval_path = None
_approved = set()
_lock = threading.RLock()
_cache = {}
_remote = {"urlopen", "urlretrieve", "requests.get", "requests.post", "httpx.get",
           "httpx.post", "urllib.request.urlopen", "urllib.request.urlretrieve"}
_sensitive = {"browser_cookie3", "keyring", "pynput", "keyboard", "win32crypt"}
_native = {".so", ".dex", ".jar", ".apk", ".class", ".zip", ".eaf", ".pyc", ".pyo"}
_dex_apis = {"hook_utils.load_dex", "finegram.runtime.load_dex"}
_dex_modules = {"hook_utils", "finegram.runtime"}

def configure(path):
    global _approval_path, _approved
    with _lock:
        _approval_path = path
        try:
            with open(path, encoding="utf-8") as handle:
                data = json.load(handle)
            _approved = {value for value in data if isinstance(value, str) and len(value) == 64}
        except (OSError, ValueError, TypeError):
            _approved = set()

def _path_name(name, directory=False):
    if not isinstance(name, str) or not name or len(name.encode("utf-8")) > _MAX_PATH:
        raise ValueError("path size")
    if directory and name.endswith("/"):
        name = name[:-1]
    parts = name.split("/")
    if len(parts) > 64 or any(not part or part in {".", ".."} or part.endswith((".", " ")) for part in parts):
        raise ValueError("path")
    if any(ord(char) < 32 or char in '\\:<>"|?*' for char in name):
        raise ValueError("path")
    return name

def _check_paths(entries):
    seen = set()
    aliases = {}
    files = {name for name, directory in entries if not directory}
    for name, directory in entries:
        _path_name(name)
        if name in seen:
            raise ValueError("duplicate path")
        seen.add(name)
        parts = name.split("/")
        for count in range(1, len(parts) + 1):
            parent = "/".join(parts[:count])
            folded = parent.casefold()
            if folded in aliases and aliases[folded] != parent:
                raise ValueError("path alias")
            aliases[folded] = parent
            if (count < len(parts) or directory) and parent in files:
                raise ValueError("file directory conflict")

def _cached_bytecode(name):
    return "__pycache__" in name.split("/")[:-1] and os.path.splitext(name)[1].lower() in {".pyc", ".pyo"}

def _walk_error(error):
    raise error

def _files(path):
    if os.path.islink(path):
        raise ValueError("symlink")
    if not os.path.isdir(path):
        return [(os.path.basename(path), path)]
    files = []
    entries = []
    for root, directories, names in os.walk(path, onerror=_walk_error):
        for name in directories + names:
            file = os.path.join(root, name)
            if os.path.islink(file):
                raise ValueError("symlink")
            relative = os.path.relpath(file, path).replace(os.sep, "/")
            if _cached_bytecode(relative):
                continue
            directory = name in directories
            mode = os.stat(file, follow_symlinks=False).st_mode
            if not (stat.S_ISDIR(mode) if directory else stat.S_ISREG(mode)):
                raise ValueError("file type")
            entries.append((relative, directory))
            if len(entries) > _MAX_FILES * 64:
                raise ValueError("file count")
        directories.sort()
        for name in sorted(names):
            file = os.path.join(root, name)
            relative = os.path.relpath(file, path).replace(os.sep, "/")
            if not _cached_bytecode(relative):
                files.append((relative, file))
                if len(files) > _MAX_FILES:
                    raise ValueError("file count")
    _check_paths(entries)
    return sorted(files)

def _read_file(path, limit):
    if os.path.islink(path) or not stat.S_ISREG(os.stat(path, follow_symlinks=False).st_mode):
        raise ValueError("file type")
    with open(path, "rb") as handle:
        data = handle.read(limit + 1)
    if len(data) > limit:
        raise ValueError("size")
    return data

def _archive_files(data):
    if len(data) > _MAX_TOTAL:
        raise ValueError("archive size")
    with zipfile.ZipFile(io.BytesIO(data)) as archive:
        infos = archive.infolist()
        if not infos or len(infos) > _MAX_FILES:
            raise ValueError("file count")
        end = data.rfind(b"PK\x05\x06", max(0, len(data) - 65557))
        if end < 0 or end + 22 > len(data):
            raise ValueError("archive end")
        _, disk, start_disk, disk_count, count, size, offset, comment = struct.unpack_from("<4s4H2IH", data, end)
        if disk or start_disk or disk_count != count or count != len(infos) or end + 22 + comment != len(data):
            raise ValueError("archive directory")
        if offset != archive.start_dir or offset + size != end:
            raise ValueError("archive layout")
        paths = []
        for info in infos:
            directory = info.is_dir()
            name = _path_name(info.orig_filename, directory)
            mode = stat.S_IFMT(info.external_attr >> 16)
            if mode not in (0, stat.S_IFDIR if directory else stat.S_IFREG):
                raise ValueError("archive file type")
            if _cached_bytecode(name):
                raise ValueError("packed bytecode cache")
            if info.flag_bits & ~0x080e or info.extract_version > 20:
                raise ValueError("archive flags")
            if info.compress_type not in {zipfile.ZIP_STORED, zipfile.ZIP_DEFLATED}:
                raise ValueError("compression")
            if info.file_size > _MAX_TOTAL or info.file_size > max(1, info.compress_size) * _MAX_RATIO:
                raise ValueError("archive size")
            paths.append((name, directory))
        _check_paths(paths)
        if sum(info.file_size for info in infos) > _MAX_TOTAL:
            raise ValueError("size")
        files = {}
        position = 0
        for info in sorted(infos, key=lambda item: item.header_offset):
            if info.header_offset != position or position + 30 > offset:
                raise ValueError("unindexed archive entry")
            header = struct.unpack_from("<4s5H3I2H", data, position)
            signature, version, flags, compression, _, _, crc, compressed, unpacked, name_size, extra_size = header
            start = position + 30 + name_size + extra_size
            stop = start + info.compress_size
            if signature != b"PK\x03\x04" or start > offset or stop > offset:
                raise ValueError("archive header")
            local_name = data[position + 30:position + 30 + name_size]
            if not flags & 0x800 and any(byte >= 128 for byte in local_name):
                raise ValueError("ambiguous path encoding")
            if local_name.decode("utf-8" if flags & 0x800 else "cp437") != info.orig_filename:
                raise ValueError("archive path mismatch")
            if version != info.extract_version or flags != info.flag_bits or compression != info.compress_type:
                raise ValueError("archive header mismatch")
            expected = (info.CRC, info.compress_size, info.file_size)
            if flags & 8:
                if compression != zipfile.ZIP_DEFLATED or (crc, compressed, unpacked) not in ((0, 0, 0), expected):
                    raise ValueError("archive descriptor")
                descriptor = stop + (4 if data[stop:stop + 4] == b"PK\x07\x08" else 0)
                if descriptor + 12 > offset or struct.unpack_from("<3I", data, descriptor) != expected:
                    raise ValueError("archive descriptor")
                position = descriptor + 12
            else:
                if (crc, compressed, unpacked) != expected:
                    raise ValueError("archive size mismatch")
                position = stop
            payload = data[start:stop]
            if compression == zipfile.ZIP_DEFLATED:
                inflater = zlib.decompressobj(-15)
                payload = inflater.decompress(payload, info.file_size + 1)
                if not inflater.eof or inflater.unused_data or inflater.unconsumed_tail:
                    raise ValueError("archive compressed data")
            if len(payload) != info.file_size or zlib.crc32(payload) & 0xffffffff != info.CRC:
                raise ValueError("archive content mismatch")
            if info.is_dir():
                if payload:
                    raise ValueError("directory content")
            else:
                files[_path_name(info.orig_filename)] = payload
        if position != offset:
            block = data[position:offset]
            if len(block) < 32 or block[-16:] != b"APK Sig Block 42":
                raise ValueError("unindexed archive data")
            if struct.unpack_from("<Q", block)[0] != len(block) - 8 or struct.unpack_from("<Q", block, len(block) - 24)[0] != len(block) - 8:
                raise ValueError("apk signing block")
            cursor = 8
            while cursor < len(block) - 24:
                if cursor + 8 > len(block) - 24:
                    raise ValueError("apk signing block")
                length = struct.unpack_from("<Q", block, cursor)[0]
                cursor += 8 + length
                if length < 4 or cursor > len(block) - 24:
                    raise ValueError("apk signing block")
        return files

def _unique_object(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError("duplicate refmap key")
        result[key] = value
    return result

def _main_names(files):
    names = set()
    for name in ("refmap.json", "refmap.yml"):
        if name not in files:
            continue
        if len(files[name]) > _MAX_FILE:
            raise ValueError("refmap size")
        text = files[name].decode("utf-8")
        if name.endswith(".json"):
            refmap = json.loads(text, object_pairs_hook=_unique_object)
            if not isinstance(refmap, dict):
                raise ValueError("refmap")
        else:
            refmap = {}
            for raw in text.splitlines():
                line = raw.strip()
                if not line or line.startswith("#") or ":" not in line:
                    continue
                key, value = line.split(":", 1)
                value = value.split(" #", 1)[0].strip().strip('\"\'')
                if key.strip() and value:
                    refmap.setdefault(key.strip(), value)
        main = refmap.get("main")
        if main is not None:
            main = _path_name(main)
            if main not in files:
                raise ValueError("missing main")
            names.add(main)
    return names

def _name(node, aliases):
    if isinstance(node, ast.Name):
        return "builtins" if node.id == "__builtins__" else aliases.get(node.id, node.id)
    if isinstance(node, ast.Attribute):
        base = _name(node.value, aliases)
        return base + "." + node.attr if base else node.attr
    if isinstance(node, ast.Call) and _name(node.func, aliases) == "getattr" and len(node.args) >= 2:
        if isinstance(node.args[1], ast.Constant) and isinstance(node.args[1].value, str):
            return _name(node.args[0], aliases) + "." + node.args[1].value
    if isinstance(node, ast.Call) and _name(node.func, aliases) in {"__import__", "importlib.import_module"} and node.args:
        if isinstance(node.args[0], ast.Constant) and isinstance(node.args[0].value, str):
            return node.args[0].value
    if isinstance(node, ast.Subscript) and isinstance(node.slice, ast.Constant) and isinstance(node.slice.value, str):
        base = _name(node.value, aliases)
        if base == "builtins":
            return base + "." + node.slice.value
    return ""

def _inspect(data, file):
    tree = ast.parse(data, filename=file)
    nodes = []
    for node in ast.walk(tree):
        nodes.append(node)
        if len(nodes) > _MAX_NODES:
            raise ValueError("node count")
    aliases = {}
    assignments = {}
    findings = []
    for node in nodes:
        if isinstance(node, ast.Import):
            for item in node.names:
                aliases[item.asname or item.name.split(".")[0]] = item.name if item.asname else item.name.split(".")[0]
        elif isinstance(node, ast.ImportFrom):
            for item in node.names:
                aliases[item.asname or item.name] = (node.module or "") + "." + item.name
        elif isinstance(node, (ast.Assign, ast.AnnAssign)):
            targets = node.targets if isinstance(node, ast.Assign) else [node.target]
            for target in targets:
                if isinstance(target, ast.Name) and node.value is not None:
                    assignments.setdefault(target.id, []).append(node.value)
    for _ in range(3):
        for symbol, values in assignments.items():
            if len(values) == 1:
                name = _name(values[0], aliases)
                if name and (name.rsplit(".", 1)[-1] in {"exec", "eval"} or name in _remote
                             or name in _dex_apis or name in _dex_modules):
                    aliases[symbol] = name

    def traits(node, seen=None, depth=0):
        if depth > 20:
            return {"dynamic"}
        seen = seen or set()
        if isinstance(node, ast.Constant):
            return set()
        if isinstance(node, ast.Name):
            if node.id in seen or node.id not in assignments:
                return {"dynamic"}
            return set().union(*(traits(value, seen | {node.id}, depth + 1) for value in assignments[node.id]))
        result = set()
        if isinstance(node, ast.Call):
            name = _name(node.func, aliases)
            if name in _remote or name.startswith(("requests.", "httpx.", "aiohttp.")):
                result.add("remote")
            if name.startswith(("base64.", "marshal.", "zlib.", "bz2.", "lzma.")) or name == "bytes.fromhex":
                result.add("encoded")
            if name == "compile":
                return traits(node.args[0], seen, depth + 1) if node.args else {"dynamic"}
        if isinstance(node, ast.BinOp) and isinstance(node.op, ast.BitXor):
            result.add("encoded")
        for child in ast.iter_child_nodes(node):
            result.update(traits(child, seen, depth + 1))
        return result

    def add(code, node, detail=""):
        finding = {"code": code, "file": file, "line": node.lineno, "detail": detail}
        if finding not in findings:
            findings.append(finding)

    for node in nodes:
        if isinstance(node, (ast.Import, ast.ImportFrom)):
            modules = [item.name for item in node.names] if isinstance(node, ast.Import) else [node.module or ""]
            for module in modules:
                if module.split(".")[0] in _sensitive:
                    add("sensitive_import", node, module)
        if not isinstance(node, ast.Call):
            continue
        name = _name(node.func, aliases)
        if name in {"exec", "eval", "builtins.exec", "builtins.eval"}:
            argument = node.args[0] if node.args else next((item.value for item in node.keywords if item.arg == "source"), None)
            origin = traits(argument) if argument is not None else {"dynamic"}
            if "remote" in origin:
                add("remote_code", node)
            elif "encoded" in origin:
                add("hidden_code", node)
            else:
                add("dynamic_code", node)
        elif name in {"os.system", "os.popen", "subprocess.run", "subprocess.Popen", "subprocess.call"}:
            add("system_command", node)
        elif name.endswith((".getAuthKey", ".getAuthKeyId")):
            add("session_access", node)
        elif name in _dex_apis or name in {"ctypes.CDLL", "ctypes.PyDLL", "ctypes.WinDLL", "ctypes.OleDLL",
                      "ctypes.cdll.LoadLibrary", "ctypes.pydll.LoadLibrary"}:
            add("native_code", node)
    return findings

def scan(path, source=None, check=True):
    digest = hashlib.sha256()
    findings = []
    sources = []
    total = 0
    fingerprint = ""
    errors = []
    status = "blocked"
    try:
        executable_names = set()
        is_bundle = os.path.isdir(path)
        container_native = False
        if is_bundle:
            if source is not None and not isinstance(source, dict):
                raise ValueError("source map")
            entries = _files(path)
            known = {name for name, _ in entries}
            if source is not None:
                for name in source:
                    if _path_name(name) not in known:
                        raise ValueError("source path")
                executable_names.update(source)
            files = {}
            for name, file in entries:
                data = source[name] if source is not None and name in source else _read_file(file, _MAX_TOTAL - total)
                if not isinstance(data, (bytes, bytearray)):
                    raise ValueError("source bytes")
                total += len(data)
                if total > _MAX_TOTAL:
                    raise ValueError("size")
                files[name] = data
        else:
            if os.path.islink(path):
                raise ValueError("symlink")
            data = source if source is not None else _read_file(path, _MAX_TOTAL)
            if not isinstance(data, (bytes, bytearray)) or len(data) > _MAX_TOTAL:
                raise ValueError("source bytes")
            suffix = os.path.splitext(path)[1].lower()
            if (data[:4] in (b"PK\x03\x04", b"PK\x05\x06", b"PK\x07\x08")
                    or suffix in {".zip", ".eaf", ".jar", ".apk"} or zipfile.is_zipfile(io.BytesIO(data))):
                files = _archive_files(data)
                is_bundle = True
                container_native = suffix in {".jar", ".apk"}
            else:
                files = {os.path.basename(path): data}
        if not files:
            raise ValueError("empty bundle")
        if is_bundle:
            executable_names.update(_main_names(files))
        for name, data in sorted(files.items()):
            digest.update(name.encode("utf-8") if is_bundle else b"file")
            digest.update(b"\0")
            digest.update(len(data).to_bytes(8, "big"))
            digest.update(data)
            suffix = os.path.splitext(name)[1].lower()
            if suffix in {".plugin", ".py", ".pyw"} or name in executable_names:
                if len(data) > _MAX_FILE:
                    errors.append({"file": name, "error": "SourceTooLargeForAnalysis"})
                else:
                    sources.append((name, data))
            elif suffix in _native:
                findings.append({"code": "native_code", "file": name, "line": 0, "detail": ""})
        if container_native:
            findings.append({"code": "native_code", "file": os.path.basename(path), "line": 0, "detail": ""})
        fingerprint = digest.hexdigest()
        cache_key = (fingerprint, tuple(sorted(files)), tuple(sorted(executable_names)),
                     os.path.basename(path) if container_native else "")
        with _lock:
            cached = _cache.get(cache_key)
        if not check:
            status = "skipped"
        elif cached is None:
            for name, data in sources:
                try:
                    findings.extend(_inspect(data, name))
                except Exception as error:
                    errors.append({"file": name, "error": type(error).__name__})
            if not errors:
                with _lock:
                    if len(_cache) >= 128:
                        _cache.clear()
                    _cache[cache_key] = findings
        else:
            findings = list(cached)
        if check:
            status = "error" if errors else "unsafe" if findings else "clean"
    except (OSError, ValueError, UnicodeError, SyntaxError, RecursionError, MemoryError,
            TypeError, EOFError, RuntimeError, NotImplementedError, struct.error, zipfile.BadZipFile, zlib.error):
        fingerprint = ""
        findings = []
        errors = [{"file": os.path.basename(path), "error": "InvalidOrUnreadableFile"}]
        status = "blocked"
    with _lock:
        approved = bool(fingerprint and fingerprint in _approved)
    return {"fingerprint": fingerprint, "findings": findings[:40], "errors": errors[:40], "status": status,
            "requires_review": (status != "clean" or bool(findings)) and not approved,
            "reviewable": bool(fingerprint), "approved": approved}

def approve(path, fingerprint):
    report = scan(path)
    if not fingerprint or report["fingerprint"] != fingerprint or not report["reviewable"]:
        return False
    with _lock:
        if not _approval_path:
            return False
        values = _approved | {fingerprint}
        temporary = _approval_path + ".tmp"
        try:
            with open(temporary, "w", encoding="utf-8") as handle:
                json.dump(sorted(values), handle)
                handle.flush()
                os.fsync(handle.fileno())
            os.replace(temporary, _approval_path)
        except OSError:
            return False
        _approved.add(fingerprint)
    return True

class ReviewRequired(ImportError):
    def __init__(self, path):
        super().__init__("Plugin code requires review: " + os.path.basename(path))
        self.path = os.path.abspath(path)

def require_reviewed(path, source=None):
    report = scan(path, source)
    if report["requires_review"]:
        raise ReviewRequired(path)
    return report
