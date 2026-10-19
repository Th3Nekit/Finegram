"""Для плагинов: реализация Java-интерфейсов из кода плагина.

Иногда плагину нужно отдать клиенту не значение, а поведение: слушателя нажатий, приёмник
событий списка. В Java это описано интерфейсом, и подсунуть туда обычный объект Python
нельзя — нужен посредник, который выглядит как реализация этого интерфейса, а вызовы
переправляет в код плагина.

Плагин описывает такой посредник обычным классом:

    class Listener(Base):
        def __init__(self, plugin):
            self.plugin = plugin

        @joverload("onItemClick", ["android.view.View", "int"])
        def on_item_click(self, view, position):
            ...

    Listener.bind(JavaObject, SomeListenerInterface)
    listener = Listener.new_instance(init_args=[plugin])

Имя метода в Java и имя метода в плагине не обязаны совпадать — связь задаёт joverload.
Перечень типов аргументов там же указывают для наглядности; выбор нужного метода среди
одноимённых идёт по числу аргументов, потому что к моменту вызова типы уже известны
самому Java.
"""

_OVERLOADS = "__fg_java_overloads__"


def joverload(java_name, arg_types=None):
    """Помечает метод как реализацию java-метода с таким именем."""

    def mark(func):
        marks = list(getattr(func, _OVERLOADS, ()))
        marks.append((str(java_name), tuple(arg_types or ())))
        setattr(func, _OVERLOADS, tuple(marks))
        return func

    return mark


def joverride(java_name=None, arg_types=None):
    """То же, что joverload, но имя java-метода можно не называть.

    Плагины пишут просто ``@joverride()`` над методом: имя в Java совпадает с именем
    функции, а число аргументов видно по её сигнатуре. Форму с явным именем тоже
    принимаем — так пишут, когда имена не совпадают.
    """
    if _is_plain_function(java_name):
        # @joverride без скобок
        func = java_name
        return joverload(func.__name__, _arg_slots(func))(func)

    if java_name is None:
        def mark(func):
            return joverload(func.__name__, arg_types or _arg_slots(func))(func)

        return mark

    return joverload(java_name, arg_types)


def _is_plain_function(value):
    """Правда, если декоратор применили без скобок и нам отдали саму функцию."""
    import types
    return isinstance(value, (types.FunctionType, types.MethodType))


def _arg_slots(func):
    """Заглушки под аргументы метода — нужны только чтобы знать их число.

    Типы здесь не выясняем: к моменту вызова из Java они уже разобраны. А вот число
    важно, иначе два одноимённых метода с разным числом аргументов перепутаются.
    """
    import inspect
    try:
        params = list(inspect.signature(func).parameters.values())
    except (TypeError, ValueError):
        return ()
    count = 0
    for param in params[1:]:  # первый — self
        if param.kind in (inspect.Parameter.VAR_POSITIONAL,
                          inspect.Parameter.VAR_KEYWORD):
            continue
        count += 1
    return ("?",) * count


class Base:
    """Родитель для классов, которые реализуют java-интерфейс или наследуют вью."""

    #: java-класс, от которого плагин хотел унаследоваться
    _fg_superclass = None
    #: интерфейсы, заданные через bind
    _fg_interfaces = ()
    #: готовый класс-посредник; создаётся один раз на класс плагина
    _fg_proxy_class = None
    #: имя, под которым посредник показывается в журналах
    _fg_name = None

    @classmethod
    def bind(cls, superclass=None, *interfaces, **kwargs):
        """Говорит, какой интерфейс этот класс реализует.

        Первым идёт родительский класс — его принимаем ради совместимости и не используем:
        подменить настоящий Java-класс во время работы нельзя, можно только реализовать
        интерфейс. Имя посредника тоже принимаем и запоминаем для журнала.
        """
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
        """Создаёт посредника и отдаёт его — таким, каким его примет Java."""
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

    #: то же под именем, к которому привыкли плагины
    new_java_instance = new_instance

    @classmethod
    def from_java(cls, obj):
        """Объект плагина, стоящий за этой вью. Если его нет — сама вью."""
        try:
            owner = obj.owner()
        except Exception:
            return obj
        return owner if owner is not None else obj

    # --- вызовы родителя: super().onMeasure(...) и прочие ---

    def _fg_super(self, name, *args):
        """Зовёт настоящий метод родителя у подставной вью."""
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
        """Всё, чего нет у плагина, спрашиваем у самой вью.

        Благодаря этому плагин пишет self.addView(...) и self.invalidate() так, будто
        он и есть вью, — а на деле он лишь стоит за ней.
        """
        if name.startswith("_fg_") or name == "java":
            raise AttributeError(name)
        view = self.__dict__.get("java")
        if view is None:
            raise AttributeError(name)
        return getattr(view, name)


def _views():
    """Класс клиента с заготовленными потомками вью."""
    from java import jclass
    return jclass("com.th3nekit.finegram.plugins.FGPluginViews")


def _java_class_name(java_class):
    """Полное имя java-класса: по нему клиент и подбирает потомка."""
    holder = getattr(java_class, "class_", None)
    if holder is not None:
        try:
            return holder.getName()
        except Exception:
            pass
    return getattr(java_class, "__name__", str(java_class))


def _as_args(init_args, kwargs):
    """Доводы конструктора: их передают и списком, и по одному.

    Плагины пишут обе формы: new_instance([a, b]) и new_instance(context). Вторую нельзя
    перебирать — это один объект, а не набор.
    """
    source = init_args if init_args is not None else kwargs.get("args")
    if source is None:
        return []
    if isinstance(source, (list, tuple)):
        return list(source)
    return [source]


def _new_view_instance(cls, init_args, kwargs):
    """Создаёт подставную вью и ставит за ней объект плагина.

    Первым доводом идёт контекст: без него вью не построить. Остальное уходит в
    on_post_init — у плагина это замена конструктора, ведь саму вью создаём мы.
    """
    args = _as_args(init_args, kwargs)
    if not args:
        raise ValueError("%s: для вью нужен Context первым доводом" % cls.__name__)
    context = args[0]
    view = _views().create(cls._fg_superclass, context)
    if view is None:
        raise NotImplementedError(
            "%s: заготовленного потомка %s нет" % (cls.__name__, cls._fg_superclass))

    owner = cls.__new__(cls)
    # Кладём напрямую в словарь: обычное присваивание ушло бы в __getattr__.
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
        # У самого объекта класса спросить не получилось — считаем, что интерфейс:
        # ошибётся не здесь, а при создании посредника, и с понятным текстом.
        return True


def _collect_overloads(cls):
    """Собирает по всей цепочке наследования: «имя java-метода» -> методы плагина."""
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
    """Собирает класс-посредник под интерфейсы, заданные в bind.

    Класс создаётся один раз на класс плагина и потом переиспользуется: посредник живёт
    под своим именем в Java, и второе создание с тем же именем не проходит — плагин
    оказывается с нерабочим слушателем, хотя ошибки не видно.
    """
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
    # new_instance().java у плагинов — сам java-объект; посредник им и является
    body["java"] = property(lambda self: self)

    proxy_class = type(cls._fg_name or (cls.__name__ + "Proxy"),
                       (dynamic_proxy(*interfaces),),
                       body)

    # Конструктор задаём после создания класса: внутри него нужно позвать конструктор
    # посредника, а сослаться на него по имени можно только когда класс уже есть.
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
    """Делает метод посредника: принимает вызов из Java и зовёт метод плагина."""
    # По числу аргументов: одноимённые методы интерфейса различаются именно им, а типы
    # к моменту вызова уже разобраны самим Java.
    by_count = {}
    for count, func in variants:
        by_count.setdefault(count, func)
    single = variants[0][1] if len(variants) == 1 else None

    def call(self, *args):
        target = single or by_count.get(len(args))
        if target is None:
            # Ни одна из помеченных сигнатур не подошла — берём любую, чтобы вызов не
            # пропал молча: у Java на той стороне уже идёт жест или анимация.
            target = variants[0][1]
        return target(self._fg_owner, *args)

    call.__name__ = java_name
    return call


def java_subclass(superclass=None, *interfaces, **kwargs):
    """Декоратор: класс плагина становится реализацией java-интерфейса.

    Настоящие java-классы (не интерфейсы) во время работы у нас не наследуются. Такой класс
    всё равно объявляется без ошибки, чтобы плагин загрузился, а понятная ошибка возникает
    только при попытке создать экземпляр.
    """

    def decorator(cls):
        # Настоящий класс: берём заготовленного потомка, если он у клиента есть.
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
    """Поле java-класса, объявленное в классе плагина.

    Хранится у объекта плагина как обычный атрибут; значение по умолчанию — default.
    """

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
    """Геттер для jfield: под этим именем Java получает значение поля."""

    def __init__(self, java_name):
        self.java_name = java_name

    def attach(self, owner, field_name):
        def getter(self):
            return getattr(self, field_name)

        joverload(self.java_name)(getter)
        if self.java_name not in owner.__dict__:
            setattr(owner, self.java_name, getter)
