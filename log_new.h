#pragma once

#include <chrono>
#include <ostream>
#include <string>

#include "util/string/format.h"

namespace cpplog {

namespace internal {

/**
 * @brief      An enum representing the levels within the logging system.
 */
enum Level { TRACE, DEBUG, INFO, WARNING, ERROR, FATAL };

/**
 * @brief      A class representing a single log message.
 */
class LogMessage {
 public:
  LogMessage(Level level, int line, const std::string& file,
             const std::string& msg_format,
             const string::FormatListType& format_args);

  /**
   * @brief      Emit this log message to the given stream using some format.
   *
   * @details    Before emitting, this message will be formatted according to
   *             `line_fmt`. A number of replacements will be made:
   *
   *             - {message} will be replaced with the message.
   *             - {datetime} will be the date the message was created.
   *             - {file} will be the file the message was logged from.
   *
   *             If `color` is set to `true`:
   *
   *             - {level} will be replaced with the level string.
   *             - {lc} will be replace with a color based on the level.
   *             - {red}, {blue}... etc. will be replaced with ANSI codes for
   *               the color they represent. You can also do the same with
   *               punctuation.
   *
   *             This message will then be output along the given stream.
   *
   * @param[in]  line_fmt  The format string of the line to output.
   * @param      out       The stream to output the message to.
   * @param[in]  color     Whether or not to enable colored output.
   */
  void Emit(const std::string& line_fmt, std::ostream& out,
            bool color = true) const;

 private:
  /**
   * The level this log message is being printed at.
   */
  Level level_;

  /**
   * The line this log messages was printed on.
   */
  int line_;

  /**
   * The filename that this log message was printed from.
   */
  std::string file_;

  /**
   * Time that this message was logged.
   */
  std::chrono::time_point<std::chrono::system_clock> log_time_;

  /**
   * The message to be displayed to the screen. This is not a format string, but
   * a full message which can be displayed as-is.
   */
  std::string msg_format_;

  /**
   * The formatting args required to format `msg`.
   */
  string::FormatListType format_args_;
};

/**
 * @brief      Queue a message into the messaging queue.
 *
 * @param[in]  message  The message to queue.
 */
void QueueMessage(const LogMessage& message);

}  // namespace internal

class Logger {
 public:
  ~Logger();
};

// Initialize the logging system.
Logger Init();

}  // namespace cpplog

/**
 * @brief      Log a message of a given level to the display.
 *
 * @param      LEVEL       The level to log at, e.g. INFO or WARNING.
 * @param      MSG_FORMAT  The cppstring format string for the  message.
 * @param      ...         The cppstring argument list.
 */
#define LOG(LEVEL, MSG_FORMAT, ...)                                         \
  do {                                                                      \
    ::cpplog::internal::LogMessage msg(::cpplog::internal::LEVEL, __LINE__, \
                                       __FILE__, MSG_FORMAT, __VA_ARGS__);  \
    ::cpplog::internal::QueueMessage(msg);                                  \
  } while (false)

#define LOG_TRACE(MSG_FORMAT, ...) LOG(TRACE, MSG_FORMAT, __VA_ARGS__)
#define LOG_DEBUG(MSG_FORMAT, ...) LOG(DEBUG, MSG_FORMAT, __VA_ARGS__)
#define LOG_INFO(MSG_FORMAT, ...) LOG(INFO, MSG_FORMAT, __VA_ARGS__)
#define LOG_WARNING(MSG_FORMAT, ...) LOG(WARNING, MSG_FORMAT, __VA_ARGS__)
#define LOG_ERROR(MSG_FORMAT, ...) LOG(ERROR, MSG_FORMAT, __VA_ARGS__)
#define LOG_FATAL(MSG_FORMAT, ...) LOG(FATAL, MSG_FORMAT, __VA_ARGS__)
