import collections
import importlib.machinery
import importlib.util
import json
import os
import re
import sys
import time
import threading
import traceback

_META_PATTERN = re.compile(
    r"^__(?P<key>[a-z_]+)__\s*=\s*(?P<quote>['\"])(?P<value>.*?)(?P=quote)",
    re.MULTILINE,
)

_META_KEYS = ("id", "name", "version", "author", "description", "min_version", "icon",
              "app_version", "sdk_version")

_loaded = {}

_settings = {}

_errors = {}

_settings_path = None
_log_sink = None

_recent = collections.deque(maxlen=400)

def configure(settings_path, log_sink=None, libs_path=None, libs_progress=None):

    global _settings_path, _log_sink, _settings
    _settings_path = settings_path
    _log_sink = log_sink
    with _settings_lock:
        _settings = _read_settings()
    if libs_path:
        from finegram import libs
        libs.configure(libs_path, libs_progress)

def last_error(plugin_id):

    return _errors.get(plugin_id, "")

def log(message):
    text = str(message)
    _recent.append(text)
    if _log_sink is not None:
        try:
            _log_sink.onPluginLog(text)
            return
        except Exception:
            pass
    print(text)

def recent_log():

    return "\n".join(_recent)

def clear_log():
    _recent.clear()

def read_metadata(path):

    from finegram import bundle
    if bundle.is_bundle(path):
        return bundle.read_metadata(path)

    meta = {}
    try:
        with open(path, "r", encoding="utf-8", errors="ignore") as handle:
            head = handle.read(8192)
    except OSError as error:
        log(f"не удалось прочитать {path}: {error}")
        return meta

    for match in _META_PATTERN.finditer(head):
        key = match.group("key")
        if key in _META_KEYS:
            meta[key] = match.group("value")
    if "id" not in meta:
        meta["id"] = os.path.splitext(os.path.basename(path))[0]
    if "name" not in meta:
        meta["name"] = meta["id"]
    return meta

def metadata_json(path):

    return json.dumps(read_metadata(path), ensure_ascii=False)

def _module_name(plugin_id):
    return "finegram_plugin_" + re.sub(r"[^A-Za-z0-9_]", "_", plugin_id)

_plugins_dir = None
_ids_by_file = {}

def _remember_dir(path):
    global _plugins_dir
    parent = os.path.dirname(os.path.abspath(path))
    if parent and os.path.isdir(parent):
        _plugins_dir = parent

def _alias(plugin_id, module):
    if plugin_id.isidentifier() and plugin_id not in sys.modules:
        sys.modules[plugin_id] = module

def _plugin_file_for(plugin_id):
    if not _plugins_dir:
        return None
    direct = os.path.join(_plugins_dir, plugin_id + ".plugin")
    if os.path.isfile(direct):
        return direct
    try:
        names = os.listdir(_plugins_dir)
    except OSError:
        return None
    for name in names:
        if not name.endswith(".plugin"):
            continue
        path = os.path.join(_plugins_dir, name)
        known = _ids_by_file.get(path)
        if known is None:
            try:
                known = read_metadata(path).get("id") or ""
            except Exception:
                known = ""
            _ids_by_file[path] = known
        if known == plugin_id:
            return path
    return None

def _execute_plugin_file(plugin_id, path):

    module_name = _module_name(plugin_id)
    try:
        stamp = os.path.getmtime(path)
    except OSError:
        stamp = None
    cached = sys.modules.get(module_name)
    if cached is not None and stamp is not None and getattr(cached, "__fg_stamp__", None) == stamp:
        return cached
    loader = importlib.machinery.SourceFileLoader(module_name, path)
    spec = importlib.util.spec_from_file_location(module_name, path, loader=loader)
    if spec is None or spec.loader is None:
        return None
    module = importlib.util.module_from_spec(spec)
    sys.modules[module_name] = module
    try:
        spec.loader.exec_module(module)
    except Exception:
        sys.modules.pop(module_name, None)
        raise
    if stamp is not None:
        module.__fg_stamp__ = stamp
    return module

class _ExistingModuleLoader:
    def __init__(self, module):
        self._module = module

    def create_module(self, spec):
        return self._module

    def exec_module(self, module):
        pass

class _PluginImporter:

    def find_spec(self, fullname, path=None, target=None):
        if path is not None or "." in fullname or fullname.startswith("finegram_plugin_"):
            return None
        module = sys.modules.get(_module_name(fullname))
        if module is None:
            file = _plugin_file_for(fullname)
            if file is None:
                return None
            try:
                module = _execute_plugin_file(fullname, file)
            except Exception:
                log(f"библиотека {fullname} не загрузилась:\n{traceback.format_exc()}")
                return None
            if module is None:
                return None
        return importlib.util.spec_from_loader(fullname, _ExistingModuleLoader(module))

class _LibraryImporter:

    _busy = set()

    def find_spec(self, fullname, path=None, target=None):
        if path is not None or "." in fullname:
            return None
        if fullname in self._busy:
            return None
        try:
            from finegram import libs
        except Exception:
            return None
        if libs.is_java_namespace(fullname):
            return None
        self._busy.add(fullname)
        try:
            if not libs.ensure(fullname):
                return None
        except Exception:
            log(f"библиотека {fullname} не довезлась:\n{traceback.format_exc()}")
            return None
        finally:
            self._busy.discard(fullname)

        return _retry_after_install(fullname, target)

def _retry_after_install(fullname, target):

    for finder in sys.meta_path:
        if isinstance(finder, _LibraryImporter):
            continue
        try:
            spec = finder.find_spec(fullname, None, target)
        except Exception:
            continue
        if spec is not None:
            return spec
    return None

if not any(isinstance(finder, _PluginImporter) for finder in sys.meta_path):
    sys.meta_path.append(_PluginImporter())

if not any(isinstance(finder, _LibraryImporter) for finder in sys.meta_path):
    sys.meta_path.append(_LibraryImporter())

def load(path):

    meta = read_metadata(path)
    plugin_id = meta.get("id") or ""
    if not plugin_id:
        return ""
    _remember_dir(path)
    if plugin_id in _loaded:
        unload(plugin_id)

    module_name = _module_name(plugin_id)
    from finegram import bundle
    if bundle.is_bundle(path):
        return _load_bundle(path, plugin_id, module_name, meta)

    try:
        stamp = os.path.getmtime(path)
    except OSError:
        stamp = None

    cached = sys.modules.get(module_name)
    if cached is not None and stamp is not None and getattr(cached, "__fg_stamp__", None) == stamp:

        module = cached
    else:
        try:

            loader = importlib.machinery.SourceFileLoader(module_name, path)
            spec = importlib.util.spec_from_file_location(module_name, path, loader=loader)
            if spec is None or spec.loader is None:
                log(f"плагин {plugin_id}: не удалось подготовить модуль")
                return ""
            module = importlib.util.module_from_spec(spec)
            sys.modules[module_name] = module
            spec.loader.exec_module(module)
            if stamp is not None:
                module.__fg_stamp__ = stamp
        except ModuleNotFoundError as missing:

            from finegram import libs
            if missing.name and libs.ensure(missing.name):
                try:
                    module = importlib.util.module_from_spec(spec)
                    sys.modules[module_name] = module
                    spec.loader.exec_module(module)
                    if stamp is not None:
                        module.__fg_stamp__ = stamp
                except Exception:
                    _errors[plugin_id] = traceback.format_exc()
                    log(f"плагин {plugin_id} не запустился:\n{traceback.format_exc()}")
                    return ""
            else:
                _errors[plugin_id] = traceback.format_exc()
                log(f"плагин {plugin_id} не запустился:\n{traceback.format_exc()}")
                return ""
        except Exception:
            _errors[plugin_id] = traceback.format_exc()
            log(f"плагин {plugin_id} не запустился:\n{traceback.format_exc()}")
            return ""

    _alias(plugin_id, module)
    instance = _instantiate(module, meta)
    if instance is None:
        _errors[plugin_id] = "в файле нет класса плагина"
        log(f"плагин {plugin_id}: в файле нет класса плагина")
        return ""

    try:
        instance.on_load()
    except Exception:
        _errors[plugin_id] = traceback.format_exc()
        log(f"плагин {plugin_id} упал при включении:\n{traceback.format_exc()}")
        return ""

    _loaded[plugin_id] = instance
    _errors.pop(plugin_id, None)
    log(f"плагин {plugin_id} включён")
    return plugin_id

def _load_bundle(root, plugin_id, module_name, meta):

    from finegram import bundle
    import elyx

    main = bundle.main_path(root)
    if not main:
        _errors[plugin_id] = "в сборке нет точки входа"
        log(f"плагин {plugin_id}: в сборке нет точки входа")
        return ""

    package_dir = os.path.dirname(main)
    elyx.register(plugin_id, root)

    previous = sys.modules.get(module_name)
    if previous is not None:
        _forget_package(module_name)

    try:
        spec = importlib.util.spec_from_file_location(
            module_name, main, submodule_search_locations=[package_dir])
        if spec is None or spec.loader is None:
            log(f"плагин {plugin_id}: не удалось подготовить сборку")
            return ""
        module = importlib.util.module_from_spec(spec)
        sys.modules[module_name] = module
        spec.loader.exec_module(module)
    except Exception:
        _errors[plugin_id] = traceback.format_exc()
        log(f"плагин {plugin_id} не запустился:\n{traceback.format_exc()}")
        _forget_package(module_name)
        return ""

    _alias(plugin_id, module)
    instance = _instantiate(module, meta)
    if instance is None:
        _errors[plugin_id] = "в сборке нет класса плагина"
        log(f"плагин {plugin_id}: в сборке нет класса плагина")
        return ""

    try:
        instance.on_load()
    except Exception:
        _errors[plugin_id] = traceback.format_exc()
        log(f"плагин {plugin_id} упал при включении:\n{traceback.format_exc()}")
        return ""

    _loaded[plugin_id] = instance
    _errors.pop(plugin_id, None)
    log(f"плагин {plugin_id} включён")
    return plugin_id

def _forget_package(module_name):

    prefix = module_name + "."
    for name in [name for name in sys.modules if name == module_name or name.startswith(prefix)]:
        sys.modules.pop(name, None)

def _instantiate(module, meta):

    from finegram.plugin import FinegramPlugin
    from base_plugin import BasePlugin

    bases = (FinegramPlugin, BasePlugin)
    for attr in vars(module).values():
        if isinstance(attr, type) and issubclass(attr, bases) and attr not in bases:
            instance = attr()
            instance.id = meta.get("id", "")
            instance.name = meta.get("name", "")
            instance.version = meta.get("version", "")
            instance.author = meta.get("author", "")
            instance.description = meta.get("description", "")
            return instance
    return None

def unload(plugin_id):
    flush_settings()
    instance = _loaded.pop(plugin_id, None)
    if instance is None:
        return False
    try:
        instance.on_unload()
    except Exception:
        log(f"плагин {plugin_id} упал при выключении:\n{traceback.format_exc()}")

    unhook_all(plugin_id)
    _settings_items.pop(plugin_id, None)
    log(f"плагин {plugin_id} выключен")
    return True

def loaded_ids():
    return list(_loaded.keys())

def is_loaded(plugin_id):
    return plugin_id in _loaded

def _read_settings():

    for path in (_settings_path, _backup_path()):
        if not path or not os.path.exists(path):
            continue
        try:
            with open(path, "r", encoding="utf-8") as handle:
                data = json.load(handle)
            if isinstance(data, dict):
                if path != _settings_path:
                    log("настройки плагинов взяты из запасной копии")
                return data
        except Exception:
            continue
    return {}

def _backup_path():
    return None if not _settings_path else _settings_path + ".bak"

def _plain(value):

    if value is None or isinstance(value, (bool, int, float, str)):
        return value
    if isinstance(value, dict):
        return {str(k): _plain(v) for k, v in value.items()}
    if isinstance(value, (list, tuple)):
        return [_plain(v) for v in value]
    return str(value)

_settings_dirty = False
_settings_timer = None
_settings_lock = threading.RLock()
_SETTINGS_DELAY = 1.5

def _write_settings_now():
    global _settings_dirty
    with _settings_lock:
        if not _settings_path:
            return
        temp = _settings_path + ".tmp"
        backup_temp = _settings_path + ".bak.tmp"
        try:
            payload = json.dumps({k: _plain(v) for k, v in _settings.items()}, ensure_ascii=False)
            with open(temp, "w", encoding="utf-8") as handle:
                handle.write(payload)
                handle.flush()
                os.fsync(handle.fileno())
            previous = None
            if os.path.exists(_settings_path):
                try:
                    with open(_settings_path, "r", encoding="utf-8") as handle:
                        candidate = handle.read()
                    if isinstance(json.loads(candidate), dict):
                        previous = candidate
                except (OSError, ValueError):
                    pass
            if previous is not None:
                with open(backup_temp, "w", encoding="utf-8") as handle:
                    handle.write(previous)
                    handle.flush()
                    os.fsync(handle.fileno())
                os.replace(backup_temp, _backup_path())
            os.replace(temp, _settings_path)
            _settings_dirty = False
        except Exception as error:
            log("настройки плагинов не сохранились: " + str(error))
            for pending in (temp, backup_temp):
                try:
                    if os.path.exists(pending):
                        os.remove(pending)
                except OSError:
                    pass

def _write_settings():
    global _settings_dirty, _settings_timer
    with _settings_lock:
        _settings_dirty = True
        if _settings_timer is not None:
            return
        try:
            _settings_timer = threading.Timer(_SETTINGS_DELAY, _flush_settings)
            _settings_timer.daemon = True
            _settings_timer.start()
        except Exception:
            _settings_timer = None
            _write_settings_now()

def _flush_settings():
    global _settings_timer
    with _settings_lock:
        if _settings_timer is not None:
            _settings_timer.cancel()
            _settings_timer = None
        if _settings_dirty:
            _write_settings_now()

def flush_settings():

    _flush_settings()

def get_setting(plugin_id, key, default=None):
    with _settings_lock:
        return _settings.get(plugin_id, {}).get(key, default)

def all_settings(plugin_id):
    with _settings_lock:
        return dict(_settings.get(plugin_id, {}))

def clear_settings(plugin_id):
    with _settings_lock:
        if _settings.pop(plugin_id, None) is not None:
            _write_settings()

def set_setting(plugin_id, key, value):
    with _settings_lock:
        _settings.setdefault(plugin_id, {})[key] = value
        _write_settings()
    instance = _loaded.get(plugin_id)
    if instance is not None:
        try:
            instance.on_setting_changed(key, value)
        except Exception:
            log(f"плагин {plugin_id}: обработчик настройки упал:\n{traceback.format_exc()}")

_bridge = None

def set_bridge(bridge):
    global _bridge
    _bridge = bridge

def _report(action):
    log(action + ":" + chr(10) + traceback.format_exc())

_name_subs = {}

_substring_subs = {}

_send_message_subs = []

def _push_flags():

    if _bridge is None:
        return
    try:
        names = ",".join(sorted(_name_subs.keys()))
        parts = ",".join(sorted(_substring_subs.keys()))
        _bridge.setDispatchNames(names, parts, bool(_send_message_subs))
    except Exception:
        pass

def add_name_hook(plugin_id, name, match_substring=False):

    if match_substring:
        subs = _substring_subs.setdefault(str(name), [])
    else:
        subs = _name_subs.setdefault(str(name), [])
    if plugin_id not in subs:
        subs.append(plugin_id)
    _push_flags()
    return True

def remove_name_hook(plugin_id, name):
    for storage in (_name_subs, _substring_subs):
        subs = storage.get(str(name))
        if subs and plugin_id in subs:
            subs.remove(plugin_id)
            if not subs:
                storage.pop(str(name), None)
    _push_flags()

def add_send_message_hook(plugin_id):
    if plugin_id not in _send_message_subs:
        _send_message_subs.append(plugin_id)
    _push_flags()
    return True

def _drop_subscriptions(plugin_id):
    for storage in (_name_subs, _substring_subs):
        for name in list(storage.keys()):
            subs = storage[name]
            if plugin_id in subs:
                subs.remove(plugin_id)
            if not subs:
                storage.pop(name, None)
    _file_hooks.pop(plugin_id, None)
    try:
        import intents

        intents.remove_plugin_handlers(plugin_id)
    except Exception:
        pass
    if plugin_id in _send_message_subs:
        _send_message_subs.remove(plugin_id)
    _push_flags()

_CANCEL = "CANCEL"
_MODIFY = "MODIFY"
_MODIFY_FINAL = "MODIFY_FINAL"

def _strategy(result):

    if result is None:
        return ""
    value = getattr(result, "strategy", None)
    if value is None:
        return ""
    text = str(value)
    return text.rsplit(".", 1)[-1].upper()

def _modified(result, field):

    name = _strategy(result)
    if name not in (_MODIFY, _MODIFY_FINAL):
        return None
    return getattr(result, field, None)

_SLOW_HANDLER_MS = 100

_slow_plugins = {}

class _watch:

    __slots__ = ("plugin_id", "started")

    def __init__(self, plugin_id):
        self.plugin_id = str(plugin_id or "?")
        self.started = 0.0

    def __enter__(self):
        self.started = time.monotonic()
        return self

    def __exit__(self, exc_type, exc_value, traceback_obj):
        spent = int((time.monotonic() - self.started) * 1000)
        if spent >= _SLOW_HANDLER_MS:
            stats = _slow_plugins.setdefault(self.plugin_id, [0, 0])
            stats[0] += 1
            if spent > stats[1]:
                stats[1] = spent
            log(f"плагин {self.plugin_id} держал поток {spent} мс")
        return False

def slow_plugins():

    return [f"{plugin_id}:{count}:{longest}"
            for plugin_id, (count, longest) in _slow_plugins.items()]

def forget_slow_plugin(plugin_id):
    _slow_plugins.pop(str(plugin_id), None)

def dispatch_request(name, account, request):

    replacement = None
    for plugin in _subscribers(name):
        handler = getattr(plugin, "pre_request_hook", None)
        if handler is None:
            continue
        try:
            with _watch(getattr(plugin, "id", "?")):
                result = handler(name, account, request)
        except Exception:
            _report("плагин " + str(getattr(plugin, "id", "?")) + ": обработчик запроса упал")
            continue
        kind = _strategy(result)
        if kind == _CANCEL:
            return [True, None]
        changed = _modified(result, "request")
        if changed is not None:
            replacement = changed
            request = changed
        if kind == _MODIFY_FINAL:
            break
    return [False, replacement]

def dispatch_response(name, account, response, error):

    new_response = None
    new_error = None
    for plugin in _subscribers(name):
        handler = getattr(plugin, "post_request_hook", None)
        if handler is None:
            continue
        try:
            with _watch(getattr(plugin, "id", "?")):
                result = handler(name, account, response, error)
        except Exception:
            _report("плагин " + str(getattr(plugin, "id", "?")) + ": обработчик ответа упал")
            continue
        changed = _modified(result, "response")
        if changed is not None:
            new_response = changed
            response = changed
        changed = _modified(result, "error")
        if changed is not None:
            new_error = changed
            error = changed
        if _strategy(result) == _MODIFY_FINAL:
            break
    return [new_response, new_error]

def dispatch_update(name, account, update):

    replacement = None
    for plugin in _subscribers(name):
        handler = getattr(plugin, "on_update_hook", None)
        if handler is None:
            continue
        try:
            with _watch(getattr(plugin, "id", "?")):
                result = handler(name, account, update)
        except Exception:
            _report("плагин " + str(getattr(plugin, "id", "?")) + ": обработчик обновления упал")
            continue
        kind = _strategy(result)
        if kind == _CANCEL:
            return [True, None]
        changed = _modified(result, "update")
        if changed is not None:
            replacement = changed
            update = changed
        if kind == _MODIFY_FINAL:
            break
    return [False, replacement]

def dispatch_send_message(account, params):

    replacement = None
    for plugin_id in list(_send_message_subs):
        plugin = _loaded.get(plugin_id)
        if plugin is None:
            continue
        handler = getattr(plugin, "on_send_message_hook", None)
        if handler is None:
            continue
        try:
            result = handler(account, params)
        except Exception:
            _report("плагин " + plugin_id + ": обработчик отправки упал")
            continue
        kind = _strategy(result)
        if kind == _CANCEL:
            return [True, None]
        changed = _modified(result, "params")
        if changed is not None:
            replacement = changed
            params = changed
        if kind == _MODIFY_FINAL:
            break
    return [False, replacement]

def _subscribers(name):
    text = str(name)
    ids = list(_name_subs.get(text, ()))
    for part, subscribers in _substring_subs.items():
        if part in text:
            ids.extend(subscriber for subscriber in subscribers if subscriber not in ids)

    result = []
    for plugin_id in ids:
        plugin = _loaded.get(plugin_id)
        if plugin is not None:
            result.append(plugin)
    return result

class _HookAdapter:

    def __init__(self, plugin_id, hook):
        self._plugin_id = plugin_id
        self._hook = hook

    def before(self, frame):

        replacement = getattr(self._hook, "replace_hooked_method", None)
        if replacement is not None:
            try:
                frame.setResult(replacement(_wrap_call(frame)))
            except Exception:
                _report("плагин " + str(self._plugin_id) + ": замена метода упала")
            return
        self._call("before_hooked_method", frame)

    def after(self, frame):
        self._call("after_hooked_method", frame)

    def _call(self, method_name, frame):
        handler = getattr(self._hook, method_name, None)
        if handler is None:
            return
        try:
            result = handler(_wrap_call(frame))
        except Exception:
            _report("плагин " + str(self._plugin_id) + ": " + method_name + " упал")
            return

        if result is not None and getattr(result, "strategy", 0) == _CANCEL:
            try:
                frame.setResult(None)
            except Exception:
                pass

def hook_member(plugin_id, member, hook):

    if _bridge is None:
        log("хуки недоступны: мост не готов")
        return 0
    try:
        return int(_bridge.hookMember(plugin_id, member, _HookAdapter(plugin_id, hook)))
    except Exception:
        _report("не удалось перехватить метод")
        return 0

def unhook_member(token):
    if _bridge is None or not token:
        return
    try:
        _bridge.unhookOne(int(token))
    except Exception:
        _report("не снять перехват")

def add_hook(plugin_id, class_name, method_name, param_types, hook):

    if _bridge is None:
        log("хуки недоступны: мост не готов")
        return False
    try:
        return bool(_bridge.hook(plugin_id, class_name, method_name,
                                 [str(t) for t in (param_types or [])],
                                 _HookAdapter(plugin_id, hook)))
    except Exception:
        _report("не удалось перехватить " + str(class_name) + "." + str(method_name))
        return False

def unhook_all(plugin_id):
    _drop_subscriptions(plugin_id)
    if _bridge is None:
        return
    try:
        _bridge.unhookAll(plugin_id)
    except Exception:
        _report("не удалось снять перехваты " + str(plugin_id))

def _owner_of(func):

    module = getattr(func, "__module__", None)
    if module is None:
        bound = getattr(func, "__self__", None)
        if bound is not None:
            module = getattr(type(bound), "__module__", None)
    if not module or not module.startswith("finegram_plugin_"):
        return None
    for plugin_id in list(_loaded.keys()):
        if _module_name(plugin_id) == module:
            return plugin_id
    return None

def _guard(func):

    owner = _owner_of(func)
    if owner is None:
        return func

    def guarded(*args, **kwargs):
        if owner not in _loaded:
            log(f"плагин {owner} выключен — отложенную работу пропускаем")
            return None
        return func(*args, **kwargs)

    return guarded

def run_on_ui_thread(func, delay=0):
    if _bridge is None:
        return
    try:
        _bridge.runOnUiThread(_guard(func), int(delay or 0))
    except Exception:
        _report("не выполнить в потоке интерфейса")

def get_queue(queue_name):

    if _bridge is None:
        return None
    try:
        return _bridge.getQueue(str(queue_name))
    except Exception:
        return None

def run_on_queue(func, delay=0, queue_name="plugins"):
    if _bridge is None:
        return
    try:
        _bridge.runOnQueue(_guard(func), int(delay or 0), str(queue_name))
    except Exception:
        _report("не выполнить в фоновой очереди")

def find_class(class_name):

    try:
        from java import jclass

        return jclass(str(class_name))
    except Exception:
        return None

def load_dex(data, key=None):

    if not data:
        return None
    if _bridge is None:
        log("dex не загрузить: мост не готов")
        return None
    try:
        return _bridge.loadDex(bytes(data), None if key is None else str(key))
    except Exception:
        _report("не загрузить dex плагина")
        return None

def load_dex_class(loader, class_name):

    if loader is None or _bridge is None:
        return None
    try:
        return _bridge.loadDexClass(loader, str(class_name))
    except Exception:
        _report("в dex плагина нет класса " + str(class_name))
        return None

def unpack_payload(path, begin_marker, end_marker):

    try:
        with open(path or "", "r", encoding="utf-8", errors="ignore") as handle:
            body = handle.read()
    except OSError:
        return ""

    start = body.rfind(begin_marker)
    stop = body.rfind(end_marker)
    if start < 0 or stop < 0 or stop < start:
        return ""
    chunk = body[start + len(begin_marker):stop]
    return "".join(
        line.strip()[2:].strip()
        for line in chunk.splitlines()
        if line.strip().startswith("# ")
    )

def get_private_field(obj, field_name):
    if _bridge is None:
        return None
    try:
        return _bridge.getPrivateField(obj, str(field_name))
    except Exception:
        return None

def set_private_field(obj, field_name, value):
    if _bridge is None:
        return
    try:
        _bridge.setPrivateField(obj, str(field_name), value)
    except Exception:
        _report("не записать поле " + str(field_name))

def get_last_fragment():
    return None if _bridge is None else _bridge.getLastFragment()

def get_user_config(account=None):
    return None if _bridge is None else _bridge.getUserConfig(-1 if account is None else int(account))

def get_messages_controller(account=None):
    return None if _bridge is None else _bridge.getMessagesController(-1 if account is None else int(account))

def send_request(request, callback=None, account=None):
    if _bridge is None:
        return 0
    try:
        return _bridge.sendRequest(request, callback, -1 if account is None else int(account))
    except Exception:
        _report("запрос не отправлен")
        return 0

def show_bulletin(text, kind="info", icon=0):

    if _bridge is None:
        log(str(text))
        return
    try:
        _bridge.showBulletin(str(text), str(kind), int(icon or 0))
    except Exception:
        log(str(text))

def show_bulletin_two_line(title, subtitle, kind="info", icon=0):

    if _bridge is None:
        log(str(title) + ": " + str(subtitle))
        return
    try:
        _bridge.showBulletinTwoLine(str(title), str(subtitle), str(kind), int(icon or 0))
    except Exception:
        log(str(title) + ": " + str(subtitle))

def show_bulletin_with_button(text, button_text, on_click=None, icon=0, duration=5000):

    if _bridge is None:
        log(str(text))
        return
    try:
        _bridge.showBulletinWithButton(str(text), str(button_text), on_click,
                                       int(icon or 0), int(duration))
    except Exception:
        log(str(text))

def show_bulletin_undo(text, on_undo=None, subtitle=None):

    if _bridge is None:
        log(str(text))
        return
    try:
        _bridge.showBulletinUndo(str(text), None if subtitle is None else str(subtitle), on_undo)
    except Exception:
        log(str(text))

def show_error_dialog(text):

    if _bridge is None:
        log(str(text))
        return
    try:
        _bridge.showErrorDialog(str(text))
    except Exception:
        log(str(text))

def show_dialog(title, message, buttons=None):
    if _bridge is None:
        log(str(title) + ": " + str(message))
        return
    try:
        pressed = buttons or {}
        positive = pressed.get("positive")
        negative = pressed.get("negative")
        _bridge.showDialog(
            str(title), str(message),
            None if positive is None else str(positive[0]),
            None if positive is None else positive[1],
            None if negative is None else str(negative[0]),
        )
    except Exception:
        _report("диалог не показан")

def copy_to_clipboard(text):
    if _bridge is None:
        return
    try:
        _bridge.copyToClipboard(str(text))
    except Exception:
        _report("не скопировать в буфер обмена")

def application_context():
    return None if _bridge is None else _bridge.applicationContext()

def menu_changed():

    if _bridge is None:
        return
    try:
        _bridge.menuItemsChanged()
    except Exception:
        pass

def _menu_allowed(item, context):

    condition = getattr(item, "condition", None)
    if condition is None:
        return True
    if not callable(condition):
        return bool(condition)
    try:
        if _accepts(condition, 1):
            return bool(condition(context))
        return bool(condition())
    except Exception:
        return False

def _menu_context(context):
    if context is None:
        return {}
    if isinstance(context, dict):
        return context
    try:
        return {str(key): context.get(key) for key in context.keySet()}
    except (AttributeError, TypeError):
        return dict(context)

def menu_items_json(kind=None, context=None, include_hidden=False):

    wanted = None if kind is None else str(kind)
    context = _menu_context(context)
    result = []
    for plugin_id, plugin in _loaded.items():
        getter = getattr(plugin, "get_menu_items", None)
        if getter is None:
            continue
        try:
            items = getter() or []
        except Exception:
            _report("плагин " + str(plugin_id) + ": пункты меню не собрались")
            continue
        for item in items:
            menu_type = str(getattr(item, "menu_type", "") or "")
            menu_type = menu_type.rsplit(".", 1)[-1] if menu_type.startswith("MenuItemType.") else menu_type
            if wanted is not None and menu_type != wanted:
                continue
            if not include_hidden and not _menu_allowed(item, context):
                continue
            result.append({
                "plugin": plugin_id,
                "id": str(getattr(item, "item_id", "")),
                "menu_type": menu_type,
                "text": str(getattr(item, "text", "")),
                "subtext": _text_or_none(getattr(item, "subtext", None)),
                "icon": _text_or_none(getattr(item, "icon", None)),
                "priority": int(getattr(item, "priority", 0) or 0),
            })
    result.sort(key=lambda entry: -entry["priority"])
    return json.dumps(result, ensure_ascii=False)

def _text_or_none(value):
    return None if value is None else str(value)

def menu_click(plugin_id, item_id, context=None):

    plugin = _loaded.get(plugin_id)
    if plugin is None:
        return
    getter = getattr(plugin, "get_menu_items", None)
    if getter is None:
        return
    try:
        items = getter() or []
    except Exception:
        return
    for item in items:
        if str(getattr(item, "item_id", "")) != str(item_id):
            continue
        handler = getattr(item, "on_click", None)
        if handler is None:
            return
        _fire(plugin_id, handler, _menu_context(context))
        return
        return

def _wrap_call(frame):

    try:
        from base_plugin import MethodHookParam

        return MethodHookParam(frame)
    except Exception:
        return frame

_file_hooks = {}
_file_hook_counter = [0]

def add_file_hook(plugin_id, extensions, on_click, name=None):

    if on_click is None:
        return None
    if isinstance(extensions, str):
        extensions = [extensions]
    normalized = {str(e).lower().lstrip(".") for e in (extensions or []) if e}
    if not normalized:
        return None

    _file_hook_counter[0] += 1
    handle = "%s:%d" % (plugin_id, _file_hook_counter[0])
    _file_hooks.setdefault(plugin_id, []).append({
        "handle": handle,
        "extensions": normalized,
        "callback": on_click,
        "name": name or handle,
    })
    _push_file_flags()
    return handle

def remove_file_hook(plugin_id, handle):
    hooks = _file_hooks.get(plugin_id)
    if not hooks:
        return
    _file_hooks[plugin_id] = [h for h in hooks if h["handle"] != handle]
    if not _file_hooks[plugin_id]:
        _file_hooks.pop(plugin_id, None)
    _push_file_flags()

def _push_file_flags():
    if _bridge is None:
        return
    try:
        _bridge.setFileHooksActive(bool(_file_hooks))
    except Exception:
        pass

def dispatch_file(file_name, path, message):

    extension = str(file_name or "").lower().rsplit(".", 1)
    extension = extension[1] if len(extension) == 2 else ""
    if not extension:
        return False

    for plugin_id, hooks in list(_file_hooks.items()):
        if plugin_id not in _loaded:
            continue
        for hook in hooks:
            if extension not in hook["extensions"]:
                continue
            try:
                if hook["callback"](path, message):
                    return True
            except TypeError:
                try:
                    if hook["callback"](path):
                        return True
                except Exception:
                    _report("плагин " + plugin_id + ": обработчик файла упал")
            except Exception:
                _report("плагин " + plugin_id + ": обработчик файла упал")
    return False

def set_intents_active(active):

    if _bridge is None:
        return
    try:
        _bridge.setIntentsActive(bool(active))
    except Exception:
        pass

def dispatch_intent(intent):

    try:
        import intents

        return bool(intents.dispatch_intent(intent))
    except Exception:
        _report("плагины не обработали намерение")
        return False

def plugin_instance(plugin_id):

    return _loaded.get(plugin_id)

def has_plugin(plugin_id):

    return plugin_id in _loaded

def share_dex_loader(stamp, loader):

    if not stamp or loader is None:
        return False
    store = getattr(sys, "_wsbypass_panel", None)
    if store is None:
        store = {}
        try:
            sys._wsbypass_panel = store
        except Exception:
            return False
    try:
        store[str(stamp)] = loader
    except Exception:
        return False

    for name, module in list(sys.modules.items()):
        if not name.startswith("finegram_plugin_"):
            continue
        state = getattr(module, "_wsdash", None)
        if isinstance(state, dict) and state.get("tries"):
            state["tries"] = 0
    return True

def _module_of(plugin_id):

    module = sys.modules.get("finegram_plugin_" + re.sub(r"[^A-Za-z0-9_]", "_", str(plugin_id)))
    if module is not None:
        return module
    for name, candidate in list(sys.modules.items()):
        if getattr(candidate, "__id__", None) == plugin_id:
            return candidate
    return None

def prime_panel(plugin_id, methods):

    module = _module_of(plugin_id)
    if module is None:
        return False
    state = getattr(module, "_wsdash", None)
    if not isinstance(state, dict):
        return False
    try:
        for key in ("cls", "create", "active", "update", "toast", "isopen", "close"):
            value = methods.get(key)
            if value is not None:
                state[key] = value
        state["tries"] = 0
        state["error"] = ""
        return True
    except Exception:
        _report("плагин " + str(plugin_id) + ": не принял готовое окно")
        return False

def call_plugin(plugin_id, method, *args):

    plugin = _loaded.get(plugin_id)
    if plugin is None:
        return None
    handler = getattr(plugin, str(method), None)
    if handler is None or not callable(handler):
        return None
    try:
        return handler(*args)
    except Exception:
        _report("плагин " + str(plugin_id) + ": метод " + str(method) + " упал")
        return None

def plugin_error(plugin_id, method):

    plugin = _loaded.get(plugin_id)
    if plugin is None:
        return "плагин не запущен"
    handler = getattr(plugin, str(method), None)
    if handler is None or not callable(handler):
        return "у плагина нет " + str(method)
    try:
        handler()
        return ""
    except Exception:
        return traceback.format_exc()

_APP_EVENTS = {
    "app_start": "START", "start": "START", "started": "START",
    "app_stop": "STOP", "stop": "STOP", "stopped": "STOP",
    "app_pause": "PAUSE", "pause": "PAUSE", "paused": "PAUSE",
    "app_resume": "RESUME", "resume": "RESUME", "resumed": "RESUME",
    "account_switched": "ACCOUNT_SWITCHED",
}

def _app_event(event_type):

    from base_plugin import AppEvent

    name = _APP_EVENTS.get(str(event_type).lower(), None)
    if name is None:
        return str(event_type)
    return getattr(AppEvent, name, str(event_type))

def dispatch_app_event(event_type):

    event = _app_event(event_type)

    if str(event_type).lower() in ("app_pause", "pause", "paused", "app_stop", "stop", "stopped"):
        flush_settings()
    for plugin_id, plugin in list(_loaded.items()):
        handler = getattr(plugin, "on_app_event", None)
        if handler is None:
            continue
        try:
            handler(event)
        except Exception:
            _report("плагин " + plugin_id + ": обработчик события приложения упал")

def dispatch_updates(container_name, account, updates):

    replacement = None
    for plugin in _subscribers(container_name):
        handler = getattr(plugin, "on_updates_hook", None)
        if handler is None:
            continue
        try:
            result = handler(container_name, account, updates)
        except Exception:
            _report("плагин " + str(getattr(plugin, "id", "?")) + ": обработчик пачки обновлений упал")
            continue
        kind = _strategy(result)
        if kind == _CANCEL:
            return [True, None]
        changed = _modified(result, "updates")
        if changed is not None:
            replacement = changed
            updates = changed
        if kind == _MODIFY_FINAL:
            break
    return [False, replacement]

def export_settings(plugin_id):
    return all_settings(plugin_id)

def import_settings(plugin_id, values):
    if not values:
        return
    changes = {str(key): value for key, value in dict(values).items()}
    with _settings_lock:
        _settings.setdefault(plugin_id, {}).update(changes)
        _write_settings()
    plugin = _loaded.get(plugin_id)
    if plugin is not None:
        for key, value in changes.items():
            try:
                plugin.on_setting_changed(key, value)
            except Exception:
                _report("плагин " + str(plugin_id) + ": обработчик настройки упал")

def reload_settings(plugin_id):

    if _bridge is None:
        return
    try:
        _bridge.reloadPluginSettings(str(plugin_id))
    except Exception:
        pass

_settings_items = {}

_settings_seq = {}

_JAVA_ITEM_FIELDS = (
    "type", "text", "subtext", "key", "icon", "hint", "mask", "items",
    "defaultValue", "multiline", "maxLength", "min", "max", "accent", "red", "linkAlias",
)

_JAVA_ITEM_RENAME = {
    "defaultValue": "default",
    "maxLength": "max_length",
    "linkAlias": "link_alias",
}

def _java_item_to_dict(item):

    data = {}
    for field in _JAVA_ITEM_FIELDS:
        try:
            value = getattr(item, field)
        except Exception:
            continue
        if value is None:
            continue
        if field == "items":
            try:
                value = [str(entry) for entry in value]
            except Exception:
                continue
        elif not isinstance(value, (bool, int, float)):
            value = str(value)
        data[_JAVA_ITEM_RENAME.get(field, field)] = value
    if not data.get("type"):
        return None
    data.setdefault("clickable", data["type"] in ("text", "input", "selector"))
    data.setdefault("has_sub_fragment", False)
    data.setdefault("long_clickable", False)
    return data

def _item_to_dict(item):
    if hasattr(item, "to_dict"):
        return item.to_dict()
    if hasattr(item, "getClass"):
        return _java_item_to_dict(item)
    return dict(item)

def _describe(plugin_id, items):

    result = []
    for index, item in enumerate(items):
        try:
            data = _item_to_dict(item)
        except Exception:
            continue
        if not data:
            continue
        data["kind"] = getattr(item, "kind", None) or data.get("type") or "text"
        data["index"] = index
        key = data.get("key")
        if key:
            data["value"] = get_setting(plugin_id, key, data.get("default"))
        result.append(data)
    return result

def _build(plugin_id, items, screen):

    _settings_items[(plugin_id, screen)] = list(items)
    return json.dumps({"screen": screen, "items": _describe(plugin_id, items)},
                      ensure_ascii=False, default=str)

def forget_settings(plugin_id):

    for key in [k for k in _settings_items if k[0] == plugin_id]:
        _settings_items.pop(key, None)
    _settings_seq.pop(plugin_id, None)

def settings_json(plugin_id):

    plugin = _loaded.get(plugin_id)
    if plugin is None:
        return "[]"
    builder = getattr(plugin, "create_settings", None)
    if builder is None:
        return "[]"
    try:
        items = builder() or []
    except Exception:
        _report("плагин " + str(plugin_id) + ": настройки не собрались")
        return "[]"

    keys = [k for k in _settings_items if k[0] == plugin_id and k[1] != 0]
    for key in sorted(keys, key=lambda item: item[1])[:-16]:
        _settings_items.pop(key, None)
    return _build(plugin_id, items, 0)

def _screen_items(plugin_id, screen):
    return _settings_items.get((plugin_id, int(screen))) or []

def sub_settings_json(plugin_id, screen, index):

    items = _screen_items(plugin_id, screen)
    index = int(index)
    if index < 0 or index >= len(items):
        return "[]"
    builder = getattr(items[index], "create_sub_fragment", None)
    if builder is None:
        return "[]"
    try:
        nested = builder() or []
    except Exception:
        _report("плагин " + str(plugin_id) + ": вложенный экран не собрался")
        return "[]"
    number = int(_settings_seq.get(plugin_id, 0)) + 1
    _settings_seq[plugin_id] = number
    return _build(plugin_id, nested, number)

def _accepts(handler, count):

    import inspect

    try:
        signature = inspect.signature(handler)
    except (TypeError, ValueError):
        return count == 0
    try:
        signature.bind(*range(count))
        return True
    except TypeError:
        return False

def _fire(plugin_id, handler, *args):

    for count in range(len(args), -1, -1):
        if not _accepts(handler, count):
            continue
        try:
            return handler(*args[:count])
        except Exception:
            _report("плагин " + str(plugin_id) + ": обработчик настроек упал")
            return None
    _report("плагин " + str(plugin_id) + ": обработчик настроек не принимает столько аргументов")
    return None

def _pick_row(plugin_id, screen, index, title=None):

    items = _screen_items(plugin_id, screen)
    index = int(index)
    if not items:
        return None
    if title is None:
        return items[index] if 0 <= index < len(items) else None

    wanted = str(title)
    if 0 <= index < len(items) and str(getattr(items[index], "text", "")) == wanted:
        return items[index]
    for item in items:
        if str(getattr(item, "text", "")) == wanted:
            return item
    return items[index] if 0 <= index < len(items) else None

def settings_click(plugin_id, index, screen=0, view=None, title=None):

    item = _pick_row(plugin_id, screen, index, title)
    if item is None:
        return
    handler = getattr(item, "on_click", None)
    if handler is None:
        return
    _fire(plugin_id, handler, view)

def settings_long_click(plugin_id, index, screen=0, view=None):

    items = _screen_items(plugin_id, screen)
    index = int(index)
    if index < 0 or index >= len(items):
        return False
    handler = getattr(items[index], "on_long_click", None)
    if handler is None:
        return False
    _fire(plugin_id, handler, view)
    return True

def settings_changed(plugin_id, index, value, screen=0, title=None):

    item = _pick_row(plugin_id, screen, index, title)
    key = getattr(item, "key", None) if item is not None else None
    if key:
        set_setting(plugin_id, key, value)
    if item is None:
        return
    handler = getattr(item, "on_change", None)
    if handler is None:
        return
    _fire(plugin_id, handler, value)

def settings_view(plugin_id, screen, index, context):

    items = _screen_items(plugin_id, screen)
    index = int(index)
    if index < 0 or index >= len(items):
        return None
    item = items[index]
    builder = getattr(item, "create_view", None)
    if builder is None:
        return None
    try:
        view = builder(context)
    except Exception:
        _report("плагин " + str(plugin_id) + ": своя строка настроек не нарисовалась")
        return None
    binder = getattr(item, "bind_view", None)
    if binder is not None and view is not None:
        try:
            binder(view)
        except Exception:
            _report("плагин " + str(plugin_id) + ": своя строка настроек не обновилась")
    return view
