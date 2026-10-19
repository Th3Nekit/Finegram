"""Всплывающие сообщения поверх интерфейса.

Короткая полоска внизу экрана: сообщить об успехе, об ошибке или предложить отменить
только что сделанное. Всё показывается в потоке интерфейса само, звать его отдельно не нужно.

Набор методов и порядок их аргументов повторяют привычный плагинам: они передают сюда и
номер картинки, и экран, на котором показать сообщение, и делают это без имён.
"""

from finegram import runtime


class BulletinHelper:
    """Готовые виды сообщений."""

    #: сколько сообщение висит на экране, миллисекунды
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
        """Короткую ошибку показываем полоской, длинную и техническую — окном.

        Длинный текст в полоске не помещается, а из окна его можно спокойно прочитать
        и скопировать — именно так плагины и показывают свои сбои.
        """
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
        """Две строки: заголовок и пояснение под ним."""
        runtime.show_bulletin_two_line(str(title), str(subtitle), "info", icon_res_id)

    @staticmethod
    def show_with_button(text, icon_res_id=0, button_text="", on_click=None,
                         fragment=None, duration=DURATION_PROLONG):
        """Сообщение с кнопкой справа."""
        runtime.show_bulletin_with_button(str(text), str(button_text), on_click,
                                          icon_res_id, duration)

    @staticmethod
    def show_undo(text, on_undo=None, on_action=None, subtitle=None, fragment=None):
        """Сообщение с кнопкой «Отменить»."""
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
    """Runnable из функции: так его зовут некоторые плагины."""
    from android_utils import make_runnable

    return make_runnable(func)
