import json
import os
import re

REFMAP = "refmap.json"
REFMAP_YAML = "refmap.yml"

_DEFAULT_MAIN = "main.py"
_DEFAULT_META = "metainfo.json"
_MAX_METADATA_BYTES = 2 * 1024 * 1024

def is_bundle(path):

    if not os.path.isdir(path):
        return False
    return any(os.path.isfile(os.path.join(path, name)) for name in (REFMAP, REFMAP_YAML)) or _find_default(path) is not None

def _find_default(root):

    direct = os.path.join(root, _DEFAULT_MAIN)
    if os.path.isfile(direct):
        return direct
    try:
        names = sorted(os.listdir(root))
    except OSError:
        return None
    for name in names:
        nested = os.path.join(root, name, _DEFAULT_MAIN)
        if os.path.isfile(nested):
            return nested
        deeper = os.path.join(root, name, "src", _DEFAULT_MAIN)
        if os.path.isfile(deeper):
            return deeper
    return None

def read_refmap(root):
    for name in (REFMAP, REFMAP_YAML):
        path = _resolve(root, name)
        if path and os.path.isfile(path):
            return _read_object(path)
    return {}

def _read_object(path):
    try:
        with open(path, "rb") as handle:
            raw = handle.read(_MAX_METADATA_BYTES + 1)
        if len(raw) > _MAX_METADATA_BYTES:
            return {}
        text = raw.decode("utf-8")
        if path.endswith(".yml"):
            data = {}
            for raw_line in text.splitlines():
                line = raw_line.strip()
                if not line or line.startswith("#") or ":" not in line:
                    continue
                key, value = line.split(":", 1)
                value = value.split(" #", 1)[0].strip().strip('\"\'')
                if key.strip() and value:
                    data.setdefault(key.strip(), value)
        else:
            data = json.loads(text)
        return data if isinstance(data, dict) else {}
    except (OSError, ValueError):
        return {}

def _resolve(root, value):

    if not isinstance(value, str) or not value:
        return None
    root = os.path.realpath(root)
    full = os.path.realpath(os.path.join(root, value))
    try:
        if os.path.commonpath((root, full)) != root:
            return None
    except ValueError:
        return None
    return full if os.path.exists(full) else None

def main_path(root):

    refmap = read_refmap(root)
    return _resolve(root, refmap.get("main")) or _find_default(root)

def assets_dir(root):
    refmap = read_refmap(root)
    found = _resolve(root, refmap.get("assets"))
    if found:
        return found
    main = main_path(root)
    if not main:
        return None

    for name in ("res", "assets"):
        guess = os.path.join(os.path.dirname(os.path.dirname(main)), name)
        if os.path.isdir(guess):
            return guess
    return None

def strings_dir(root):
    refmap = read_refmap(root)
    found = _resolve(root, refmap.get("strings"))
    if found:
        return found
    main = main_path(root)
    if not main:
        return None
    for name in ("locales", "strings"):
        guess = os.path.join(os.path.dirname(os.path.dirname(main)), name)
        if os.path.isdir(guess):
            return guess
    return None

def read_metadata(root):

    refmap = read_refmap(root)
    meta_name = refmap.get("metainfo")
    path = _resolve(root, meta_name)
    if meta_name and path is None:
        return {}
    if path is None:
        path = _resolve(root, _DEFAULT_META)
    if path is None:
        main = main_path(root)
        if main:
            for parent in (os.path.dirname(main), os.path.dirname(os.path.dirname(main))):
                guess = _resolve(root, os.path.relpath(os.path.join(parent, _DEFAULT_META), root))
                if guess and os.path.isfile(guess):
                    path = guess
                    break
    meta = {}
    if path:
        data = _read_object(path)
        meta = {key: str(value) for key, value in data.items()
                if isinstance(value, (str, int, float))}
    if not re.fullmatch(r"[A-Za-z0-9_-][A-Za-z0-9._-]{0,99}", meta.get("id", "")):
        return {}
    if not meta.get("name"):
        meta["name"] = meta["id"]
    return meta
