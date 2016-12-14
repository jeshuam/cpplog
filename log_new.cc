#include "log_new.h"

#include <condition_variable>
#include <ctime>
#include <deque>
#include <mutex>
#include <thread>

#include <gflags/gflags.h>

#include "util/string/constants.h"

// TODO: features to add:
//    - cleanup the code a lot
//    - setting the minimum log level for display
//    - logging to multiple streams (different files per message type etc.)
//      e.g. LOG.TRACE, LOG.INFO (where LOG.DEBUG includes DEBUG and above)
//    - get colors working properly
//    - log file rotation based on a flag
//    - trim filenames in log messages and line numbers for consistent line
//      lengths
//    - display more precise time with the datetime
//    - allow displaying thread number
//    - LOG_IF(n)
//    - LOG_EVERY_N
//    - LOG_IF_EVERY_N
//    - LOG_FIRST_N
//    - DLOG* (logs compiled out depending on configuration)
//    - VLOG(level); support arbitrary levels

DEFINE_bool(cpplog_async_logging, false,
            "When enabled, perform logging asynchronously.");

DEFINE_string(cpplog_line_format,
              "{nc}{lc}{level}{nc} {bold}{white}@{nc} {gray}{datetime}{nc} "
              ": {white}{italic}{file}:{line}{nc} {bold}{white}::{nc} "
              "{lc}{message}{nc}",
              "A Python-ish format string of the log header information.");

DEFINE_bool(cpplog_colorize_output, true,
            "When enabled, colorize output messages. This is not done when "
            "logging to files.");

DEFINE_string(cpplog_datetime_format, "%a %b %d %T",
              "The strftime formatting to use for {datetime} substitution.");

namespace cpplog {

namespace internal {

namespace {

/**
 * The message queue to store log messages in.
 */
std::deque<LogMessage> message_queue;
std::mutex message_queue_insert_lock, emit_lock;
std::thread* message_processing_thread;
std::condition_variable message_queue_condition;

std::string _LevelToString(Level level) {
  switch (level) {
    case TRACE:
      return "T";
    case DEBUG:
      return "D";
    case INFO:
      return "I";
    case WARNING:
      return "W";
    case ERROR:
      return "E";
    case FATAL:
      return "F";
    default:
      return "?";
  }
}

std::string _GetColor(Level level) {
  switch (level) {
    case TRACE:
    case DEBUG:
      return string::color::kGray;
    case INFO:
#if OS_WINDOWS
      return string::color::kCyan + string::color::kBold;
#else
      return string::color::kBlue + string::color::kBold;
#endif  // OS_WINDOWS
    case WARNING:
      return string::color::kYellow + string::color::kBold;
    case ERROR:
    case FATAL:
      return string::color::kRed + string::color::kBold;
    default:
      return "";
  }
}

void _DoEmitMessage(const LogMessage& msg) {
  if (!FLAGS_cpplog_async_logging) emit_lock.lock();
  msg.Emit(FLAGS_cpplog_line_format, std::cout);
  if (!FLAGS_cpplog_async_logging) emit_lock.unlock();
}

void _ProcessMessageQueue() {
  std::mutex mutex;
  std::unique_lock<std::mutex> lock(mutex);

  while (true) {
    // Wait for something to appear.
    message_queue_condition.wait(lock, [] { return message_queue.size() > 0; });

    if (message_queue.size() > 0) {
      // Get the next thing to display.
      auto msg = message_queue.front();

      // Emit the message.
      _DoEmitMessage(msg);

      // Remove it from the queue now that it has been displayed.
      message_queue.pop_front();
    }
  }
}

}  // namespace

LogMessage::LogMessage(Level level, int line, const std::string& file,
                       const std::string& msg_format,
                       const string::FormatListType& format_args)
    : level_(level),
      line_(line),
      file_(file),
      log_time_(std::chrono::system_clock::now()),
      msg_format_(msg_format),
      format_args_(format_args) {}

void LogMessage::Emit(const std::string& line_fmt, std::ostream& out,
                      bool color) const {
  // Format the message.
  auto msg_formatted = string::Format(msg_format_, format_args_);

  // Format the datetime.
  char time_str_buffer[256];
  auto log_time_c = std::chrono::system_clock::to_time_t(log_time_);
  std::strftime(time_str_buffer, sizeof(time_str_buffer),
                FLAGS_cpplog_datetime_format.c_str(),
                std::localtime(&log_time_c));

  // // Format the line.
  // // TODO(jeshua): Make this actually format the line properly.
  auto line_formatted =
      string::FormatMap(line_fmt, {{"message", msg_formatted},
                                   {"file", file_},
                                   {"line", line_},
                                   {"datetime", time_str_buffer},
                                   {"level", _LevelToString(level_)}},
                        true);

  // Add colors if requested.
  if (color) {
    line_formatted = string::FormatMap(line_formatted,
                                       {
                                           {"nc", string::color::kReset},
                                           {"bold", string::color::kBold},
                                           {"italic", string::color::kItalic},
                                           {"black", string::color::kBlack},
                                           {"red", string::color::kRed},
                                           {"green", string::color::kGreen},
                                           {"yellow", string::color::kYellow},
                                           {"blue", string::color::kBlue},
                                           {"magenta", string::color::kMagenta},
                                           {"cyan", string::color::kCyan},
                                           {"white", string::color::kWhite},
                                           {"gray", string::color::kGray},
                                           {"lc", _GetColor(level_)},
                                       });
  }

  out << string::FormatTrimTags(line_formatted) << std::endl;
}

void QueueMessage(const LogMessage& msg) {
  if (FLAGS_cpplog_async_logging) {
    message_queue_insert_lock.lock();
    message_queue.push_back(msg);
    message_queue_condition.notify_one();
    message_queue_insert_lock.unlock();
  } else {
    _DoEmitMessage(msg);
  }
}

}  // namespace internal

Logger::~Logger() {
  while (internal::message_queue.size() > 0) {
    ;  // do nothing, wait for the thread
  }
}

Logger Init() {
  // Start the thread, if required.
  if (FLAGS_cpplog_async_logging) {
    internal::message_processing_thread =
        new std::thread(internal::_ProcessMessageQueue);
  }

  return Logger();
}

}  // namespace cpplog

int main(int argc, char** argv) {
  gflags::ParseCommandLineFlags(&argc, &argv, true);
  auto _ = cpplog::Init();

  LOG_INFO("a = {}, {}", {1, "c"});
  LOG_TRACE("a = {}, {}", {1, "c"});

  return 0;
}
