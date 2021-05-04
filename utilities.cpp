#include "utilities.h"

#include <boost/nowide/convert.hpp>

namespace utils {

std::string Utf16ToUtf8(const std::wstring& str) {
  return boost::nowide::narrow(str);
}

std::wstring Utf8ToUtf16(const std::string& str) {
  return boost::nowide::widen(str);
}

}