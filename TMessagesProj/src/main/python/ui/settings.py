class _Item:

    kind = "item"

    _SKIP = ("on_click", "on_change", "on_long_click", "create_sub_fragment",
             "create_view", "bind_view", "view", "attached_view_handler",
             "equals_handler", "content_equals_handler")

    def to_dict(self):
        data = {}
        for key, value in vars(self).items():
            if key.startswith("_") or key in self._SKIP:
                continue
            data[key] = value

        data["clickable"] = self.on_click is not None or self.create_sub_fragment is not None
        data["has_sub_fragment"] = self.create_sub_fragment is not None
        data["long_clickable"] = self.on_long_click is not None
        return data

    on_click = None
    on_change = None
    on_long_click = None
    create_sub_fragment = None
    link_alias = None

class Header(_Item):

    kind = "header"
    type = "header"

    def __init__(self, text=""):
        self.text = text

class Divider(_Item):

    kind = "divider"
    type = "divider"

    def __init__(self, text=None):
        self.text = text

class Text(_Item):

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

        if self.view is not None and self.create_view is None:
            prebuilt = self.view
            self.create_view = lambda context, _view=prebuilt: _view

class SimpleSettingFactory:

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
