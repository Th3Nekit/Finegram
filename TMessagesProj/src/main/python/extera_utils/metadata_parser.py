import finegram.runtime as _runtime

def get_metadata(path):

    try:
        return _runtime.read_metadata(path) or {}
    except Exception:
        return {}

def parse_metadata(path):

    return get_metadata(path)
