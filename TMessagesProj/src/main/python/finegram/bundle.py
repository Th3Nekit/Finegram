import json
import os

REFMAP = "refmap.json"

_DEFAULT_MAIN = "main.py"
_DEFAULT_META = "metainfo.json"

def is_bundle(path):

    if not os.path.isdir(path):
        return False
    return os.path.exists(os.path.join(path, REFMAP)) or _find_default(path) is not None

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

    path = os.path.join(root, REFMAP)
    if not os.path.exists(path):
        return {}
    try:
        with open(path, "r", encoding="utf-8") as handle:
            data = json.load(handle)
        return data if isinstance(data, dict) else {}
    except (OSError, ValueError):
        return {}

def _resolve(root, value):

    if not value:
        return None
    full = os.path.normpath(os.path.join(root, value))
    if not full.startswith(os.path.normpath(root)):
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
    path = _resolve(root, refmap.get("metainfo"))
    if path is None:
        main = main_path(root)
        if main:
            guess = os.path.join(os.path.dirname(os.path.dirname(main)), _DEFAULT_META)
            path = guess if os.path.exists(guess) else None
    meta = {}
    if path:
        try:
            with open(path, "r", encoding="utf-8") as handle:
                data = json.load(handle)
            if isinstance(data, dict):
                meta = {key: str(value) for key, value in data.items()
                        if isinstance(value, (str, int, float))}
        except (OSError, ValueError):
            meta = {}
    if not meta.get("id"):
        meta["id"] = os.path.basename(os.path.normpath(root))
    if not meta.get("name"):
        meta["name"] = meta["id"]
    return meta
