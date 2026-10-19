"""Для плагинов: разметка текста.

Плагину удобно писать `**жирным**`, а серверу нужны отдельно текст и список оформлений
с отступами. Здесь одно превращается в другое.

Отступы считаются в кодовых единицах UTF-16 — так их считает Telegram. Для обычных букв
это то же самое, что и в Python, а вот эмодзи занимает две единицы, и без пересчёта
оформление на длинных сообщениях с эмодзи уезжает.
"""

import re


class TLEntityType:
    """Виды оформления текста.

    Имена те же, что и у соседей: плагины перечисляют их напрямую, а `STRIKE` и `LINK`
    остались как привычные нам синонимы.
    """

    BOLD = "bold"
    ITALIC = "italic"
    UNDERLINE = "underline"
    STRIKETHROUGH = "strikethrough"
    SPOILER = "spoiler"
    CODE = "code"
    PRE = "pre"
    TEXT_LINK = "text_link"
    CUSTOM_EMOJI = "custom_emoji"
    QUOTE = "quote"

    #: как это называлось в наших ранних сборках
    STRIKE = STRIKETHROUGH
    LINK = TEXT_LINK


class RawEntity:
    """Одно оформление: где начинается, сколько занимает и что это."""

    def __init__(self, entity_type, offset, length, language=None, url=None, document_id=None):
        self.type = entity_type
        self.offset = offset
        self.length = length
        self.language = language
        self.url = url
        self.document_id = document_id

    def to_tl(self):
        """Собирает объект оформления в том виде, в каком его ждёт сервер."""
        from org.telegram.tgnet import TLRPC

        builders = {
            TLEntityType.BOLD: TLRPC.TL_messageEntityBold,
            TLEntityType.ITALIC: TLRPC.TL_messageEntityItalic,
            TLEntityType.UNDERLINE: TLRPC.TL_messageEntityUnderline,
            TLEntityType.STRIKETHROUGH: TLRPC.TL_messageEntityStrike,
            TLEntityType.SPOILER: TLRPC.TL_messageEntitySpoiler,
            TLEntityType.CODE: TLRPC.TL_messageEntityCode,
            TLEntityType.QUOTE: TLRPC.TL_messageEntityBlockquote,
        }
        if self.type == TLEntityType.TEXT_LINK:
            entity = TLRPC.TL_messageEntityTextUrl()
            entity.url = self.url or ""
        elif self.type == TLEntityType.PRE:
            entity = TLRPC.TL_messageEntityPre()
            entity.language = self.language or ""
        elif self.type == TLEntityType.CUSTOM_EMOJI:
            entity = TLRPC.TL_messageEntityCustomEmoji()
            entity.document_id = int(self.document_id or 0)
        else:
            builder = builders.get(self.type)
            if builder is None:
                return None
            entity = builder()
        entity.offset = self.offset
        entity.length = self.length
        return entity

    def to_tlrpc_object(self):
        """То же, что to_tl: под этим именем метод зовут чужие плагины."""
        return self.to_tl()

    def __repr__(self):
        return "RawEntity(%s, %d, %d)" % (self.type, self.offset, self.length)


class ParsedMessage:
    """Текст без разметки и список оформлений к нему."""

    def __init__(self, text, entities):
        self.text = text
        self.entities = entities

    def to_tl_entities(self):
        """Список оформлений в виде объектов клиента; годится для отправки."""
        from java.util import ArrayList

        result = ArrayList()
        for entity in self.entities:
            built = entity.to_tl()
            if built is not None:
                result.add(built)
        return result

    def __repr__(self):
        return "ParsedMessage(%r, %d оформлений)" % (self.text, len(self.entities))


def to_utf16_len(text):
    """Длина строки в кодовых единицах UTF-16 — так её считает Telegram."""
    return len(text.encode("utf-16-le")) // 2


def get_utf16_code_unit_offset(text, index):
    """Отступ в единицах UTF-16 до символа с таким номером."""
    return to_utf16_len(text[:index])


# порядок важен: тройные и двойные метки разбираются раньше одиночных
_RULES = [
    (TLEntityType.PRE, re.compile(r"```(\w*)\n?(.*?)```", re.DOTALL)),
    (TLEntityType.CODE, re.compile(r"`([^`\n]+)`")),
    (TLEntityType.BOLD, re.compile(r"\*\*(.+?)\*\*", re.DOTALL)),
    (TLEntityType.ITALIC, re.compile(r"__(.+?)__", re.DOTALL)),
    (TLEntityType.STRIKE, re.compile(r"~~(.+?)~~", re.DOTALL)),
    (TLEntityType.SPOILER, re.compile(r"\|\|(.+?)\|\|", re.DOTALL)),
    (TLEntityType.UNDERLINE, re.compile(r"--(.+?)--", re.DOTALL)),
    (TLEntityType.LINK, re.compile(r"\[(.+?)\]\((.+?)\)", re.DOTALL)),
]


def parse_markdown(text):
    """Разбирает разметку и возвращает чистый текст с оформлениями.

    Понимает `**жирный**`, `__курсив__`, `~~зачёркнутый~~`, `||спойлер||`, `--подчёркнутый--`,
    `` `код` ``, ```` ```блок``` ```` и `[ссылка](адрес)`.
    """
    if not text:
        return ParsedMessage("", [])

    result = str(text)
    entities = []

    changed = True
    while changed:
        changed = False
        for entity_type, pattern in _RULES:
            match = pattern.search(result)
            if match is None:
                continue

            if entity_type == TLEntityType.LINK:
                inner, extra = match.group(1), match.group(2)
            elif entity_type == TLEntityType.PRE:
                extra, inner = match.group(1), match.group(2)
            else:
                inner, extra = match.group(1), None

            start = match.start()
            result = result[:start] + inner + result[match.end():]

            offset = get_utf16_code_unit_offset(result, start)
            length = to_utf16_len(inner)
            entities.append(RawEntity(
                entity_type, offset, length,
                url=extra if entity_type == TLEntityType.LINK else None,
                language=extra if entity_type == TLEntityType.PRE else None,
            ))

            # оформления, стоявшие правее, сдвинулись на длину снятых меток
            shift = to_utf16_len(match.group(0)) - length
            for existing in entities[:-1]:
                if existing.offset > offset:
                    existing.offset -= shift
            changed = True
            break

    entities.sort(key=lambda e: (e.offset, e.length))
    return ParsedMessage(result, entities)


def count_chars_until(text, stop_chars, start_index=0):
    """Сколько символов до первого из stop_chars; вся длина, если их нет."""
    for i in range(start_index, len(text)):
        if text[i] in stop_chars:
            return i - start_index
    return len(text) - start_index
