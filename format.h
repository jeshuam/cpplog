#pragma once

#include <regex>
#include <string>
#include <unordered_map>

namespace util {
namespace log {

/**
 * @brief      Format the given string with parameters from the given map. This
 *             essentially implements a very basic version of the Python .format
 *             format strings.
 *
 * @param[in]  fmt   The Python-style .format() format string with names.
 * @param[in]  args  The parameters to write to the format string.
 *
 * @return     The formatting string.
 */
std::string Format(const std::string& fmt,
                   const std::unordered_map<std::string, std::string>& args);

/**
 * @brief      Erase all format tags from the given string.
 *
 * @param[in]  fmt   The format string to remove tags from.
 *
 * @return     A new string with the format tags removed.
 */
std::string FormatEraseTags(const std::string& fmt);

/**
 * @brief      Check whether a given format has some tag within it.
 *
 * @param[in]  fmt   The format to check.
 * @param[in]  tag   The tag to search for.
 *
 * @return     true if the format contains at least one instance of tag.
 */
bool FormatHasTag(const std::string& fmt, const std::string& tag);

// Magic color reset.
extern std::string Reset;

// Color formats.
extern std::string Bold;
extern std::string Italic;

// Colors.
extern std::string Black;
extern std::string Red;
extern std::string Green;
extern std::string Yellow;
extern std::string Blue;
extern std::string Magenta;
extern std::string Cyan;
extern std::string White;
extern std::string Gray;

// Mapping of color names --> colors.
extern const std::unordered_map<std::string, std::string> ColorMapping;

}  // namespace log
}  // namespace util
