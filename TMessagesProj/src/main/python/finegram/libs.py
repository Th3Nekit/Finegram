import hashlib
import importlib
import importlib.metadata
import io
import json
import os
import shutil
import stat
import sys
import threading
import time
import zipfile
from email.parser import Parser
from urllib.error import HTTPError
from urllib.request import Request, urlopen

from packaging.markers import default_environment
from packaging.requirements import Requirement
from packaging.specifiers import SpecifierSet
from packaging.utils import canonicalize_name
from packaging.version import InvalidVersion, Version

_INDEX = "https://pypi.org/pypi/%s/json"
_TIMEOUT = 20
_MAX_BYTES = 12 * 1024 * 1024
_MAX_UNPACKED = 48 * 1024 * 1024
_ROOT = None
_PROGRESS = None
_FAILED = {}
_IN_FLIGHT = set()
_LOCK = threading.RLock()
_JAVA_ROOTS = frozenset(("android", "androidx", "java", "javax", "dalvik", "com", "org", "kotlin"))
_MODULE_TO_PACKAGE = {
    "PIL": "Pillow", "bs4": "beautifulsoup4", "yaml": "PyYAML",
    "dateutil": "python-dateutil", "attr": "attrs", "OpenSSL": "pyOpenSSL",
    "Crypto": "pycryptodome", "serial": "pyserial", "cv2": "opencv-python",
    "sklearn": "scikit-learn", "docx": "python-docx", "pptx": "python-pptx",
    "fitz": "PyMuPDF", "magic": "python-magic", "jwt": "PyJWT",
    "telebot": "pyTelegramBotAPI",
}

def configure(root, progress=None):
    global _ROOT, _PROGRESS
    with _LOCK:
        if _ROOT != root:
            _FAILED.clear()
        _ROOT, _PROGRESS = root, progress
        _rescan()

def _rescan():
    if not _ROOT or not os.path.isdir(_ROOT):
        return
    for name in installed():
        path = os.path.join(_ROOT, name)
        if path not in sys.path:
            sys.path.append(path)
    importlib.invalidate_caches()

def _say(package, done, total):
    if _PROGRESS is not None:
        try:
            _PROGRESS.onLibraryProgress(package, int(done), int(total))
        except Exception:
            pass

def _log(message):
    from finegram.runtime import log
    log(message)

def package_for(module):
    return _MODULE_TO_PACKAGE.get(module, module)

def is_java_namespace(module):
    return str(module).split(".", 1)[0] in _JAVA_ROOTS

def _python_matches(specifier):
    return not specifier or Version("%d.%d.%d" % sys.version_info[:3]) in SpecifierSet(specifier)

def _pure_wheel(release):
    for item in release:
        name = item.get("filename") or ""
        if item.get("packagetype") != "bdist_wheel" or item.get("yanked"):
            continue
        if not name.endswith(("-py3-none-any.whl", "-py2.py3-none-any.whl")):
            continue
        if (item.get("size") or 0) > _MAX_BYTES:
            continue
        try:
            if _python_matches(item.get("requires_python")):
                return item
        except Exception:
            continue
    return None

def _select_wheel(meta, requirement):
    releases = meta.get("releases") or {}
    versions = []
    for name in releases:
        try:
            version = Version(name)
        except InvalidVersion:
            continue
        if version in requirement.specifier:
            versions.append((version, name))
    for _, name in sorted(versions, reverse=True):
        wheel = _pure_wheel(releases[name])
        if wheel is not None:
            return name, wheel
    return None, None

def _fetch(url, limit, package=None, total=0):
    request = Request(url, headers={"User-Agent": "Finegram"})
    with urlopen(request, timeout=_TIMEOUT) as response:
        if package is None:
            data = response.read(limit + 1)
        else:
            chunks, got = [], 0
            while True:
                chunk = response.read(64 * 1024)
                if not chunk:
                    break
                chunks.append(chunk)
                got += len(chunk)
                if got > limit:
                    raise ValueError("слишком большой ответ")
                _say(package, got, total)
            data = b"".join(chunks)
    if len(data) > limit:
        raise ValueError("слишком большой ответ")
    return data

def _metadata(archive):
    names = [n for n in archive.namelist() if n.endswith(".dist-info/METADATA")]
    if len(names) != 1:
        raise ValueError("нет описания пакета")
    return Parser().parsestr(archive.read(names[0]).decode("utf-8", "replace"))

def _dependencies(archive, extras=()):
    environment = default_environment()
    out = []
    for value in _metadata(archive).get_all("Requires-Dist", []):
        requirement = Requirement(value)
        if requirement.url:
            raise ValueError("зависимость вне PyPI")
        if requirement.marker is None or any(
                requirement.marker.evaluate(dict(environment, extra=extra))
                for extra in ({""} | set(extras))):
            out.append(requirement)
    return out

def _unpack(archive, target):
    if sum(entry.file_size for entry in archive.infolist()) > _MAX_UNPACKED:
        raise ValueError("слишком большой пакет")
    for entry in archive.infolist():
        parts = entry.filename.replace("\\", "/").split("/")
        if entry.filename.startswith(("/", "\\")) or any(p in ("..", ".") or ":" in p for p in parts):
            raise ValueError("недопустимый путь в пакете")
        if stat.S_ISLNK(entry.external_attr >> 16):
            raise ValueError("ссылка в пакете")
        if parts[0].endswith(".data"):
            if len(parts) < 3 or parts[1] != "purelib":
                continue
            parts = parts[2:]
        destination = os.path.join(target, *parts)
        if entry.is_dir():
            os.makedirs(destination, exist_ok=True)
        else:
            os.makedirs(os.path.dirname(destination), exist_ok=True)
            with archive.open(entry) as source, open(destination, "wb") as output:
                shutil.copyfileobj(source, output)

def _available(requirement):
    try:
        return Version(importlib.metadata.version(requirement.name)) in requirement.specifier
    except (importlib.metadata.PackageNotFoundError, InvalidVersion):
        return False

def ensure(module, depth=0):
    if not _ROOT or depth > 6 or is_java_namespace(module):
        return False
    try:
        requirement = module if isinstance(module, Requirement) else Requirement(package_for(str(module)))
    except Exception:
        return False
    if requirement.url or is_java_namespace(requirement.name):
        return False
    key = canonicalize_name(requirement.name)
    with _LOCK:
        if key in _IN_FLIGHT:
            return True
        if _available(requirement) and not requirement.extras:
            return True
        failed_until = _FAILED.get(str(requirement), 0)
        if failed_until > time.monotonic():
            return False
        _IN_FLIGHT.add(key)
        try:
            return _install(requirement, key, depth)
        except Exception as error:
            retry = 3600 if isinstance(error, HTTPError) and error.code == 404 else 20
            _FAILED[str(requirement)] = time.monotonic() + retry
            _log("библиотека %s: установка не завершена (%s)" % (requirement.name, error))
            return False
        finally:
            _IN_FLIGHT.discard(key)

def _install(requirement, key, depth):
    target = os.path.join(_ROOT, key)
    if os.path.isdir(target):
        if target not in sys.path:
            sys.path.append(target)
        importlib.invalidate_caches()
        if _available(requirement):
            distributions = list(importlib.metadata.distributions(path=[target]))
            if distributions:
                environment = default_environment()
                for value in distributions[0].requires or []:
                    dependency = Requirement(value)
                    if dependency.marker is None or any(dependency.marker.evaluate(dict(environment, extra=e)) for e in ({""} | requirement.extras)):
                        if not ensure(dependency, depth + 1):
                            return False
            return True
        _log("библиотека %s: установленная версия не подходит (%s)" % (requirement.name, requirement.specifier))
        return False
    if _available(requirement):
        distribution = importlib.metadata.distribution(requirement.name)
        environment = default_environment()
        for value in distribution.requires or []:
            dependency = Requirement(value)
            if dependency.marker is None or any(dependency.marker.evaluate(dict(environment, extra=e)) for e in ({""} | requirement.extras)):
                if not ensure(dependency, depth + 1):
                    return False
        return True
    meta = json.loads(_fetch(_INDEX % requirement.name, 4 * 1024 * 1024).decode("utf-8"))
    version, wheel = _select_wheel(meta, requirement)
    if wheel is None:
        _FAILED[str(requirement)] = time.monotonic() + 3600
        _log("библиотека %s: нет совместимой версии для Python %d.%d" % (requirement.name, *sys.version_info[:2]))
        return False
    _say(requirement.name, 0, wheel.get("size") or 0)
    blob = _fetch(wheel["url"], _MAX_BYTES, requirement.name, wheel.get("size") or 0)
    digest = (wheel.get("digests") or {}).get("sha256")
    if not digest or hashlib.sha256(blob).hexdigest() != digest:
        raise ValueError("контрольная сумма не совпала")
    staging = target + ".part"
    _wipe(staging)
    try:
        with zipfile.ZipFile(io.BytesIO(blob)) as archive:
            metadata = _metadata(archive)
            if canonicalize_name(metadata["Name"] or "") != key or metadata["Version"] != version:
                raise ValueError("описание не совпало с пакетом")
            if not _python_matches(metadata["Requires-Python"]):
                raise ValueError("версия Python не поддерживается")
            for dependency in _dependencies(archive, requirement.extras):
                if not ensure(dependency, depth + 1):
                    _log("библиотека %s: не установлена зависимость %s" % (requirement.name, dependency))
                    return False
            _unpack(archive, staging)
        os.rename(staging, target)
    finally:
        _wipe(staging)
    sys.path.append(target)
    importlib.invalidate_caches()
    _FAILED.pop(str(requirement), None)
    _log("библиотека %s %s: установлена" % (requirement.name, version))
    return True

def _wipe(path):
    if path and os.path.isdir(path):
        shutil.rmtree(path)

def installed():
    if not _ROOT or not os.path.isdir(_ROOT):
        return []
    return sorted(name for name in os.listdir(_ROOT)
                  if os.path.isdir(os.path.join(_ROOT, name)) and not name.endswith(".part"))

def forget(package):
    if not _ROOT:
        return
    key = canonicalize_name(package)
    with _LOCK:
        path = os.path.join(_ROOT, key)
        if path in sys.path:
            sys.path.remove(path)
        _FAILED.clear()
        _wipe(path)
        importlib.invalidate_caches()
