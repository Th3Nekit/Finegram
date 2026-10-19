"""Для плагинов: намерения Android.

Когда в клиент приходит намерение — им поделились ссылкой, открыли файл, нажали на кнопку
в другом приложении — плагин может забрать его себе. Плагин описывает, что ему интересно,
и получает обращение раньше, чем клиент возьмётся за дело сам.

Фильтр — обычный словарь: `{"action": "android.intent.action.SEND", "type": "text/plain"}`.
Совпадением считается только то, что плагин перечислил: остальное не проверяется.
"""

from finegram import runtime


class HandlerNotRegistered(Exception):
    """Обработчика с таким именем нет."""


class IntentContext:
    """Пришедшее намерение и то, что с ним можно сделать."""

    def __init__(self, intent, matched):
        self.intent = intent
        self.matched = matched or {}
        self._consumed = False

    @property
    def action(self):
        try:
            return str(self.intent.getAction() or "")
        except Exception:
            return ""

    @property
    def type(self):
        try:
            return str(self.intent.getType() or "")
        except Exception:
            return ""

    @property
    def data(self):
        """Ссылка из намерения строкой."""
        try:
            uri = self.intent.getData()
            return str(uri.toString()) if uri is not None else ""
        except Exception:
            return ""

    def extra(self, key, default=None):
        """Дополнительное поле намерения."""
        try:
            extras = self.intent.getExtras()
            if extras is None:
                return default
            value = extras.get(str(key))
            return default if value is None else value
        except Exception:
            return default

    def consume(self):
        """Клиенту это намерение больше не показывать."""
        self._consumed = True

    @property
    def consumed(self):
        return self._consumed


class HandlerInfo:
    """Зарегистрированный обработчик."""

    def __init__(self, plugin_id, filters, callback, priority=0, name=None):
        self.plugin_id = plugin_id
        self.filters = dict(filters or {})
        self.callback = callback
        self.priority = int(priority or 0)
        self.name = name or ("%s:%d" % (plugin_id, id(callback)))

    def matches(self, context):
        """Правда, если намерение подходит под то, что просил плагин."""
        for key, expected in self.filters.items():
            actual = _field(context, key)
            if isinstance(expected, (list, tuple, set)):
                if actual not in {str(one) for one in expected}:
                    return False
            elif str(expected) != actual:
                return False
        return True


def _field(context, key):
    name = str(key).lower()
    if name == "action":
        return context.action
    if name in ("type", "mime", "mime_type"):
        return context.type
    if name in ("data", "uri", "url"):
        return context.data
    if name == "scheme":
        data = context.data
        return data.split(":", 1)[0] if ":" in data else ""
    if name == "host":
        try:
            uri = context.intent.getData()
            return str(uri.getHost() or "") if uri is not None else ""
        except Exception:
            return ""
    value = context.extra(key)
    return "" if value is None else str(value)


class _HandlerHandle:
    """То, чем обработчик снимают."""

    def __init__(self, manager, info):
        self._manager = manager
        self._info = info

    @property
    def name(self):
        return self._info.name

    def unhandle(self):
        self._manager.remove_handler(self._info)


class IntentsManager:
    """Реестр обработчиков намерений."""

    _instance = None

    def __init__(self):
        self._handlers = []

    @classmethod
    def get_instance(cls):
        if cls._instance is None:
            cls._instance = IntentsManager()
        return cls._instance

    def add_handler(self, plugin_id, filters, callback, priority=0, name=None):
        if callback is None:
            return None
        info = HandlerInfo(plugin_id, filters, callback, priority, name)
        self._handlers.append(info)
        self._handlers.sort(key=lambda h: -h.priority)
        runtime.set_intents_active(True)
        return _HandlerHandle(self, info)

    def remove_handler(self, info):
        if isinstance(info, _HandlerHandle):
            info = info._info
        if info in self._handlers:
            self._handlers.remove(info)
        runtime.set_intents_active(bool(self._handlers))

    def remove_plugin_handlers(self, plugin_id):
        self._handlers = [h for h in self._handlers if h.plugin_id != plugin_id]
        runtime.set_intents_active(bool(self._handlers))

    def dispatch(self, intent):
        """Раздаёт намерение обработчикам. True — его забрал плагин."""
        context = IntentContext(intent, {})
        for handler in list(self._handlers):
            if not handler.matches(context):
                continue
            try:
                result = handler.callback(context)
            except Exception as error:
                runtime.log("плагин " + handler.plugin_id + ": обработчик намерения упал: " + str(error))
                continue
            if context.consumed or result is True:
                return True
        return False


def dispatch_intent(intent):
    """Точка, из которой клиент отдаёт намерение плагинам."""
    return IntentsManager.get_instance().dispatch(intent)


def remove_plugin_handlers(plugin_id):
    IntentsManager.get_instance().remove_plugin_handlers(plugin_id)
