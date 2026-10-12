from html.parser import HTMLParser

from markdown_utils import (ParsedMessage, RawEntity, TLEntityType, parse_markdown,
                            to_utf16_len)

__all__ = ["parse_text", "parse_html", "parse_markdown", "RawEntity", "TLEntityType"]

_SIMPLE_TAGS = {
    "b": TLEntityType.BOLD, "strong": TLEntityType.BOLD,
    "i": TLEntityType.ITALIC, "em": TLEntityType.ITALIC,
    "u": TLEntityType.UNDERLINE, "ins": TLEntityType.UNDERLINE,
    "s": TLEntityType.STRIKETHROUGH, "strike": TLEntityType.STRIKETHROUGH, "del": TLEntityType.STRIKETHROUGH,
    "code": TLEntityType.CODE,
    "tg-spoiler": TLEntityType.SPOILER,
    "blockquote": TLEntityType.QUOTE,
}

class _HtmlCollector(HTMLParser):

    def __init__(self):
        super().__init__(convert_charrefs=True)
        self.parts = []
        self.length = 0
        self.stack = []
        self.entities = []

    def _open(self, entity_type, **extra):
        self.stack.append((entity_type, self.length, extra))

    def handle_starttag(self, tag, attrs):
        attrs = dict(attrs)
        if tag == "br":
            self.handle_data("\n")
            return
        if tag == "a":
            self._open(TLEntityType.TEXT_LINK, url=attrs.get("href") or "")
        elif tag == "pre":
            self._open(TLEntityType.PRE, language="")
        elif tag == "code" and self.stack and self.stack[-1][0] == TLEntityType.PRE:

            language = (attrs.get("class") or "").replace("language-", "")
            entity_type, start, extra = self.stack[-1]
            extra["language"] = language
            self.stack.append(("pre-code", self.length, {}))
        elif tag == "span" and "tg-spoiler" in (attrs.get("class") or ""):
            self._open(TLEntityType.SPOILER)
        elif tag == "tg-emoji":
            self._open(TLEntityType.CUSTOM_EMOJI, document_id=attrs.get("emoji-id") or 0)
        elif tag in _SIMPLE_TAGS:
            self._open(_SIMPLE_TAGS[tag])
        else:
            self.stack.append((None, self.length, {}))

    def handle_endtag(self, tag):
        if tag == "br" or not self.stack:
            return
        entity_type, start, extra = self.stack.pop()
        if entity_type in (None, "pre-code"):
            return
        length = self.length - start
        if length > 0:
            self.entities.append(RawEntity(entity_type, start, length,
                                           language=extra.get("language"),
                                           url=extra.get("url"),
                                           document_id=extra.get("document_id")))

    def handle_data(self, data):
        self.parts.append(data)
        self.length += to_utf16_len(data)

def parse_html(text):
    collector = _HtmlCollector()
    collector.feed(str(text or ""))
    collector.close()
    while collector.stack:
        collector.handle_endtag("")
    entities = sorted(collector.entities, key=lambda e: (e.offset, -e.length))
    return ParsedMessage("".join(collector.parts), entities)

def parse_text(text, parse_mode="markdown", *args, **kwargs):

    mode = str(parse_mode or "markdown").lower()
    parsed = parse_html(text) if mode == "html" else parse_markdown(text)

    try:
        from java.util import ArrayList

        entities = ArrayList()
        add = entities.add
    except Exception:
        entities = []
        add = entities.append
    for entity in parsed.entities:
        try:
            built = entity.to_tl()
        except Exception:
            built = None
        if built is not None:
            add(built)
    return {"message": parsed.text, "entities": entities}
