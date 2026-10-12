import functools
import re

from finegram import runtime

class _NameValue:

    __slots__ = ("_owner", "_name", "value")

    def __init__(self, owner, name, value):
        self._owner = owner
        self._name = name
        self.value = value

    @property
    def name(self):
        return self._name

    def __str__(self):
        return "%s.%s" % (self._owner, self._name)

    __repr__ = __str__

    def __eq__(self, other):
        if isinstance(other, _NameValue):
            return self.value == other.value
        return other == self.value or other == str(self)

    def __ne__(self, other):
        return not self.__eq__(other)

    def __hash__(self):
        return hash(self.value)

    def __bool__(self):
        return True

class HookStrategy:

    DEFAULT = _NameValue("HookStrategy", "DEFAULT", "DEFAULT")
    MODIFY = _NameValue("HookStrategy", "MODIFY", "MODIFY")
    CANCEL = _NameValue("HookStrategy", "CANCEL", "CANCEL")
    MODIFY_FINAL = _NameValue("HookStrategy", "MODIFY_FINAL", "MODIFY_FINAL")

    BLOCK = CANCEL

    @classmethod
    def _values(cls):
        return (cls.DEFAULT, cls.MODIFY, cls.CANCEL, cls.MODIFY_FINAL)

    def __new__(cls, value):

        for item in cls._values():
            if item == value or item.name == value:
                return item
        raise ValueError("неизвестная стратегия: %r" % (value,))

class HookResult:

    __slots__ = ("strategy", "request", "response", "update", "updates", "error",
                 "params", "result", "args")

    def __init__(self, strategy=None, request=None, response=None, update=None,
                 updates=None, error=None, params=None, result=None, args=None):
        self.strategy = HookStrategy.DEFAULT if strategy is None else strategy
        self.request = request
        self.response = response
        self.update = update
        self.updates = updates
        self.error = error
        self.params = params
        self.result = result
        self.args = args

    def __repr__(self):
        return "HookResult(strategy=%s)" % (self.strategy,)

class AppEvent:

    START = _NameValue("AppEvent", "START", "app_start")
    STOP = _NameValue("AppEvent", "STOP", "app_stop")
    PAUSE = _NameValue("AppEvent", "PAUSE", "app_pause")
    RESUME = _NameValue("AppEvent", "RESUME", "app_resume")

    STARTED = START
    RESUMED = RESUME
    PAUSED = PAUSE
    ACCOUNT_SWITCHED = _NameValue("AppEvent", "ACCOUNT_SWITCHED", "account_switched")

    @classmethod
    def _values(cls):
        return (cls.START, cls.STOP, cls.PAUSE, cls.RESUME, cls.ACCOUNT_SWITCHED)

    def __new__(cls, value):
        for item in cls._values():
            if item == value or item.name == value:
                return item
        raise ValueError("неизвестное событие: %r" % (value,))

class PluginError(Exception):

    def __init__(self, message, plugin_id=None):
        super().__init__(message)
        self.plugin_id = plugin_id

class MethodHookParam:

    def __init__(self, frame):
        self._frame = frame

    @property
    def thisObject(self):
        return self._frame.thisObject

    @property
    def args(self):
        return self._frame.args

    @property
    def method(self):
        return getattr(self._frame, "method", None)

    def getResult(self):
        return self._frame.getResult()

    def setResult(self, value):
        self._frame.setResult(value)

    result = property(getResult, setResult)

    def getThrowable(self):
        try:
            return self._frame.getThrowable()
        except Exception:
            return None

    def setThrowable(self, error):
        try:
            self._frame.setThrowable(error)
        except Exception:
            pass

    def invokeOriginal(self):

        try:
            return self._frame.invokeOriginalMethod()
        except Exception:
            return None

class XposedHook:
    pass

class BaseHook(XposedHook):
    def before_hooked_method(self, param):
        return None

    def after_hooked_method(self, param):
        return None

class MethodHook(BaseHook):
    pass

class MethodReplacement(BaseHook):
    pass

class _FilteredHook(MethodHook):

    def __init__(self, inner, before_filters, after_filters):
        self._inner = inner
        self._before = before_filters
        self._after = after_filters

    def before_hooked_method(self, param):
        if self._before and not _filters_pass(self._before, param):
            return None
        return self._inner.before_hooked_method(param)

    def after_hooked_method(self, param):
        if self._after and not _filters_pass(self._after, param):
            return None
        return self._inner.after_hooked_method(param)

def hook_wrapper(before=None, after=None):

    class _Wrapper(MethodHook):
        def before_hooked_method(self, param):
            return before(param) if before is not None else None

        def after_hooked_method(self, param):
            return after(param) if after is not None else None

    return _Wrapper()

class HookFilterData:

    def __init__(self, kind, arg_index=None, value=None, instance_of=None, parts=None):
        self.kind = kind
        self.arg_index = arg_index
        self.value = value
        self.instance_of = instance_of
        self.parts = parts or []

    def matches(self, param):
        try:
            return _FILTERS[self.kind](self, param)
        except Exception:
            return False

    def to_java_filter(self):
        return self

def _argument(data, param):
    args = param.args
    index = data.arg_index or 0
    return args[index] if args is not None and 0 <= index < len(args) else None

_FILTERS = {
    "result_instance_of": lambda d, p: isinstance(p.result, d.instance_of) if d.instance_of else False,
    "result_equal": lambda d, p: p.result == d.value,
    "result_not_equal": lambda d, p: p.result != d.value,
    "argument_is_null": lambda d, p: _argument(d, p) is None,
    "argument_not_null": lambda d, p: _argument(d, p) is not None,
    "argument_is_true": lambda d, p: bool(_argument(d, p)) is True,
    "argument_is_false": lambda d, p: bool(_argument(d, p)) is False,
    "argument_equal": lambda d, p: _argument(d, p) == d.value,
    "argument_not_equal": lambda d, p: _argument(d, p) != d.value,
    "argument_instance_of": lambda d, p: isinstance(_argument(d, p), d.instance_of) if d.instance_of else False,
    "any": lambda d, p: any(part.matches(p) for part in d.parts),
    "all": lambda d, p: all(part.matches(p) for part in d.parts),
    "result_is_null": lambda d, p: p.result is None,
    "result_not_null": lambda d, p: p.result is not None,
    "result_is_true": lambda d, p: p.result is True or p.result == True,
    "result_is_false": lambda d, p: p.result is False or p.result == False,
    "condition": lambda d, p: _condition_matches(d, p),
}

_FILTERS["argument_is_instance_of"] = _FILTERS["argument_instance_of"]
_FILTERS["result_is_instance_of"] = _FILTERS["result_instance_of"]

_JAVA_ROOTS = ("org", "com", "android", "androidx", "java", "javax", "kotlin", "dalvik")
_DOTTED = re.compile(r"\b(?:%s)(?:\.[A-Za-z_$][\w$]*)+" % "|".join(_JAVA_ROOTS))
_STRING = re.compile(r"(\"(?:\\.|[^\"\\])*\"|'(?:\\.|[^'\\])*')")
_INSTANCEOF = re.compile(r"([\w.\[\]()]+)\s+instanceof\s+(_jstatic\('[^']+'\))")
_compiled_conditions = {}
_resolved_statics = {}

def _resolve_static(name):
    if name in _resolved_statics:
        return _resolved_statics[name]
    from java import jclass
    parts = name.split(".")
    value = None
    for cut in range(len(parts), 0, -1):
        head, rest = parts[:cut], parts[cut:]
        candidates = [".".join(head)]

        for inner in range(len(head) - 1, 0, -1):
            candidates.append(".".join(head[:inner]) + "$" + "$".join(head[inner:]))
        for class_name in candidates:
            try:
                value = jclass(class_name)
            except Exception:
                continue
            for attr in rest:
                value = getattr(value, attr)
            _resolved_statics[name] = value
            return value
    raise NameError(name)

def _translate_condition(expression):
    out = []
    for index, piece in enumerate(_STRING.split(expression)):
        if index % 2 == 1:
            out.append(piece)
            continue
        piece = piece.replace("&&", " and ").replace("||", " or ")
        piece = re.sub(r"!(?!=)", " not ", piece)
        piece = re.sub(r"\bnull\b", "None", piece)
        piece = re.sub(r"\btrue\b", "True", piece)
        piece = re.sub(r"\bfalse\b", "False", piece)
        piece = _DOTTED.sub(lambda m: "_jstatic(%r)" % m.group(0), piece)
        piece = _INSTANCEOF.sub(r"_instanceof(\1, \2)", piece)
        out.append(piece)
    return "".join(out).strip()

def _condition_matches(data, param):
    expression = data.value or ""
    code = _compiled_conditions.get(expression)
    try:
        if code is None:
            code = compile(_translate_condition(expression), "<condition>", "eval")
            _compiled_conditions[expression] = code
        scope = {"param": param, "obj": data.instance_of, "_jstatic": _resolve_static,
                 "_instanceof": lambda value, clazz: isinstance(value, clazz)}
        return bool(eval(code, {"__builtins__": {}}, scope))
    except Exception:
        return True

def _filters_pass(filters, param):
    for item in filters:
        if isinstance(item, HookFilterData) and not item.matches(param):
            return False
    return True

def hook_filters(*filters):

    def decorator(func):
        @functools.wraps(func)
        def wrapper(*args, **kwargs):
            if args and not _filters_pass(filters, args[-1]):
                return None
            return func(*args, **kwargs)

        wrapper.__hook_filters__ = tuple(filters)
        return wrapper

    return decorator

fn_hook_filters = hook_filters

class HookFilter:

    RESULT_IS_NULL = HookFilterData("result_is_null")
    RESULT_NOT_NULL = HookFilterData("result_not_null")
    RESULT_IS_TRUE = HookFilterData("result_is_true")
    RESULT_IS_FALSE = HookFilterData("result_is_false")

    @staticmethod
    def Condition(expression, obj=None):
        return HookFilterData("condition", value=str(expression), instance_of=obj)

    @staticmethod
    def ResultIsInstanceOf(clazz):
        return HookFilterData("result_instance_of", instance_of=clazz)

    @staticmethod
    def ResultEqual(value):
        return HookFilterData("result_equal", value=value)

    @staticmethod
    def ResultNotEqual(value):
        return HookFilterData("result_not_equal", value=value)

    @staticmethod
    def ArgumentIsNull(index):
        return HookFilterData("argument_is_null", arg_index=index)

    @staticmethod
    def ArgumentNotNull(index):
        return HookFilterData("argument_not_null", arg_index=index)

    @staticmethod
    def ArgumentIsTrue(index):
        return HookFilterData("argument_is_true", arg_index=index)

    @staticmethod
    def ArgumentIsFalse(index):
        return HookFilterData("argument_is_false", arg_index=index)

    @staticmethod
    def ArgumentEqual(index, value):
        return HookFilterData("argument_equal", arg_index=index, value=value)

    @staticmethod
    def ArgumentNotEqual(index, value):
        return HookFilterData("argument_not_equal", arg_index=index, value=value)

    @staticmethod
    def ArgumentIsInstanceOf(index, clazz):
        return HookFilterData("argument_instance_of", arg_index=index, instance_of=clazz)

    @staticmethod
    def Or(*filters):
        return HookFilterData("any", parts=list(filters))

    @staticmethod
    def And(*filters):
        return HookFilterData("all", parts=list(filters))

class MenuItemType:
    CHAT_ACTION_MENU = _NameValue("MenuItemType", "CHAT_ACTION_MENU", "chat_action_menu")
    DRAWER_MENU = _NameValue("MenuItemType", "DRAWER_MENU", "drawer_menu")
    MAIN_MENU = _NameValue("MenuItemType", "MAIN_MENU", "main_menu")
    MESSAGE_CONTEXT_MENU = _NameValue("MenuItemType", "MESSAGE_CONTEXT_MENU", "message_context_menu")
    PROFILE_ACTION_MENU = _NameValue("MenuItemType", "PROFILE_ACTION_MENU", "profile_action_menu")

    PROFILE_MENU = PROFILE_ACTION_MENU

    @classmethod
    def _values(cls):
        return (cls.CHAT_ACTION_MENU, cls.DRAWER_MENU, cls.MESSAGE_CONTEXT_MENU,
                cls.PROFILE_ACTION_MENU)

    def __new__(cls, value):
        for item in cls._values():
            if item == value or item.name == value:
                return item
        raise ValueError("неизвестный вид меню: %r" % (value,))

class MenuItemData:

    def __init__(self, menu_type=None, text="", on_click=None, item_id=None, icon=None,
                 subtext=None, condition=None, priority=0):
        self.menu_type = menu_type
        self.text = text
        self.on_click = on_click
        self.item_id = item_id
        self.icon = icon
        self.subtext = subtext
        self.condition = condition
        self.priority = priority

class BasePlugin:

    id = ""
    name = ""
    version = ""
    author = ""
    description = ""
    min_version = ""
    icon = None

    def __init__(self):
        self._hook_tokens = []
        self._hook_names = []
        self._menu_items = {}
        self._file_hooks = []
        self._intent_hooks = []
        self._subscriptions = []
        self.enabled = False

    def on_plugin_load(self):
        pass

    def on_plugin_unload(self):
        pass

    def create_settings(self):

        return []

    def on_app_event(self, event_type):
        pass

    def pre_request_hook(self, request_name, account, request):

        return HookResult()

    def post_request_hook(self, request_name, account, response, error):

        return HookResult()

    def on_update_hook(self, update_name, account, update):

        return HookResult()

    def on_updates_hook(self, container_name, account, updates):

        return HookResult()

    def on_send_message_hook(self, account, params):

        return HookResult()

    def on_load(self):
        self.enabled = True
        self.on_plugin_load()

    def on_unload(self):
        self.enabled = False
        try:
            self.on_plugin_unload()
        finally:
            for handle in list(self._file_hooks):
                self.remove_file_hook(handle)
            self._hook_tokens = []
            self._hook_names = []
            self._menu_items = {}
            self._file_hooks = []
            self._intent_hooks = []
            for subscription in self._subscriptions:
                try:
                    subscription.remove()
                except Exception:
                    pass
            self._subscriptions = []
            runtime.unhook_all(self.id)

    def on_setting_changed(self, key, value):
        handler = getattr(self, "on_plugin_setting_changed", None)
        if handler is not None:
            handler(key, value)

    def add_hook(self, name, match_substring=False, priority=0):

        self._hook_names.append(str(name))
        return runtime.add_name_hook(self.id, name, bool(match_substring))

    def remove_hook(self, name):
        if str(name) in self._hook_names:
            self._hook_names.remove(str(name))
        runtime.remove_name_hook(self.id, name)

    def add_on_send_message_hook(self, priority=0):

        return runtime.add_send_message_hook(self.id)

    def observe_notifications(self, handler, *notification_ids, account=None):

        import client_utils

        subscription = client_utils.observe_notifications(
            handler, *notification_ids, account=account)
        if subscription is not None:
            self._subscriptions.append(subscription)
        return subscription

    def hook_method(self, method, hook=None, priority=None, before=None, after=None,
                    before_filters=(), after_filters=(), **kwargs):

        handler = hook if hook is not None else hook_wrapper(before, after)
        if (before_filters or after_filters) and not isinstance(handler, MethodReplacement):
            handler = _FilteredHook(handler, tuple(before_filters or ()), tuple(after_filters or ()))
        token = runtime.hook_member(self.id, method, handler)
        if token:
            self._hook_tokens.append(token)
            return token
        return None

    def hook_all_methods(self, target, method_name, hook=None, priority=None, before=None, after=None,
                         before_filters=(), after_filters=(), **kwargs):

        clazz = runtime.find_class(target) if isinstance(target, str) else target
        if clazz is None:
            return []
        tokens = []
        for member in _declared_methods(clazz):
            try:
                if member.getName() != method_name:
                    continue
                member.setAccessible(True)
            except Exception:
                continue
            token = self.hook_method(member, hook, priority, before, after,
                                     before_filters=before_filters, after_filters=after_filters)
            if token:
                tokens.append(token)
        return tokens

    def hook_all_constructors(self, target, hook=None, priority=None, before=None, after=None,
                              before_filters=(), after_filters=(), **kwargs):

        clazz = runtime.find_class(target) if isinstance(target, str) else target
        if clazz is None:
            return []
        tokens = []
        for member in _declared_constructors(clazz):
            try:
                member.setAccessible(True)
            except Exception:
                continue
            token = self.hook_method(member, hook, priority, before, after,
                                     before_filters=before_filters, after_filters=after_filters)
            if token:
                tokens.append(token)
        return tokens

    def unhook_method(self, token):
        if not token:
            return
        if token in self._hook_tokens:
            self._hook_tokens.remove(token)
        runtime.unhook_member(token)

    def hook_class_method(self, class_name, method_name, param_types=None, hook=None):

        return runtime.add_hook(self.id, class_name, method_name, param_types or [], hook)

    def add_file_hook(self, extensions, on_click, name=None):

        handle = runtime.add_file_hook(self.id, extensions, on_click, name)
        if handle:
            self._file_hooks.append(handle)
        return handle

    def remove_file_hook(self, handle):
        if handle in self._file_hooks:
            self._file_hooks.remove(handle)
        runtime.remove_file_hook(self.id, handle)

    def add_intent_hook(self, filters, callback, priority=0, name=None):

        if callback is None:
            return None
        import intents

        handle = intents.IntentsManager.get_instance().add_handler(
            self.id, filters, callback, priority, name)
        if handle is not None:
            self._intent_hooks.append(handle)
        return handle

    def remove_intent_hook(self, handle):
        if handle is None:
            return
        if handle in self._intent_hooks:
            self._intent_hooks.remove(handle)
        try:
            handle.unhandle()
        except Exception:
            pass

    def add_menu_item(self, item_data):
        item_id = getattr(item_data, "item_id", None) or str(len(self._menus()))
        item_data.item_id = item_id
        self._menus()[item_id] = item_data
        runtime.menu_changed()
        return item_id

    def remove_menu_item(self, item_id):
        removed = self._menus().pop(item_id, None) is not None
        if removed:
            runtime.menu_changed()
        return removed

    def _menus(self):

        return self.__dict__.setdefault("_menu_items", {})

    def get_menu_items(self, menu_type=None):
        items = list(self._menus().values())
        if menu_type is None:
            return items
        return [item for item in items if item.menu_type == menu_type]

    def get_setting(self, key, default=None):
        return runtime.get_setting(self.id, key, default)

    def set_setting(self, key, value, reload_settings=False):
        runtime.set_setting(self.id, key, value)

        if reload_settings:
            runtime.reload_settings(self.id)

    def export_settings(self):

        return runtime.export_settings(self.id)

    def import_settings(self, settings, reload_settings=True):

        runtime.import_settings(self.id, settings)
        if reload_settings:
            runtime.reload_settings(self.id)

    def reload_settings(self):

        runtime.reload_settings(self.id)

    def log(self, message):
        runtime.log("[" + str(self.id) + "] " + str(message))

def _declared_methods(clazz):

    for accessor in (lambda c: c.getClass().getDeclaredMethods(), lambda c: c.getDeclaredMethods()):
        try:
            methods = accessor(clazz)
            if methods is not None:
                return list(methods)
        except Exception:
            continue
    return []

def _declared_constructors(clazz):
    for accessor in (lambda c: c.getClass().getDeclaredConstructors(), lambda c: c.getDeclaredConstructors()):
        try:
            found = accessor(clazz)
            if found is not None:
                return list(found)
        except Exception:
            continue
    return []

_JAVA_EXPORTS = {
    "PluginsController": "com.exteragram.messenger.plugins.PluginsController",
    "PluginsConstants": "com.exteragram.messenger.plugins.PluginsConstants",
}

def __getattr__(name):

    class_name = _JAVA_EXPORTS.get(name)
    if class_name is None:
        raise AttributeError(name)
    from java import jclass
    value = jclass(class_name)
    globals()[name] = value
    return value

def log(message):

    runtime.log(message)
