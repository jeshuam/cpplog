#pragma once

#include <chrono>
#include <string>
#include <sstream>
#include <vector>

namespace util {
namespace log {

/**
 * @brief An output log with several levels of specificity.
 */
class Log {
 public:
  enum Level { TRACE, DEBUG, INFO, WARNING, ERROR, FATAL };
  typedef std::tuple<Level, int, const char*, std::string> LogMessageTuple;

  /**
   * @brief      Determine whether the logging subsystem is ready to emit
   *             messages yet or not.
   *
   * @return     true if we are ready, false otherwise.
   */
  static bool IsReadyToLog();

  /**
   * @brief      Set the minimum log level required to display. Any log messages
   *             that are lower than this level will not be displayed at all.
   *
   * @param[in]  level The new minimum logging level.
   */
  static void SetMinLogLevel(Level level);

  /**
   * @brief      Get the current minimum logging level.
   *
   * @param[out] The current minimum log level.
   */
  static Level MinLogLevel() { return _min_log_level; }

  /**
   * @brief      Convert a string to the corresponding enum level.
   *
   * @param[in]  level  { The string representation of the level. }
   *
   * @return     { The level corresponding to the given string. }
   *
   * @throw      std::invalid_argument  { Thrown if the given string doesn't
   *                                      match any log level. }
   */
  static Level StringToLogLevel(const std::string& level);

  /**
   * @brief      Determines whether an output target is active.
   *
   * @return     true if some output target is active, false otherwise.
   */
  static bool IsAnyOutputActive();

  // Internal logging functions.
  static void EmitFormat(Level level, int line, const char* file,
                         const char* fmt, ...);
  static void Emit(Level level, int line, const char* file,
                   const std::string& message);

 private:
  // Actually emit a message.
  static void _DoEmit(Level level, const std::string& message);
  static void _SaveMessage(Level level, int line, const char* file,
                           const std::string& message);
  static void _EmitSavedMessages();

  // Generate the header for the log at the current time.
  static std::string _GetFilenameToDisplay(int line, const char* file);
  static std::string _GenerateLogMessage(Level level, int line,
                                         const char* file,
                                         const std::string& message);

  // Header methods.
  static std::string _GetLevelString(Level level);
  static std::string _GetLevelColor(Level level);
  static std::string _GetDatetimeString();

  // Open the output file if it hasn't already been opened.
  static void _OpenOutputFileIfNecessary();

  // Rotate the log file to a new file. The current log file will be renamd to
  // FLAGS_log_file.1 and all new log entries will be written to the new log.
  static void _RotateLogFileIfNecessary();

  // Current minimum log level.
  static Level _min_log_level;

  // Messages which occur before main has started.
  static std::vector<LogMessageTuple>* _pre_init_messages;

  // Current output log file.
  static std::ofstream _output_file;
};

/**
 * @brief      A logging infrastructure item that will scope the log system. All
 *             log messages created while these objects are active will be
 *             indented by a set number of spaces. Scoped logs initially emit a
 *             name, and will display the name again when the object is
 *             destroyed. Scoped logging messages are always emitted
 */
class ScopedLog {
 public:
  ScopedLog(Log::Level level, const std::string& name, int line,
            const char* file);
  ~ScopedLog();

 private:
  Log::Level _level;
  std::string _name, _file;
  int _line;
};

}  // namespace log
}  // namespace util

/**
 * Internal, low-level log macros. Should noe be used directly.
 */
#define _LOG(level, fmt, ...)                                      \
  do {                                                             \
    if (!::util::log::Log::IsReadyToLog() ||                       \
        (::util::log::Log::IsAnyOutputActive() &&                  \
         level >= ::util::log::Log::MinLogLevel())) {              \
      ::util::log::Log::EmitFormat(level, __LINE__, __FILE__, fmt, \
                                   ##__VA_ARGS__);                 \
    }                                                              \
  } while (false)

#define _LOG_STREAM(level, stream)                          \
  do {                                                      \
    if (!::util::log::Log::IsReadyToLog() ||                \
        (::util::log::Log::IsAnyOutputActive() &&           \
         level >= ::util::log::Log::MinLogLevel())) {       \
      std::stringstream _tmp_sstream##__LINE__;             \
      _tmp_sstream##__LINE__ << stream;                     \
      ::util::log::Log::Emit(level, __LINE__, __FILE__,     \
                             _tmp_sstream##__LINE__.str()); \
    }                                                       \
  } while (false)

#define _LOG_SCOPED(level)                                                 \
  ::util::log::ScopedLog _scoped_log##__LINE__(level, __PRETTY_FUNCTION__, \
                                               __LINE__, __FILE__)

// Timed log messages; log a message once every
// interval.
// using namespace std::literals;
// using namespace std::chrono_literals;

#define _LOG_EVERY(level, freq, fmt, ...)                                   \
  do {                                                                      \
    if (!::util::log::Log::IsReadyToLog() ||                                \
        (::util::log::Log::IsAnyOutputActive() &&                           \
         level >= ::util::log::Log::MinLogLevel())) {                       \
      static std::chrono::high_resolution_clock::time_point                 \
          _start_time_##__LINE__;                                           \
      if ((std::chrono::high_resolution_clock::now() -                      \
           _start_time_##__LINE__) > freq) {                                \
        _start_time_##__LINE__ = std::chrono::high_resolution_clock::now(); \
        ::util::log::Log::EmitFormat(level, __LINE__, __FILE__, fmt,        \
                                     ##__VA_ARGS__);                        \
      }                                                                     \
    }                                                                       \
  } while (false)

#define _LOG_STREAM_EVERY(level, freq, stream)                              \
  do {                                                                      \
    if (!::util::log::Log::IsReadyToLog() ||                                \
        (::util::log::Log::IsAnyOutputActive() &&                           \
         level >= ::util::log::Log::MinLogLevel())) {                       \
      static std::chrono::high_resolution_clock::time_point                 \
          _start_time_##__LINE__;                                           \
      if ((std::chrono::high_resolution_clock::now() -                      \
           _start_time_##__LINE__) > freq) {                                \
        _start_time_##__LINE__ = std::chrono::high_resolution_clock::now(); \
        std::stringstream _tmp_sstream##__LINE__;                           \
        _tmp_sstream##__LINE__ << stream;                                   \
        ::util::log::Log::Emit(level, __LINE__, __FILE__,                   \
                               _tmp_sstream##__LINE__.str());               \
      }                                                                     \
    }                                                                       \
  } while (false)

/**
 * Log messages to the screen using a format string. The log messages will have
 * the format LOG_INFO("Hello %d", 3);.
 */
#define LOG_TRACE(fmt, ...) _LOG(::util::log::Log::TRACE, fmt, ##__VA_ARGS__)
#define LOG_DEBUG(fmt, ...) _LOG(::util::log::Log::DEBUG, fmt, ##__VA_ARGS__)
#define LOG_INFO(fmt, ...) _LOG(::util::log::Log::INFO, fmt, ##__VA_ARGS__)
#define LOG_WARNING(fmt, ...) \
  _LOG(::util::log::Log::WARNING, fmt, ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) _LOG(::util::log::Log::ERROR, fmt, ##__VA_ARGS__)
#define LOG_FATAL(fmt, ...) _LOG(::util::log::Log::FATAL, fmt, ##__VA_ARGS__)

/**
 * Log messages to the screen using a stream. The log messages will have the
 * format LOG_INFO_STREAM("hello " << "world " << 3);. The stream will not be
 * evaluated unless the log message is going to be displayed. Note however that
 * the whole stream will be buffered, so you can't use this to send super super
 * long streams.
 */
#define LOG_TRACE_STREAM(stream) _LOG_STREAM(::util::log::Log::TRACE, stream)
#define LOG_DEBUG_STREAM(stream) _LOG_STREAM(::util::log::Log::DEBUG, stream)
#define LOG_INFO_STREAM(stream) _LOG_STREAM(::util::log::Log::INFO, stream)
#define LOG_WARNING_STREAM(stream) \
  _LOG_STREAM(::util::log::Log::WARNING, stream)
#define LOG_ERROR_STREAM(stream) _LOG_STREAM(::util::log::Log::ERROR, stream)
#define LOG_FATAL_STREAM(stream) _LOG_STREAM(::util::log::Log::FATAL, stream)

/**
 * Add a scope around the next block. This will indent all future log messages
 * until this scope has been exited, at which the log messages will be dedented.
 * If the scope level is less than the minimum, no scope will be applied. The
 * scope will have the method signature tha the scope was added in as a message.
 */
#define LOG_TRACE_SCOPED() _LOG_SCOPED(::util::log::Log::TRACE)
#define LOG_DEBUG_SCOPED() _LOG_SCOPED(::util::log::Log::DEBUG)
#define LOG_INFO_SCOPED() _LOG_SCOPED(::util::log::Log::INFO)
#define LOG_WARNING_SCOPED() _LOG_SCOPED(::util::log::Log::WARNING)
#define LOG_ERROR_SCOPED() _LOG_SCOPED(::util::log::Log::ERROR)

/**
 * Emit a log message at some frequency. The frequency can be a C++14 fancy time
 * thing (e.g. 1s, 5min, 4ms). This only guarantees that the log messages will
 * be _at least_ freq apart; they may be more depending on the code path and
 * other delays.
 */
#define LOG_TRACE_EVERY(freq, fmt, ...) \
  _LOG_EVERY(::util::log::Log::TRACE, freq, fmt, ##__VA_ARGS__)
#define LOG_DEBUG_EVERY(freq, fmt, ...) \
  _LOG_EVERY(::util::log::Log::DEBUG, freq, fmt, ##__VA_ARGS__)
#define LOG_INFO_EVERY(freq, fmt, ...) \
  _LOG_EVERY(::util::log::Log::INFO, freq, fmt, ##__VA_ARGS__)
#define LOG_WARNING_EVERY(freq, fmt, ...) \
  _LOG_EVERY(::util::log::Log::WARNING, freq, fmt, ##__VA_ARGS__)
#define LOG_ERROR_EVERY(freq, fmt, ...) \
  _LOG_EVERY(::util::log::Log::ERROR, freq, fmt, ##__VA_ARGS__)

/**
 * Same as the above format methods but for streams.
 */
#define LOG_TRACE_STREAM_EVERY(freq, stream) \
  _LOG_STREAM_EVERY(::util::log::Log::TRACE, freq, stream)
#define LOG_DEBUG_STREAM_EVERY(freq, stream) \
  _LOG_STREAM_EVERY(::util::log::Log::DEBUG, freq, stream)
#define LOG_INFO_STREAM_EVERY(freq, stream) \
  _LOG_STREAM_EVERY(::util::log::Log::INFO, freq, stream)
#define LOG_WARNING_STREAM_EVERY(freq, stream) \
  _LOG_STREAM_EVERY(::util::log::Log::WARNING, freq, stream)
#define LOG_ERROR_STREAM_EVERY(freq, stream) \
  _LOG_STREAM_EVERY(::util::log::Log::ERROR, freq, stream)
