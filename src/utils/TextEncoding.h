#pragma once

#include <string>
#include <string_view>

namespace overlay::utils {

std::wstring Utf8ToWstring(std::string_view utf8);
std::string WstringToUtf8(std::wstring_view wstr);

} // namespace overlay::utils
