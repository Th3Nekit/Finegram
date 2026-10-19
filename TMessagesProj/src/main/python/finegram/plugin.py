"""Базовый класс плагина Finegram.

Плагин — это один Python-файл с метаданными вверху и классом, унаследованным от
FinegramPlugin. Метаданные читаются до импорта, поэтому список установленных плагинов
можно показать, ничего не запуская.
"""


class FinegramPlugin:
    """Точка расширения клиента.

    Наследник переопределяет нужные методы; вызывать их напрямую не нужно —
    это делает рантайм.
    """

    #: заполняется рантаймом при загрузке
    id = ""
    name = ""
    version = ""
    author = ""
    description = ""

    def on_load(self):
        """Плагин включён: самое место подписаться на события и поставить хуки."""

    def on_unload(self):
        """Плагин выключают: снять хуки и освободить всё, что заняли."""

    def on_setting_changed(self, key, value):
        """Пользователь поменял настройку плагина."""

    # --- то, чем плагин пользуется ---

    def log(self, message):
        from finegram import runtime
        runtime.log(f"[{self.id}] {message}")

    def get_setting(self, key, default=None):
        from finegram import runtime
        return runtime.get_setting(self.id, key, default)

    def set_setting(self, key, value):
        from finegram import runtime
        runtime.set_setting(self.id, key, value)
