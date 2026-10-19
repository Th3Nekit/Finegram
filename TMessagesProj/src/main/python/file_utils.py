"""Для плагинов: файлы и каталоги для плагинов.

Плагину не нужно знать, где именно клиент держит свои файлы: он спрашивает каталог по
назначению и работает с ним. Всё, что здесь есть, не бросает исключений — при ошибке
возвращается None или False, чтобы плагин не падал на мелочи.
"""

import os
import shutil

from finegram import runtime


def _context():
    return runtime.application_context()


def _dir(getter, fallback):
    try:
        context = _context()
        if context is None:
            return fallback
        path = getter(context)
        return str(path.getAbsolutePath()) if path is not None else fallback
    except Exception:
        return fallback


def get_files_dir():
    """Личный каталог приложения."""
    return _dir(lambda c: c.getFilesDir(), "")


def get_cache_dir():
    """Каталог для временного."""
    return _dir(lambda c: c.getCacheDir(), "")


def get_plugins_dir():
    """Каталог, где лежат установленные плагины."""
    root = get_files_dir()
    return os.path.join(root, "plugins") if root else ""


def get_plugin_data_dir(plugin_id):
    """Отдельный каталог под данные конкретного плагина; создаётся при первом обращении."""
    root = get_files_dir()
    if not root or not plugin_id:
        return ""
    path = os.path.join(root, "plugin-data", str(plugin_id))
    ensure_dir_exists(path)
    return path


def _media_dir(name):
    try:
        from org.telegram.messenger import FileLoader

        directory = FileLoader.checkDirectory(name)
        return str(directory.getAbsolutePath()) if directory is not None else ""
    except Exception:
        return ""


def get_images_dir():
    from org.telegram.messenger import FileLoader

    return _media_dir(FileLoader.MEDIA_DIR_IMAGE)


def get_videos_dir():
    from org.telegram.messenger import FileLoader

    return _media_dir(FileLoader.MEDIA_DIR_VIDEO)


def get_audios_dir():
    from org.telegram.messenger import FileLoader

    return _media_dir(FileLoader.MEDIA_DIR_AUDIO)


def get_documents_dir():
    from org.telegram.messenger import FileLoader

    return _media_dir(FileLoader.MEDIA_DIR_DOCUMENT)


# --- чтение и запись ---

def read_file(path, encoding="utf-8"):
    """Текст файла или None, если прочитать не вышло."""
    try:
        with open(path, "r", encoding=encoding, errors="replace") as handle:
            return handle.read()
    except OSError:
        return None


def write_file(path, content, encoding="utf-8"):
    try:
        ensure_dir_exists(os.path.dirname(path))
        with open(path, "w", encoding=encoding, newline="") as handle:
            handle.write(content)
        return True
    except OSError as error:
        runtime.log("файл не записан: " + str(error))
        return False


def read_file_bytes(path):
    try:
        with open(path, "rb") as handle:
            return handle.read()
    except OSError:
        return None


def write_file_bytes(path, content):
    try:
        ensure_dir_exists(os.path.dirname(path))
        with open(path, "wb") as handle:
            handle.write(content)
        return True
    except OSError as error:
        runtime.log("файл не записан: " + str(error))
        return False


def delete_file(path):
    try:
        if os.path.isdir(path):
            shutil.rmtree(path)
        elif os.path.exists(path):
            os.remove(path)
        return True
    except OSError:
        return False


def ensure_dir_exists(path):
    if not path:
        return False
    try:
        os.makedirs(path, exist_ok=True)
        return True
    except OSError:
        return False


def list_dir(path, recursive=False, include_files=True, include_dirs=False, extensions=None):
    """Содержимое каталога с простыми фильтрами."""
    result = []
    if not path or not os.path.isdir(path):
        return result

    allowed = None
    if extensions:
        allowed = {("." + e.lstrip(".")).lower() for e in extensions}

    def matches(name):
        return allowed is None or os.path.splitext(name)[1].lower() in allowed

    try:
        if recursive:
            for root, dirs, files in os.walk(path):
                if include_dirs:
                    result.extend(os.path.join(root, d) for d in dirs)
                if include_files:
                    result.extend(os.path.join(root, f) for f in files if matches(f))
        else:
            for name in os.listdir(path):
                full = os.path.join(path, name)
                if os.path.isdir(full):
                    if include_dirs:
                        result.append(full)
                elif include_files and matches(name):
                    result.append(full)
    except OSError:
        return result
    return sorted(result)


class FileInfo:
    """Сведения о файле в удобном виде."""

    def __init__(self, path):
        self.path = path
        self.name = os.path.basename(path)
        self.extension = os.path.splitext(path)[1].lstrip(".").lower()
        self.exists = os.path.exists(path)
        self.size = os.path.getsize(path) if self.exists and os.path.isfile(path) else 0
        self.is_dir = os.path.isdir(path)

    def read(self):
        return read_file(self.path)

    def read_bytes(self):
        return read_file_bytes(self.path)

    def __repr__(self):
        return "FileInfo(%r, %d байт)" % (self.name, self.size)


class ExtensionAlreadyRegistered(Exception):
    """Такое расширение уже забрал другой плагин."""


class ExtensionNotRegistered(Exception):
    """Расширение никем не занято."""


class Place:
    """Где показывать действие плагина для его файлов."""

    DEFAULT = "default"
    DOCUMENT = "document"
    SHARED_FILES = "shared_files"


class FilesController:
    """Файлы, которые взял на себя плагин.

    Настоящая привязка живёт в base_plugin.add_file_hook — здесь только знакомые плагинам
    имена, чтобы их импорт не падал на пустом месте.
    """

    Place = Place

    @staticmethod
    def get_instance():
        return FilesController()
