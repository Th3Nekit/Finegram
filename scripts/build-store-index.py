#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import argparse
import json
import os
import sys
import time
import urllib.request

MIRRORS = [
    "https://git.kangel.xyz/KangelPlugins/Plugins-Store/raw/branch/main/store.json",
    "https://gitverse.ru/api/repos/bigfishtheory/Plugins-Store/raw/branch/main/store.json",
    "https://codeberg.org/Kangel/Plugins-Store/raw/branch/main/store.json",
    "https://raw.githubusercontent.com/Kangel-Plugins/Plugins-Store/main/store.json",
]

TIMEOUT = 20

def fetch(url):
    request = urllib.request.Request(url, headers={"User-Agent": "Finegram-store-builder"})
    with urllib.request.urlopen(request, timeout=TIMEOUT) as response:
        return response.read().decode("utf-8")

def load_catalogue():

    errors = []
    for url in MIRRORS:
        try:
            data = json.loads(fetch(url))
            if isinstance(data, dict) and data:
                return data, url
            errors.append("%s: опись пуста" % url)
        except Exception as error:
            errors.append("%s: %s" % (url, error))
    raise RuntimeError("ни одно зеркало не ответило:\n  " + "\n  ".join(errors))

def load_own(path):
    if not os.path.exists(path):
        return {}
    with open(path, encoding="utf-8") as handle:
        data = json.load(handle)
    return data if isinstance(data, dict) else {}

def to_item(plugin_id, raw):

    if isinstance(raw, str):
        raw = {"url": raw}
    url = str(raw.get("url", "")).strip()
    if not url.startswith("https://"):
        return None

    url = url.replace(" ", "%20")

    item = {
        "id": plugin_id,
        "kind": "icons" if url.endswith(".icons") else "plugin",
        "name": raw.get("name", plugin_id),
        "author": raw.get("author", ""),
        "version": str(raw.get("version", "")),
        "description": raw.get("description", ""),
        "icon": raw.get("icon", ""),
        "url": url,
        "status": raw.get("status", ""),
        "hash": raw.get("hash", ""),
        "signature": raw.get("signature", ""),
        "min_version": raw.get("min_version", ""),
    }
    for key, value in raw.items():
        if key.startswith("description_") and value:
            item[key] = value
    for key in ("requirements", "dependencies"):
        value = raw.get(key)
        if value:
            item[key] = value if isinstance(value, list) else [
                part.strip() for part in str(value).split(",") if part.strip()
            ]
    for key in ("size", "updated_at", "downloads"):
        if raw.get(key):
            item[key] = raw[key]
    return item

def build(own_path):
    catalogue, source = load_catalogue()
    own = load_own(own_path)

    merged = dict(catalogue)
    merged.update(own)

    items = []
    skipped = []
    for plugin_id, raw in merged.items():
        item = to_item(plugin_id, raw)
        if item is None:
            skipped.append(plugin_id)
            continue
        items.append(item)

    items.sort(key=lambda entry: entry["name"].lower())
    return {
        "version": 1,
        "built_at": int(time.time()),
        "source": source,
        "items": items,
    }, skipped

def main():
    parser = argparse.ArgumentParser(description="Сборка описи магазина Finegram")
    parser.add_argument("--out", required=True, help="куда записать index.json")
    parser.add_argument(
        "--own",
        default=os.path.join(os.path.dirname(os.path.abspath(__file__)), "own.json"),
        help="файл со своими записями",
    )
    args = parser.parse_args()

    index, skipped = build(args.own)

    directory = os.path.dirname(os.path.abspath(args.out))
    if directory:
        os.makedirs(directory, exist_ok=True)
    tmp = args.out + ".tmp"
    with open(tmp, "w", encoding="utf-8") as handle:
        json.dump(index, handle, ensure_ascii=False, indent=2)
    os.replace(tmp, args.out)

    print("записей: %d, источник: %s" % (len(index["items"]), index["source"]))
    if skipped:
        print("пропущено (адрес не по https): %s" % ", ".join(skipped))

if __name__ == "__main__":
    try:
        main()
    except Exception as error:
        print("сборка описи не удалась: %s" % error, file=sys.stderr)
        sys.exit(1)
