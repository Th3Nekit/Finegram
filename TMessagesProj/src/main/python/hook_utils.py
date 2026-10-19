"""Для плагинов: доступ к внутренностям клиента."""

from finegram import runtime

try:
    from java import jclass
except ImportError:  # на компьютере Java нет — плагин всё равно не запустится
    jclass = None


def find_class(class_name):
    """Находит класс клиента по полному имени. Возвращает None, если класса нет."""
    return runtime.find_class(class_name)


def get_private_field(obj, field_name):
    """Читает закрытое поле объекта."""
    return runtime.get_private_field(obj, field_name)


def set_private_field(obj, field_name, value):
    """Записывает закрытое поле объекта."""
    runtime.set_private_field(obj, field_name, value)


def get_private_field_of_class(class_name, field_name):
    """Читает закрытое поле класса, не создавая объект."""
    target = runtime.find_class(class_name)
    return None if target is None else runtime.get_private_field(target, field_name)


def load_dex(data, key=None):
    """Загружает Java-код, который плагин принёс с собой в своём файле.

    Возвращает загрузчик классов; из него класс берут через `load_dex_class`.
    Пример: собранный dex лежит в конце `.plugin` в виде строк-комментариев, плагин
    достаёт их `unpack_payload`, распаковывает и передаёт сюда.
    """
    return runtime.load_dex(data, key)


def load_dex_class(loader, class_name):
    """Класс из dex, который принёс плагин."""
    return runtime.load_dex_class(loader, class_name)


def unpack_payload(path, begin_marker, end_marker):
    """Собирает вложение из хвоста файла плагина в одну строку (как оно там лежит)."""
    return runtime.unpack_payload(path, begin_marker, end_marker)


def get_static_private_field(class_name, field_name):
    """Читает закрытое поле класса: сам объект для этого не нужен."""
    target = class_name if not isinstance(class_name, str) else runtime.find_class(class_name)
    return None if target is None else runtime.get_private_field(target, field_name)


def set_static_private_field(class_name, field_name, value):
    """Записывает закрытое поле класса."""
    target = class_name if not isinstance(class_name, str) else runtime.find_class(class_name)
    if target is None:
        return False
    runtime.set_private_field(target, field_name, value)
    return True


def log(message):
    """Пишет в журнал: этот же вызов плагины берут и отсюда."""
    runtime.log(message)


def get_field(obj, field_name):
    """Читает поле объекта, в том числе закрытое."""
    return get_private_field(obj, field_name)


def set_field(obj, field_name, value):
    set_private_field(obj, field_name, value)


def jarray(element_type):
    from java import jarray as _jarray

    return _jarray(element_type)
