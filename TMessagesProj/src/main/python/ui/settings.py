"""Описание экрана настроек плагина.

Плагин возвращает список этих элементов из create_settings, а клиент рисует по ним экран.
Плагину не нужно знать ничего про интерфейс Android: он описывает, что показать, а как это
выглядит — забота клиента.

Набор полей и порядок аргументов повторяют то, к чему привыкли плагины: они
часто передают аргументы без имён, поэтому переставлять их местами нельзя.
"""


class _Item:
    """Общая часть всех строк: превращение в словарь для клиента."""

    kind = "item"

    #: поля, которые незачем гнать на ту сторону — обработчики зовём по номеру строки
    _SKIP = ("on_click", "on_change", "on_long_click", "create_sub_fragment",
             "create_view", "bind_view", "view", "attached_view_handler",
             "equals_handler", "content_equals_handler")

    def to_dict(self):
        data = {}
        for key, value in vars(self).items():
            if key.startswith("_") or key in self._SKIP:
                continue
            data[key] = value
        # клиенту важно знать, есть ли обработчики: от этого зависит, нажимается ли строка
        data["clickable"] = self.on_click is not None or self.create_sub_fragment is not None
        data["has_sub_fragment"] = self.create_sub_fragment is not None
        data["long_clickable"] = self.on_long_click is not None
        return data

    # значения по умолчанию: не у каждой строки эти поля есть
    on_click = None
    on_change = None
    on_long_click = None
    create_sub_fragment = None
    link_alias = None


class Header(_Item):
    """Заголовок раздела."""

    kind = "header"
    type = "header"

    def __init__(self, text=""):
        self.text = text


class Divider(_Item):
    """Разделитель; текст под ним — пояснение к разделу."""

    kind = "divider"
    type = "divider"

    def __init__(self, text=None):
        self.text = text


class Text(_Item):
    """Строка, по которой можно нажать.

    Если задан create_sub_fragment, нажатие открывает вложенный экран: плагин отдаёт для
    него такой же список строк. Так устроены плагины со своими многоуровневыми меню.
    """

    kind = "text"
    type = "text"

    def __init__(self, text="", subtext=None, icon=None, accent=False, red=False,
                 on_click=None, create_sub_fragment=None, on_long_click=None,
                 link_alias=None):
        self.text = text
        self.subtext = subtext
        self.icon = icon
        self.accent = accent
        self.red = red
        self.on_click = on_click
        self.create_sub_fragment = create_sub_fragment
        self.on_long_click = on_long_click
        self.link_alias = link_alias


class Switch(_Item):
    """Переключатель."""

    kind = "switch"
    type = "switch"

    def __init__(self, key="", text="", default=False, subtext=None, icon=None,
                 on_change=None, on_long_click=None, link_alias=None):
        self.key = key
        self.text = text
        self.default = default
        self.subtext = subtext
        self.icon = icon
        self.on_change = on_change
        self.on_long_click = on_long_click
        self.link_alias = link_alias


class Selector(_Item):
    """Выбор одного значения из списка."""

    kind = "selector"
    type = "selector"

    def __init__(self, key="", text="", default=0, items=None, icon=None,
                 on_change=None, on_long_click=None, link_alias=None):
        self.key = key
        self.text = text
        self.default = default
        self.items = list(items or [])
        self.icon = icon
        self.on_change = on_change
        self.on_long_click = on_long_click
        self.link_alias = link_alias


class Input(_Item):
    """Строка ввода."""

    kind = "input"
    type = "input"

    def __init__(self, key="", text="", default="", subtext=None, icon=None,
                 on_change=None, on_long_click=None, link_alias=None):
        self.key = key
        self.text = text
        self.default = default
        self.subtext = subtext
        self.icon = icon
        self.on_change = on_change
        self.on_long_click = on_long_click
        self.link_alias = link_alias


class EditText(_Item):
    """Поле ввода с подсказкой: для длинного текста и своих ограничений."""

    kind = "edittext"
    type = "edit_text"

    def __init__(self, key="", hint="", default="", multiline=False, max_length=256,
                 mask=None, on_change=None, text=None):
        self.key = key
        self.hint = hint
        self.text = text if text is not None else hint
        self.default = default
        self.multiline = multiline
        self.max_length = max_length
        self.mask = mask
        self.on_change = on_change


class Slider(_Item):
    """Ползунок для числа в заданных границах."""

    kind = "slider"
    type = "slider"

    def __init__(self, key="", text="", default=0, minimum=0, maximum=100, step=1,
                 subtext=None, on_change=None):
        self.key = key
        self.text = text
        self.default = default
        self.minimum = minimum
        self.maximum = maximum
        self.step = step
        self.subtext = subtext
        self.on_change = on_change


class Custom(_Item):
    """Строка, которую плагин рисует сам.

    create_view(context) отдаёт готовый Android-вид, bind_view(view) — обновляет его перед
    показом. Такие строки клиент вставляет в список как есть, поэтому плагин может нарисовать
    в настройках что угодно: график, картинку, свою кнопку.
    """

    kind = "custom"
    type = "custom"

    def __init__(self, create_view=None, bind_view=None, view=None, item=None, text=None,
                 subtext=None, divider=False, on_click=None, on_long_click=None,
                 link_alias=None):
        self.create_view = create_view
        self.bind_view = bind_view
        self.view = view
        self.item = item
        self.text = text
        self.subtext = subtext
        self.divider = divider
        self.on_click = on_click
        self.on_long_click = on_long_click
        self.link_alias = link_alias

        # плагин мог отдать готовый вид вместо способа его создать
        if self.view is not None and self.create_view is None:
            prebuilt = self.view
            self.create_view = lambda context, _view=prebuilt: _view


class SimpleSettingFactory:
    """Заготовка своей строки настроек, которую плагин создаёт много раз.

    Плагин описывает один раз, как строка рисуется и что делает по нажатию, а потом зовёт
    заготовку как обычную функцию — каждый вызов даёт свежую строку:

        row = SimpleSettingFactory(custom_name="server", create_view_fn=make_view)
        def create_settings(self):
            return [row("Германия"), row("Казахстан")]
    """

    def __init__(self, custom_name=None, create_view_fn=None, bind_view_fn=None,
                 attached_view_handler=None, equals_handler=None,
                 content_equals_handler=None, is_clickable=True, is_shadow=False,
                 on_click_handler=None, on_long_click_handler=None, link_alias=None):
        self.custom_name = custom_name
        self.create_view_fn = create_view_fn
        self.bind_view_fn = bind_view_fn
        self.attached_view_handler = attached_view_handler
        self.equals_handler = equals_handler
        self.content_equals_handler = content_equals_handler
        self.is_clickable = is_clickable
        self.is_shadow = is_shadow
        self.on_click_handler = on_click_handler
        self.on_long_click_handler = on_long_click_handler
        self.link_alias = link_alias

    def create(self, *args, **kwargs):
        """То же, что вызвать саму заготовку."""
        return self(*args, **kwargs)

    def __call__(self, *args, **kwargs):
        factory = self

        def create_view(context):
            if factory.create_view_fn is None:
                return None
            try:
                return factory.create_view_fn(context, *args, **kwargs)
            except TypeError:
                return factory.create_view_fn(context)

        def bind_view(view):
            if factory.bind_view_fn is None:
                return
            try:
                factory.bind_view_fn(view, *args, **kwargs)
            except TypeError:
                factory.bind_view_fn(view)

        row = Custom(
            create_view=create_view,
            bind_view=bind_view if factory.bind_view_fn is not None else None,
            on_click=factory.on_click_handler if factory.is_clickable else None,
            on_long_click=factory.on_long_click_handler,
            divider=not factory.is_shadow,
            link_alias=factory.link_alias,
        )
        row.custom_name = factory.custom_name
        row.attached_view_handler = factory.attached_view_handler
        row.equals_handler = factory.equals_handler
        row.content_equals_handler = factory.content_equals_handler
        row.is_clickable = factory.is_clickable
        row.is_shadow = factory.is_shadow
        return row
