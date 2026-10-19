"""Переходник: плагины и SDK exteraGram читают шапку файла через этот модуль.

Своей разборки здесь нет — зовём ту, что уже есть в рантайме. Модуль нужен ровно затем,
чтобы импорт по привычному имени не падал ещё до первой строки плагина.
"""

import finegram.runtime as _runtime


def get_metadata(path):
    """Метаданные плагина по пути к файлу: id, имя, версия, автор и прочее из шапки."""
    try:
        return _runtime.read_metadata(path) or {}
    except Exception:
        return {}


def parse_metadata(path):
    """То же самое под вторым именем, встречающимся в старых плагинах."""
    return get_metadata(path)
