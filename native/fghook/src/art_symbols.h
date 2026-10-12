// It is licensed under GNU GPL v. 2 or later.

#pragma once

#include <string_view>

namespace fghook {

void *FindArtSymbolByPrefix(std::string_view prefix);

void ReleaseArtSymbols();

}
