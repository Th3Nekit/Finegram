import json
import os
import sys

_PREFIX = "finegram_plugin_"

_roots = {}

_meta_cache = {}

_strings_cache = {}

def register(plugin_id, root):

    _roots[plugin_id] = root
    _meta_cache.pop(plugin_id, None)
    _strings_cache.pop(plugin_id, None)

def forget(plugin_id):
    _roots.pop(plugin_id, None)
    _meta_cache.pop(plugin_id, None)
    _strings_cache.pop(plugin_id, None)

def current_id():

    depth = 1
    while depth < 40:
        try:
            frame = sys._getframe(depth)
        except ValueError:
            return ""
        name = frame.f_globals.get("__name__", "")
        if name.startswith(_PREFIX):
            tail = name[len(_PREFIX):]
            return tail.split(".", 1)[0]
        depth += 1
    return ""

def _root_of(plugin_id):
    if not plugin_id:
        return None
    return _roots.get(plugin_id)

def _plugin_id_to_real(marker):

    import re
    for plugin_id in _roots:
        if re.sub(r"[^A-Za-z0-9_]", "_", plugin_id) == marker:
            return plugin_id
    return marker

def _here():
    return _plugin_id_to_real(current_id())

class _Metainfo:

    def all(self):
        plugin_id = _here()
        cached = _meta_cache.get(plugin_id)
        if cached is not None:
            return cached
        root = _root_of(plugin_id)
        if root is None:
            return {}
        from finegram import bundle
        data = bundle.read_metadata(root)
        _meta_cache[plugin_id] = data
        return data

    def get(self, key, default=None):
        value = self.all().get(key)
        return default if value is None else value

    def __getitem__(self, key):
        return self.all()[key]

class _Assets:

    def dir(self):
        root = _root_of(_here())
        if root is None:
            return None
        from finegram import bundle
        return bundle.assets_dir(root)

    def get(self, name):

        base = self.dir()
        if not base or not name:
            return None
        full = os.path.normpath(os.path.join(base, str(name)))
        if not full.startswith(os.path.normpath(base)):
            return None
        return full if os.path.exists(full) else None

    def read(self, name, binary=False):
        path = self.get(name)
        if not path:
            return None
        mode = "rb" if binary else "r"
        try:
            if binary:
                with open(path, mode) as handle:
                    return handle.read()
            with open(path, mode, encoding="utf-8") as handle:
                return handle.read()
        except OSError:
            return None

class _Strings:

    def _language(self):
        try:
            from org.telegram.messenger import LocaleController
            return LocaleController.getInstance().getCurrentLocale().getLanguage()
        except Exception:
            return "en"

    def all(self):
        plugin_id = _here()
        cached = _strings_cache.get(plugin_id)
        if cached is not None:
            return cached
        root = _root_of(plugin_id)
        if root is None:
            return {}
        from finegram import bundle
        folder = bundle.strings_dir(root)
        data = {}
        if folder:

            for language in ("en", self._language()):
                path = os.path.join(folder, "strings_%s.json" % language)
                if not os.path.exists(path):
                    continue
                try:
                    with open(path, "r", encoding="utf-8") as handle:
                        loaded = json.load(handle)
                    if isinstance(loaded, dict):
                        data.update(loaded)
                except (OSError, ValueError):
                    continue
        _strings_cache[plugin_id] = data
        return data

    def get(self, key, default=None):
        value = self.all().get(key)
        if value is None:
            return key if default is None else default
        return value

    def pluralize(self, count, key):

        table = self.all()
        number = int(count)
        forms = []
        for suffix in ("_one", "_few", "_many", "_other"):
            value = table.get(key + suffix)
            if value:
                forms.append((suffix, value))
        if not forms:
            return "%d %s" % (number, self.get(key))

        chosen = dict(forms)
        tail = number % 10
        hundred = number % 100
        if tail == 1 and hundred != 11 and "_one" in chosen:
            word = chosen["_one"]
        elif 2 <= tail <= 4 and not 12 <= hundred <= 14 and "_few" in chosen:
            word = chosen["_few"]
        elif "_many" in chosen:
            word = chosen["_many"]
        else:
            word = chosen.get("_other") or forms[-1][1]
        return word.replace("%d", str(number)).replace("{}", str(number)) \
            if ("%d" in word or "{}" in word) else "%d %s" % (number, word)

class _Settings:

    def get(self, key, default=None):
        from finegram import runtime
        return runtime.get_setting(_here(), key, default)

    def set(self, key, value):
        from finegram import runtime
        runtime.set_setting(_here(), key, value)

metainfo = _Metainfo()
assets = _Assets()
strings = _Strings()
settings = _Settings()
