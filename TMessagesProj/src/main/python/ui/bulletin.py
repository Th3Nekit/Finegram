from finegram import runtime

class BulletinHelper:

    DURATION_SHORT = 1500
    DURATION_LONG = 2750
    DURATION_PROLONG = 5000

    @staticmethod
    def show_info(message, fragment=None):
        runtime.show_bulletin(str(message), "info")

    @staticmethod
    def show_success(message, fragment=None):
        runtime.show_bulletin(str(message), "success")

    @staticmethod
    def show_error(message, fragment=None):

        text = "" if message is None else str(message)
        low = text.lower()
        technical = (
            len(text) > 120
            or "exception" in low
            or "landroid" in low
            or "ljava" in low
            or "java." in low
            or "nosuchmethod" in low
            or "<init>" in low
            or "traceback" in low
        )
        if technical:
            runtime.show_error_dialog(text)
            return
        runtime.show_bulletin(text, "error")

    @staticmethod
    def show_simple(text, icon_res_id=0, fragment=None):
        runtime.show_bulletin(str(text), "info", icon_res_id)

    @staticmethod
    def show_two_line(title, subtitle, icon_res_id=0, fragment=None):

        runtime.show_bulletin_two_line(str(title), str(subtitle), "info", icon_res_id)

    @staticmethod
    def show_with_button(text, icon_res_id=0, button_text="", on_click=None,
                         fragment=None, duration=DURATION_PROLONG):

        runtime.show_bulletin_with_button(str(text), str(button_text), on_click,
                                          icon_res_id, duration)

    @staticmethod
    def show_undo(text, on_undo=None, on_action=None, subtitle=None, fragment=None):

        runtime.show_bulletin_undo(str(text), on_undo, subtitle)

    @staticmethod
    def show_copied_to_clipboard(message=None, fragment=None):
        runtime.show_bulletin(str(message) if message else "Скопировано", "copy")

    @staticmethod
    def show_link_copied(is_private_link_info=False, fragment=None):
        runtime.show_bulletin("Ссылка скопирована", "copy")

    @staticmethod
    def show_file_saved_to_gallery(is_video=False, amount=1, fragment=None):
        runtime.show_bulletin("Сохранено в галерею", "success")

    @staticmethod
    def show_file_saved_to_downloads(file_type_enum_name="UNKNOWN", amount=1, fragment=None):
        runtime.show_bulletin("Сохранено в загрузки", "success")

def R(func):

    from android_utils import make_runnable

    return make_runnable(func)
