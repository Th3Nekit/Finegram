from finegram import runtime

_button_listener_cls = None
_item_listener_cls = None

def _button_listener(callback):

    global _button_listener_cls
    if _button_listener_cls is None:
        from java import dynamic_proxy
        from org.telegram.ui.ActionBar import AlertDialog

        class _ButtonListener(dynamic_proxy(AlertDialog.OnButtonClickListener)):
            def __init__(self, handler):
                super().__init__()
                self._handler = handler

            def onClick(self, dialog, which):
                try:
                    self._handler(dialog, which)
                except Exception as error:
                    runtime.log("обработчик кнопки упал: " + str(error))

        _button_listener_cls = _ButtonListener
    return _button_listener_cls(callback)

def _item_listener(callback):

    global _item_listener_cls
    if _item_listener_cls is None:
        from java import dynamic_proxy
        from android.content import DialogInterface

        class _ItemListener(dynamic_proxy(DialogInterface.OnClickListener)):
            def __init__(self, handler):
                super().__init__()
                self._handler = handler

            def onClick(self, dialog, which):
                try:
                    self._handler(dialog, which)
                except Exception as error:
                    runtime.log("обработчик списка упал: " + str(error))

        _item_listener_cls = _ItemListener
    return _item_listener_cls(callback)

class AlertDialogBuilder:

    BUTTON_POSITIVE = -1
    BUTTON_NEGATIVE = -2
    BUTTON_NEUTRAL = -3

    ALERT_TYPE_MESSAGE = 0
    ALERT_TYPE_LOADING = 1
    ALERT_TYPE_SPINNER = 3

    def __init__(self, context=None, progress_style=0, resources_provider=None):
        self._builder = None
        self._dialog = None
        self._context = None
        try:
            from org.telegram.ui.ActionBar import AlertDialog

            if context is None:
                fragment = runtime.get_last_fragment()
                context = fragment.getParentActivity() if fragment is not None else None
            self._context = context
            if context is not None:
                if resources_provider is not None:
                    self._builder = AlertDialog.Builder(context, int(progress_style or 0), resources_provider)
                elif progress_style:
                    self._builder = AlertDialog.Builder(context, int(progress_style))
                else:
                    self._builder = AlertDialog.Builder(context)
        except Exception as error:
            runtime.log("окно не создалось: " + str(error))

    def get_context(self):

        return self._context

    getContext = get_context

    def set_title(self, title):
        if self._builder is not None:
            self._builder.setTitle(title)
        return self

    def set_message(self, message):
        if self._builder is not None:
            self._builder.setMessage(message)
        return self

    def set_view(self, view):
        if self._builder is not None:
            self._builder.setView(view)
        return self

    def set_message_text_view_clickable(self, value):
        if self._builder is not None:
            self._builder.setMessageTextViewClickable(bool(value))
        return self

    def set_items(self, items, listener=None):
        if self._builder is not None:
            values = [str(item) for item in (items or [])]
            array = _string_array(values)
            self._builder.setItems(array, None if listener is None else _item_listener(listener))
        return self

    def set_cancelable(self, value):
        try:
            if self._dialog is not None:
                self._dialog.setCancelable(bool(value))
        except Exception:
            pass
        return self

    def set_canceled_on_touch_outside(self, value):

        try:
            if self._dialog is not None:
                self._dialog.setCanceledOnTouchOutside(bool(value))
        except Exception:
            pass
        return self

    def set_dim_enabled(self, value):

        if self._builder is not None:
            try:
                self._builder.setDimEnabled(bool(value))
            except Exception:
                pass
        return self

    def set_top_image(self, drawable, background_color=0):

        if self._builder is not None:
            try:
                self._builder.setTopImage(drawable, int(background_color))
            except Exception:
                pass
        return self

    set_top_drawable = set_top_image

    def set_top_animation(self, animation, size=0, use_theme_color=False, background_color=0):

        if self._builder is not None:
            try:
                self._builder.setTopAnimation(int(animation), int(size or 0),
                                              bool(use_theme_color), int(background_color))
            except Exception:
                pass
        return self

    def set_on_dismiss_listener(self, listener):

        if self._builder is not None and listener is not None:
            try:
                self._builder.setOnDismissListener(_dismiss_listener(listener))
            except Exception:
                pass
        return self

    def set_on_cancel_listener(self, listener):

        if self._builder is not None and listener is not None:
            try:
                self._builder.setOnCancelListener(_cancel_listener(listener))
            except Exception:
                pass
        return self

    def make_button_red(self, which=BUTTON_POSITIVE):

        try:
            from org.telegram.ui.ActionBar import Theme
            from org.telegram.messenger import AndroidUtilities

            button = self.get_button(which)
            if button is not None:
                button.setTextColor(Theme.getColor(Theme.key_text_RedBold))
                button.setBackground(Theme.createRadSelectorDrawable(
                    Theme.multAlpha(Theme.getColor(Theme.key_text_RedBold), 0.12),
                    AndroidUtilities.dp(6), AndroidUtilities.dp(6)))
        except Exception:
            pass
        return self

    def set_progress(self, value):

        try:
            if self._dialog is not None:
                self._dialog.setProgress(int(value))
        except Exception:
            pass
        return self

    def is_showing(self):
        try:
            return self._dialog is not None and self._dialog.isShowing()
        except Exception:
            return False

    isShowing = is_showing

    def cancel(self):

        try:
            if self._dialog is not None:
                self._dialog.cancel()
        except Exception:
            pass
        return self

    def set_positive_button(self, text, listener=None):
        if self._builder is not None:
            self._builder.setPositiveButton(text, None if listener is None else _button_listener(listener))
        return self

    def set_negative_button(self, text, listener=None):
        if self._builder is not None:
            self._builder.setNegativeButton(text, None if listener is None else _button_listener(listener))
        return self

    def set_neutral_button(self, text, listener=None):
        if self._builder is not None:
            self._builder.setNeutralButton(text, None if listener is None else _button_listener(listener))
        return self

    def create(self):
        if self._builder is not None and self._dialog is None:
            self._dialog = self._builder.create()
        return self

    def show(self):

        try:
            from android.os import Looper

            on_ui = Looper.myLooper() == Looper.getMainLooper()
        except Exception:
            on_ui = True
        if not on_ui:
            runtime.run_on_ui_thread(self._show_now, 0)
            return self
        self._show_now()
        return self

    def _show_now(self):
        try:
            self.create()
            if self._dialog is None:
                return
            fragment = runtime.get_last_fragment()
            if fragment is not None:
                fragment.showDialog(self._dialog)
            else:
                self._dialog.show()
        except Exception as error:
            runtime.log("окно не показалось: " + str(error))

    def dismiss(self):
        try:
            if self._dialog is not None:
                self._dialog.dismiss()
        except Exception:
            pass
        return self

    def get_dialog(self):
        return self._dialog

    def get_button(self, which):
        try:
            return None if self._dialog is None else self._dialog.getButton(which)
        except Exception:
            return None

def _string_array(values):

    from java import jarray
    from java.lang import CharSequence

    return jarray(CharSequence)(values)

_dismiss_listener_cls = None
_cancel_listener_cls = None

def _dismiss_listener(handler):

    global _dismiss_listener_cls
    if _dismiss_listener_cls is None:
        from java import dynamic_proxy
        from android.content import DialogInterface

        class _Listener(dynamic_proxy(DialogInterface.OnDismissListener)):
            def __init__(self, work):
                super().__init__()
                self._work = work

            def onDismiss(self, dialog):
                try:
                    self._work(dialog)
                except TypeError:
                    self._work()
                except Exception as error:
                    runtime.log("обработчик закрытия окна упал: " + str(error))

        _dismiss_listener_cls = _Listener
    return _dismiss_listener_cls(handler)

def _cancel_listener(handler):

    global _cancel_listener_cls
    if _cancel_listener_cls is None:
        from java import dynamic_proxy
        from android.content import DialogInterface

        class _Listener(dynamic_proxy(DialogInterface.OnCancelListener)):
            def __init__(self, work):
                super().__init__()
                self._work = work

            def onCancel(self, dialog):
                try:
                    self._work(dialog)
                except TypeError:
                    self._work()
                except Exception as error:
                    runtime.log("обработчик отмены окна упал: " + str(error))

        _cancel_listener_cls = _Listener
    return _cancel_listener_cls(handler)
