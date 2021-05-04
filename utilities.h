#pragma once

#include <string>

namespace utils {
std::string Utf16ToUtf8(const std::wstring& str);
std::wstring Utf8ToUtf16(const std::string& str);
}