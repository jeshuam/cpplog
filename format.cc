#include "format.h"

namespace util {
namespace log {

std::string Format(const std::string& fmt,
                   const std::unordered_map<std::string, std::string>& args) {
  std::string result = fmt;
  for (const auto& pair : args) {
    std::string key = "{" + pair.first + "}";
    size_t location = result.find(key);
    while (location != std::string::npos) {
      // Replace it.
      result.replace(location, key.length(), pair.second);

      // Find the next occurence key (if it exists).
      location = result.find(key);
    }
  }

  return result;
}

std::string FormatEraseTags(const std::string& fmt) {
  std::regex tag_regex("\\{[a-zA-Z0-9]+\\}");
  std::string fmt_clean;
  std::regex_replace(std::back_inserter(fmt_clean), fmt.begin(), fmt.end(),
                     tag_regex, std::string(""));
  return fmt_clean;
}

bool FormatHasTag(const std::string& fmt, const std::string& tag) {
  return fmt.find("{" + tag + "}") != std::string::npos;
}

// Magic color reset.
std::string Reset = "\033[0m";

// Color formats.
std::string Bold = "\033[1m";
std::string Italic = "\033[3m";

// Colors.
std::string Black = "\033[30m";
std::string Red = "\033[31m";
std::string Green = "\033[32m";
std::string Yellow = "\033[33m";
std::string Blue = "\033[34m";
std::string Magenta = "\033[35m";
std::string Cyan = "\033[36m";
std::string White = "\033[37m";
std::string Gray = Black + Bold;

// Color mapping.
const std::unordered_map<std::string, std::string> ColorMapping = {
    {"nc", Reset},      {"bold", Bold},   {"italic", Italic},
    {"black", Black},   {"red", Red},     {"green", Green},
    {"yellow", Yellow}, {"blue", Blue},   {"magenta", Magenta},
    {"cyan", Cyan},     {"white", White}, {"gray", Gray},
};

}  // namespace log
}  // namespace util
