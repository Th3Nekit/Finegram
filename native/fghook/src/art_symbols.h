// Finegram: поиск символов libart по префиксу.
// It is licensed under GNU GPL v. 2 or later.

#pragma once

#include <string_view>

namespace fghook {

// Адрес первой функции libart, чьё имя начинается с prefix; nullptr, если такой нет.
//
// Нужна для статических функций ART: с Android 14 к их именам дописывается суффикс
// вида ".__uniq.<число>", и точным именем их не найти. Такие функции лежат только в
// .symtab, а он обычно упакован в .gnu_debugdata, поэтому образ читается с диска.
void *FindArtSymbolByPrefix(std::string_view prefix);

// Отпускает прочитанный образ: поиск по префиксу нужен только при запуске движка.
void ReleaseArtSymbols();

}  // namespace fghook
