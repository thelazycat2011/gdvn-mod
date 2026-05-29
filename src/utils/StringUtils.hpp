#pragma once

#include <string>

namespace gdvn::utils::string {

std::string replaceAll(std::string text, std::string const& from, std::string const& to);
std::string toAsciiCompatible(std::string text);

} // namespace gdvn::utils::string
