from finegram import runtime

def log(message):

    runtime.log(message)

def run_on_ui_thread(func, delay=0):

    runtime.run_on_ui_thread(func, delay)

def is_on_ui_thread():

    try:
        from android.os import Looper

        return Looper.myLooper() == Looper.getMainLooper()
    except Exception:
        return False

def copy_to_clipboard(text, message=None):

    runtime.copy_to_clipboard(text)

def get_application_context():

    return runtime.application_context()

def show_bulletin(text, kind="info"):

    runtime.show_bulletin(text, kind)

_click_class = None
_long_click_class = None

def _get_click_class():
    global _click_class
    if _click_class is not None:
        return _click_class

    from java import dynamic_proxy
    from android.view import View

    class _Listener(dynamic_proxy(View.OnClickListener)):
        def __init__(self, work):
            super().__init__()
            self._work = work

        def onClick(self, view):
            try:
                self._work(view)
            except TypeError:
                self._work()
            except Exception as error:
                log("обработчик нажатия упал: " + str(error))

    _click_class = _Listener
    return _click_class

def OnClickListener(func):

    return _get_click_class()(func)

def _get_long_click_class():
    global _long_click_class
    if _long_click_class is not None:
        return _long_click_class

    from java import dynamic_proxy
    from android.view import View

    class _Listener(dynamic_proxy(View.OnLongClickListener)):
        def __init__(self, work, consume):
            super().__init__()
            self._work = work
            self._consume = consume

        def onLongClick(self, view):
            try:
                result = self._work(view)
            except TypeError:
                result = self._work()
            except Exception as error:
                log("обработчик долгого нажатия упал: " + str(error))
                return False
            return self._consume if result is None else bool(result)

    _long_click_class = _Listener
    return _long_click_class

def OnLongClickListener(func, consume=True):

    return _get_long_click_class()(func, consume)

_task_class = None

def _get_task_class():

    global _task_class
    if _task_class is not None:
        return _task_class

    from java import dynamic_proxy
    from java.lang import Runnable

    class _Task(dynamic_proxy(Runnable)):
        def __init__(self, work):
            super().__init__()
            self._work = work

        def run(self):
            try:
                self._work()
            except Exception as error:
                log("код плагина упал: " + str(error))

    _task_class = _Task
    return _task_class

def make_runnable(func):

    return _get_task_class()(func)

R = make_runnable

def get_context():

    return get_application_context()

def set_clipboard_text(text):
    copy_to_clipboard(text)

def jclass(name):
    from java import jclass as _jclass

    return _jclass(name)

_listener_classes = {}

def _listener_class(key, build):
    cls = _listener_classes.get(key)
    if cls is None:
        cls = build()
        _listener_classes[key] = cls
    return cls

def _build_touch_class():
    from java import dynamic_proxy
    from android.view import View

    class _Listener(dynamic_proxy(View.OnTouchListener)):
        def __init__(self, work):
            super().__init__()
            self._work = work

        def onTouch(self, view, event):
            try:
                return bool(self._work(view, event))
            except Exception as error:
                log("обработчик касания упал: " + str(error))
                return False

    return _Listener

def OnTouchListener(func):

    return _listener_class("touch", _build_touch_class)(func)

def _build_key_class():
    from java import dynamic_proxy
    from android.view import View

    class _Listener(dynamic_proxy(View.OnKeyListener)):
        def __init__(self, work):
            super().__init__()
            self._work = work

        def onKey(self, view, key_code, event):
            try:
                return bool(self._work(view, key_code, event))
            except Exception as error:
                log("обработчик клавиш упал: " + str(error))
                return False

    return _Listener

def OnKeyListener(func):

    return _listener_class("key", _build_key_class)(func)

def _build_seek_class():
    from java import dynamic_proxy
    from android.widget import SeekBar

    class _Listener(dynamic_proxy(SeekBar.OnSeekBarChangeListener)):
        def __init__(self, on_progress, on_start, on_stop):
            super().__init__()
            self._on_progress = on_progress
            self._on_start = on_start
            self._on_stop = on_stop

        def onProgressChanged(self, seek_bar, progress, from_user):
            if self._on_progress is not None:
                try:
                    self._on_progress(seek_bar, progress, from_user)
                except Exception as error:
                    log("обработчик ползунка упал: " + str(error))

        def onStartTrackingTouch(self, seek_bar):
            if self._on_start is not None:
                try:
                    self._on_start(seek_bar)
                except Exception as error:
                    log("обработчик ползунка упал: " + str(error))

        def onStopTrackingTouch(self, seek_bar):
            if self._on_stop is not None:
                try:
                    self._on_stop(seek_bar)
                except Exception as error:
                    log("обработчик ползунка упал: " + str(error))

    return _Listener

def OnSeekBarChangeListener(on_progress=None, on_start=None, on_stop=None):

    return _listener_class("seek", _build_seek_class)(on_progress, on_start, on_stop)

def _build_callback_class():
    from java import dynamic_proxy, jclass as _jclass

    class _Callback(dynamic_proxy(_jclass("org.telegram.messenger.Utilities$Callback"))):
        def __init__(self, work):
            super().__init__()
            self._work = work

        def run(self, value):
            if self._work is None:
                return
            try:
                self._work(value)
            except Exception as error:
                log("обработчик плагина упал: " + str(error))

    return _Callback

def Callback(func=None):

    return _listener_class("callback", _build_callback_class)(func)
