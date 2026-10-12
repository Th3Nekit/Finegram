_OVERLOADS = "__fg_java_overloads__"

def joverload(java_name, arg_types=None):

    def mark(func):
        marks = list(getattr(func, _OVERLOADS, ()))
        marks.append((str(java_name), tuple(arg_types or ())))
        setattr(func, _OVERLOADS, tuple(marks))
        return func

    return mark

def joverride(java_name=None, arg_types=None):

    if _is_plain_function(java_name):

        func = java_name
        return joverload(func.__name__, _arg_slots(func))(func)

    if java_name is None:
        def mark(func):
            return joverload(func.__name__, arg_types or _arg_slots(func))(func)

        return mark

    return joverload(java_name, arg_types)

def _is_plain_function(value):

    import types
    return isinstance(value, (types.FunctionType, types.MethodType))

def _arg_slots(func):

    import inspect
    try:
        params = list(inspect.signature(func).parameters.values())
    except (TypeError, ValueError):
        return ()
    count = 0
    for param in params[1:]:
        if param.kind in (inspect.Parameter.VAR_POSITIONAL,
                          inspect.Parameter.VAR_KEYWORD):
            continue
        count += 1
    return ("?",) * count

class Base:

    _fg_superclass = None

    _fg_interfaces = ()

    _fg_proxy_class = None

    _fg_name = None

    @classmethod
    def bind(cls, superclass=None, *interfaces, **kwargs):

        chosen = [i for i in interfaces if i is not None]
        if not chosen and superclass is not None and _is_interface(superclass):
            chosen = [superclass]
        if not chosen:
            raise ValueError("bind: не указан ни один java-интерфейс")
        for interface in chosen:
            if not _is_interface(interface):
                raise NotImplementedError(
                    "наследовать обычный java-класс во время работы нельзя, "
                    "можно только реализовать интерфейс")
        cls._fg_interfaces = tuple(chosen)
        cls._fg_proxy_class = None
        cls._fg_name = kwargs.get("custom_name") or cls.__name__
        return cls

    @classmethod
    def new_instance(cls, init_args=None, **kwargs):

        if cls._fg_superclass is not None:
            return _new_view_instance(cls, init_args, kwargs)
        error = cls.__dict__.get("_fg_bind_error")
        if error is not None:
            raise NotImplementedError("%s: %s" % (cls.__name__, error))
        args = _as_args(init_args, kwargs)
        proxy_class = cls._fg_proxy_class
        if proxy_class is None:
            proxy_class = _build_proxy_class(cls)
            cls._fg_proxy_class = proxy_class
        return proxy_class(cls, args)

    new_java_instance = new_instance

    @classmethod
    def from_java(cls, obj):

        try:
            owner = obj.owner()
        except Exception:
            return obj
        return owner if owner is not None else obj

    def _fg_super(self, name, *args):

        view = self.__dict__.get("java")
        if view is None:
            raise AttributeError(name)
        return getattr(view, name)(*args)

    def onMeasure(self, widthMeasureSpec, heightMeasureSpec):
        return self._fg_super("superOnMeasure", widthMeasureSpec, heightMeasureSpec)

    def onLayout(self, changed, left, top, right, bottom):
        return self._fg_super("superOnLayout", changed, left, top, right, bottom)

    def onDraw(self, canvas):
        return self._fg_super("superOnDraw", canvas)

    def dispatchDraw(self, canvas):
        return self._fg_super("superDispatchDraw", canvas)

    def onTouchEvent(self, event):
        return self._fg_super("superOnTouchEvent", event)

    def onAttachedToWindow(self):
        return None

    def onDetachedFromWindow(self):
        return None

    def __getattr__(self, name):

        if name.startswith("_fg_") or name == "java":
            raise AttributeError(name)
        view = self.__dict__.get("java")
        if view is None:
            raise AttributeError(name)
        return getattr(view, name)

def _views():

    from java import jclass
    return jclass("com.th3nekit.finegram.plugins.FGPluginViews")

def _java_class_name(java_class):

    holder = getattr(java_class, "class_", None)
    if holder is not None:
        try:
            return holder.getName()
        except Exception:
            pass
    return getattr(java_class, "__name__", str(java_class))

def _as_args(init_args, kwargs):

    source = init_args if init_args is not None else kwargs.get("args")
    if source is None:
        return []
    if isinstance(source, (list, tuple)):
        return list(source)
    return [source]

def _new_view_instance(cls, init_args, kwargs):

    args = _as_args(init_args, kwargs)
    if not args:
        raise ValueError("%s: для вью нужен Context первым доводом" % cls.__name__)
    context = args[0]
    view = _views().create(cls._fg_superclass, context)
    if view is None:
        raise NotImplementedError(
            "%s: заготовленного потомка %s нет" % (cls.__name__, cls._fg_superclass))

    owner = cls.__new__(cls)

    owner.__dict__["java"] = view
    view.attachOwner(owner, sorted(_collect_overloads(cls).keys()))

    init = getattr(cls, "__init__", None)
    if init is not None and init is not object.__init__:
        try:
            init(owner, *args[1:])
        except TypeError:
            init(owner, *args)
    post = getattr(owner, "on_post_init", None)
    if post is not None:
        post(*args)
    return owner

def _is_interface(java_class):
    holder = getattr(java_class, "class_", None) or java_class
    try:
        return bool(holder.isInterface())
    except Exception:

        return True

def _collect_overloads(cls):

    found = {}
    for klass in reversed(cls.__mro__):
        for attr in vars(klass).values():
            marks = getattr(attr, _OVERLOADS, None)
            if not marks:
                continue
            for java_name, arg_types in marks:
                found.setdefault(java_name, []).append((len(arg_types), attr))
    return found

def _build_proxy_class(cls):

    from java import dynamic_proxy

    interfaces = cls._fg_interfaces
    if not interfaces:
        raise ValueError(
            "перед new_instance нужно вызвать bind и указать java-интерфейс")

    overloads = _collect_overloads(cls)
    if not overloads:
        raise ValueError(
            "в классе нет ни одного метода, помеченного joverload")

    body = {}
    for java_name, variants in overloads.items():
        body[java_name] = _make_dispatcher(java_name, variants)

    body["java"] = property(lambda self: self)

    proxy_class = type(cls._fg_name or (cls.__name__ + "Proxy"),
                       (dynamic_proxy(*interfaces),),
                       body)

    def __init__(self, owner_class, args):
        super(proxy_class, self).__init__()
        self._fg_owner = owner_class(*args)
        try:
            self._fg_owner.java = self
        except Exception:
            pass

    proxy_class.__init__ = __init__
    return proxy_class

def _make_dispatcher(java_name, variants):

    by_count = {}
    for count, func in variants:
        by_count.setdefault(count, func)
    single = variants[0][1] if len(variants) == 1 else None

    def call(self, *args):
        target = single or by_count.get(len(args))
        if target is None:

            target = variants[0][1]
        return target(self._fg_owner, *args)

    call.__name__ = java_name
    return call

def java_subclass(superclass=None, *interfaces, **kwargs):

    def decorator(cls):

        if superclass is not None and not _is_interface(superclass):
            name = _java_class_name(superclass)
            if _views().supports(name):
                cls._fg_superclass = name
                cls._fg_bind_error = None
                cls._fg_name = kwargs.get("custom_name") or cls.__name__
                return cls
            cls._fg_bind_error = NotImplementedError(
                "наследовать %s клиент не умеет: заготовленного потомка нет" % name)
            return cls
        try:
            cls.bind(superclass, *interfaces, **kwargs)
            cls._fg_bind_error = None
        except Exception as error:
            cls._fg_bind_error = error
        return cls

    return decorator

class jfield:

    def __init__(self, java_type=None, default=None, methods=None, **kwargs):
        self.java_type = java_type
        self.default = default
        self.methods = list(methods or ())
        self.name = None

    def __set_name__(self, owner, name):
        self.name = name
        for method in self.methods:
            if isinstance(method, jgetmethod):
                method.attach(owner, name)

    def __get__(self, instance, owner):
        if instance is None:
            return self
        return instance.__dict__.get(self.name, self.default)

    def __set__(self, instance, value):
        instance.__dict__[self.name] = value

class jgetmethod:

    def __init__(self, java_name):
        self.java_name = java_name

    def attach(self, owner, field_name):
        def getter(self):
            return getattr(self, field_name)

        joverload(self.java_name)(getter)
        if self.java_name not in owner.__dict__:
            setattr(owner, self.java_name, getter)
