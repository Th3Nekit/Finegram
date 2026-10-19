"""Среда плагина, собранного папкой.

У сборки сведения разложены по файлам, и добраться до них плагин должен, не зная, куда
его распаковали. Модуль один на всех, а отвечает каждому про него самого: чей вызов,
видно по имени модуля в стеке — плагины грузятся как `finegram_plugin_<опознаватель>`.

    from elyx import metainfo, assets, strings, settings
"""

import json
import os
import sys

_PREFIX = "finegram_plugin_"

#: опознаватель плагина -> папка, куда он распакован
_roots = {}

#: опознаватель -> метаданные из metainfo.json
_meta_cache = {}

#: опознаватель -> переводы под текущий язык
_strings_cache = {}


def register(plugin_id, root):
    """Зовётся рантаймом при загрузке сборки."""
    _roots[plugin_id] = root
    _meta_cache.pop(plugin_id, None)
    _strings_cache.pop(plugin_id, None)


def forget(plugin_id):
    _roots.pop(plugin_id, None)
    _meta_cache.pop(plugin_id, None)
    _strings_cache.pop(plugin_id, None)


def current_id():
    """Чей код сейчас выполняется: первый кадр стека, чей модуль принадлежит плагину."""
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
    """Имя модуля собрано из опознавателя, где чужие знаки заменены подчёркиванием."""
    import re
    for plugin_id in _roots:
        if re.sub(r"[^A-Za-z0-9_]", "_", plugin_id) == marker:
            return plugin_id
    return marker


def _here():
    return _plugin_id_to_real(current_id())


class _Metainfo:
    """Что плагин написал о себе в metainfo.json."""

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
    """Файлы, которые приехали вместе с плагином."""

    def dir(self):
        root = _root_of(_here())
        if root is None:
            return None
        from finegram import bundle
        return bundle.assets_dir(root)

    def get(self, name):
        """Путь к файлу; None — такого файла нет. Наружу из папки ресурсов имя не выходит."""
        base = self.dir()
        if not base or not name:
            return None
        try:
            base = os.path.realpath(base)
            full = os.path.realpath(os.path.join(base, str(name)))
            if os.path.commonpath((base, full)) != base:
                return None
            return full if os.path.exists(full) else None
        except (OSError, ValueError):
            return None

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
    """Переводы плагина: strings_<язык>.json рядом с кодом."""

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
            # Английский снизу, язык клиента поверх: в переводе бывают не все ключи.
            for language in ("en", self._language()):
                path = bundle._resolve(folder, "strings_%s.json" % language)
                if not path or bundle._resolve(root, os.path.relpath(path, root)) is None:
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
        """Число и слово при нём: формы записаны как `<ключ>_one`, `_few`, `_many`."""
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
    """Те же настройки плагина, что и у однофайловых: хранятся рядом, читаются так же."""

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
