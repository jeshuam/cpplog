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
enum Level { TRACE, DEBUG, INFO, WARNING, ERROR, FATAL, N_LEVELS };

/**
 * @brief      A class representing a single log message.
 */
class LogMessage {
 public:
  LogMessage(Level level, int verbosity, int line, const std::string& file,
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
   * @param[in]  line_fmt  The format string of the line to output.
   */
  void Emit(const std::string& line_fmt) const;

  /**
   * @brief      Get the level the log message was logged at.
   */
  Level level() const { return level_; }

 private:
  /**
   * The level this log message is being printed at.
   */
  Level level_;

  /**
   * The line this log messages was printed on.
   */
  int verbosity_, line_;

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

  static std::array<std::ofstream*, N_LEVELS> _log_files;
};

/**
 * @brief      Queue a message into the messaging queue.
 *
 * @param[in]  message  The message to queue.
 */
void QueueMessage(const LogMessage& message);

/**
 * @brief      A Logger is a utility class that will, when destroyed, wait for
 *             all pending log messages to be displayed before finishing. This
 *             can be used to ensure that async logging messages are properly
 *             printed before termination.
 */
class Logger {
 public:
  ~Logger();
};

}  // namespace internal

/**
 * @brief      Initialize the logging system. This function is only required if
 *             --async_logging is set. This should be called in the main()
 *             function _after_ parsing arguments but _before_ any log messages.
 *
 *                 int main(int argc, char** argv) {
 *                   gflags::ParseCommandLineFlags(&argc, &argv, true);
 *                   cpplog::Init();
 *                 }
 *
 *             Without this init() call, no log messages will display. This can
 *             be omitted when using synchronous logging.
 */
internal::Logger Init();

}  // namespace cpplog

/**
 * @brief      Log a message of a given level to the display.
 *
 * @param      LEVEL       The level to log at, e.g. INFO or WARNING.
 * @param      MSG_FORMAT  The cppstring format string for the  message.
 * @param      ...         The cppstring argument list.
 */
#define LOG(LEVEL, MSG_FORMAT, ...)                                            \
  do {                                                                         \
    ::cpplog::internal::QueueMessage(                                          \
        ::cpplog::internal::LogMessage(::cpplog::internal::LEVEL, 0, __LINE__, \
                                       __FILE__, MSG_FORMAT, __VA_ARGS__));    \
  } while (false)

#define LOG_TRACE(MSG_FORMAT, ...) LOG(TRACE, MSG_FORMAT, __VA_ARGS__)
#define LOG_DEBUG(MSG_FORMAT, ...) LOG(DEBUG, MSG_FORMAT, __VA_ARGS__)
#define LOG_INFO(MSG_FORMAT, ...) LOG(INFO, MSG_FORMAT, __VA_ARGS__)
#define LOG_WARNING(MSG_FORMAT, ...) LOG(WARNING, MSG_FORMAT, __VA_ARGS__)
#define LOG_ERROR(MSG_FORMAT, ...) LOG(ERROR, MSG_FORMAT, __VA_ARGS__)
#define LOG_FATAL(MSG_FORMAT, ...) LOG(FATAL, MSG_FORMAT, __VA_ARGS__)
