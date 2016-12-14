#include "log.h"

#include <array>
#include <condition_variable>
#include <cstdlib>
#include <ctime>
#include <deque>
#include <fstream>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <thread>

#include <gflags/gflags.h>
#include <boost/filesystem.hpp>

#include "util/string/constants.h"
#include "util/string/util.h"

// OUTPUT TOGGLES
DEFINE_bool(logtofile, false,
            "Whether or not log messages should be output to a file.");
DEFINE_bool(logtostderr, false,
            "Whether or not log messages should be output to stderr.");

// OUTPUT OPTIONS
DEFINE_string(logfile_dir, "log", "The directory to store logfiles in.");

DEFINE_string(logfile_name, "",
              "The name of the logfile to create. Will be suffixed with the "
              "logging level. Defaults to the binary name seen by gflags.");

DEFINE_bool(colorize_output, true,
            "When enabled, colorize output messages. This is not done when "
            "logging to files.");

DEFINE_string(min_log_level, "info",
              "The minimum log level to display when writing to stderr. Note "
              "that multiple log files are written, so this flag will not "
              "affect those.");

DEFINE_string(min_log_level_file, "trace",
              "The minimum log level to display when writing to files. If set "
              "to anything other than TRACE, files lower than the given level "
              "will not be created.");

DEFINE_uint32(v, 0,
              "The verbosity level to log at. Only messages with a verbosity "
              "level <= this will be logged.");

// OUTPUT FILE OPTIONS
DEFINE_uint32(logfile_max_size_mb, 50,
              "The maximum number of MiB a single logging file will take up. "
              "Note that actual disk usage might vary, because the logging "
              "system will keep the previous log file in addition to the "
              "current log file.");

// OUTPUT FORMATS
DEFINE_string(line_format,
              "{nc}{lc}{level}{nc} {gray}{thread}{nc} {bold}{white}@{nc} "
              "{gray}{datetime}{nc} "
              ": {white}{italic}{file}{nc} {bold}{white}::{nc} "
              "{lc}{message}{nc}",
              "A Python-ish format string of the log header information.");

DEFINE_string(datetime_format, "%a %b %d %T",
              "The strftime formatting to use for {datetime} substitution.");

DEFINE_string(datetime_precision, "us",
              "The precision in which to show the datetime to. Can be one of "
              "s, ms, us or ns. Note that not all platforms will support all "
              "levels.");

// PROCESSING OPTIONS
DEFINE_bool(async_logging, false,
            "When enabled, perform logging asynchronously.");

DEFINE_uint32(async_queue_max_len, 10000,
              "Maximum number of log messages to be stored in the queue until "
              "any additional messages are blocked.");

DEFINE_uint32(
    max_filename_len, 20,
    "Maximum length of the filenames to display in the log. All "
    "filenames will either be padded to this value, or truncated. "
    "Note that this length does not include the path or the line number.");

DEFINE_uint32(max_line_number_len, 4,
              "Maximum number of line number characters to print.");

namespace cpplog {

namespace internal {

namespace {

/**
 * The message queue to store log messages in.
 */
std::deque<LogMessage> LOG_MESSAGE_QUEUE;
std::condition_variable LOG_MESSAGE_QUEUE_INSERT;
std::mutex LOG_MESSAGE_QUEUE_INSERT_LOCK;
bool SHUTTING_DOWN = false;
std::thread* LOG_EMITTER;

/**
 * Log files to write to. They will be opened only once (when they are used) and
 * will be written to from then on.
 */
std::array<std::ofstream, N_LEVELS> LOG_FILES;

// UTILITY FUNCTIONS.
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

std::string _LevelToLongString(Level level) {
  switch (level) {
    case TRACE:
      return "TRACE";
    case DEBUG:
      return "DEBUG";
    case INFO:
      return "INFO";
    case WARNING:
      return "WARNING";
    case ERROR:
      return "ERROR";
    case FATAL:
      return "FATAL";
    default:
      return "?";
  }
}

Level _StringToLevel(const std::string& level) {
  auto level_upper = string::ToUpper(level);
  if (level_upper == "TRACE") {
    return TRACE;
  } else if (level_upper == "DEBUG") {
    return DEBUG;
  } else if (level_upper == "INFO") {
    return INFO;
  } else if (level_upper == "WARNING") {
    return WARNING;
  } else if (level_upper == "ERROR") {
    return ERROR;
  } else if (level_upper == "FATAL") {
    return FATAL;
  } else {
    return TRACE;
  }
}

std::string _GetColor(Level level) {
  switch (level) {
    case TRACE:
    case DEBUG:
      return string::color::kGray;
    case INFO:
#ifdef OS_WINDOWS
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

/**
 * @brief      Actually emit a message to all output streams.
 *
 *             This will, based on the current logging configuration, either:
 *             - Output to a file, based on the logging level.
 *             - Output to the display, based on the minimum level.
 *
 *             Calls to Emit() will be locked during synchronous mode to ensure
 *             that multiple threads do not print over eachother.
 *
 * @param[in]  msg   The message to emit.
 */
void _DoEmitMessage(const LogMessage& msg) {
  static std::mutex emit_lock;

  // If we aren't logging, then stop. This might make things a bit faster when
  // logging is disabled.
  if (!FLAGS_logtofile && !FLAGS_logtostderr) {
    return;
  }

  if (!FLAGS_async_logging) emit_lock.lock();
  msg.Emit(FLAGS_line_format);
  if (!FLAGS_async_logging) emit_lock.unlock();
}

/**
 * @brief      Function called within a thread to process messages. Will only be
 *             used if --async_logging is enabled.
 */
void _ProcessMessageQueue() {
  std::mutex mutex;
  std::unique_lock<std::mutex> lock(mutex);

  while (!SHUTTING_DOWN) {
    // Wait for something to appear.
    LOG_MESSAGE_QUEUE_INSERT.wait(
        lock, [] { return SHUTTING_DOWN || LOG_MESSAGE_QUEUE.size() > 0; });

    while (LOG_MESSAGE_QUEUE.size() > 0) {
      // Get the next thing to display.
      LOG_MESSAGE_QUEUE_INSERT_LOCK.lock();
      auto msg = LOG_MESSAGE_QUEUE.front();
      LOG_MESSAGE_QUEUE.pop_front();
      LOG_MESSAGE_QUEUE_INSERT_LOCK.unlock();

      // Emit the message.
      _DoEmitMessage(msg);
    }
  }
}

template <class Duration = std::chrono::milliseconds>
void _GetSubSecondTimeIn(
    const std::chrono::time_point<std::chrono::system_clock>& log_time,
    int* sub_second_time, int* n_digits) {
  using namespace std::chrono;
  typename Duration::period period;

  auto now = duration_cast<Duration>(log_time.time_since_epoch());
  *sub_second_time = now.count() % period.den;
  *n_digits = std::to_string(period.den).length() - 1;
}

std::string _GetTimeString(
    const std::chrono::time_point<std::chrono::system_clock>& log_time) {
  // Format the datetime.
  char time_str_buffer[256];
  auto log_time_c = std::chrono::system_clock::to_time_t(log_time);
  std::strftime(time_str_buffer, sizeof(time_str_buffer),
                FLAGS_datetime_format.c_str(), std::localtime(&log_time_c));
  std::string time_str(time_str_buffer);

  // Extract the sub-second time, as a string.
  int sub_second_time = 0, n_digits = 0;
  if (string::ToLower(FLAGS_datetime_precision) == "ms") {
    _GetSubSecondTimeIn<std::chrono::milliseconds>(log_time, &sub_second_time,
                                                   &n_digits);
  } else if (string::ToLower(FLAGS_datetime_precision) == "us") {
    _GetSubSecondTimeIn<std::chrono::microseconds>(log_time, &sub_second_time,
                                                   &n_digits);
  } else if (string::ToLower(FLAGS_datetime_precision) == "ns") {
    _GetSubSecondTimeIn<std::chrono::nanoseconds>(log_time, &sub_second_time,
                                                  &n_digits);
  }

  if (n_digits > 0) {
    std::stringstream sub_second_time_str;
    sub_second_time_str << "." << std::setw(n_digits) << std::setfill('0')
                        << sub_second_time;
    time_str += sub_second_time_str.str();
  }

  return time_str;
}

std::string _GetFilenameToDisplay(int line, const std::string& file) {
  boost::filesystem::path file_path(file);
  std::string filename = file_path.filename().string();
  std::string line_number = std::to_string(line);

  // Pad the filename to the maximum length.
  if (filename.length() <= FLAGS_max_filename_len) {
    // Pad the filename with spaces (short files!).
    filename =
        std::string(FLAGS_max_filename_len - filename.length(), ' ') + filename;
  } else {
    // Truncate the filename (long files!). To do this, we separate the filename
    // and the extension. We always want to show the extension + the last
    // letter, and we always want to show the start (i.e. only truncate the
    // middle of the filename).
    std::string fname = file_path.stem().string();
    std::string ext = file_path.extension().string();

    // Pick how many characters to remove from the file. +1 for the extension
    // dot and +3 for the ellipse, +2 for the last 2 characters.
    int chars_left = FLAGS_max_filename_len - (ext.length() + 3 + 1 + 2);

    // If there are no characters left... then just display as many as we can.
    if (chars_left <= 0) {
      filename = fname.substr(0, FLAGS_max_filename_len);
    } else {
      std::string first_part = fname.substr(0, chars_left);
      filename =
          first_part + "..." + fname.substr(fname.length() - 2) + "." + ext;
    }
  }

  // Pad the line number to 4 characters. Because really, you shouldn't have any
  // files longer than 9999 lines, right?
  if (line_number.length() < FLAGS_max_line_number_len) {
    line_number +=
        std::string(FLAGS_max_line_number_len - line_number.length(), ' ');
  }

  return filename + ":" + line_number;
}

}  // namespace

LogMessage::LogMessage(Level level, int verbosity, int line,
                       const std::string& file, const std::string& msg_format,
                       const string::FormatListType& format_args)
    : level_(level),
      verbosity_(verbosity),
      line_(line),
      file_(boost::filesystem::path(file).filename().string()),
      log_time_(std::chrono::system_clock::now()),
      msg_format_(msg_format),
      format_args_(format_args) {}

LogMessage::LogMessage(Level level, int verbosity, int line,
                       const std::string& file, const std::string& msg_format)
    : level_(level),
      verbosity_(verbosity),
      line_(line),
      file_(boost::filesystem::path(file).filename().string()),
      log_time_(std::chrono::system_clock::now()),
      msg_format_(msg_format) {}

void LogMessage::Emit(const std::string& line_fmt) const {
  // If this message is too verbose, then just ignore it.
  if (verbosity_ > FLAGS_v) {
    return;
  }

  // Format the message.
  auto msg_formatted = string::Format(msg_format_, format_args_);

  // Format the line.
  auto line_formatted = string::FormatMap(
      line_fmt, {{"message", msg_formatted},
                 {"file", _GetFilenameToDisplay(line_, file_)},
                 {"datetime", _GetTimeString(log_time_)},
                 {"level", _LevelToString(level_)},
                 {"thread", std::this_thread::get_id()}},
      true);

  // Add colors if requested.
  std::string line_color = line_formatted;
  if (FLAGS_colorize_output) {
    line_color = string::FormatMap(line_color,
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
                                   },
                                   true);
  }

  // Output to stderr.
  if (FLAGS_logtostderr && level_ >= _StringToLevel(FLAGS_min_log_level)) {
    std::cerr << string::FormatTrimTags(line_color) << std::endl;
  }

  // Output to files.
  if (FLAGS_logtofile) {
    // Log to all of the relevant files.
    auto min_level = _StringToLevel(FLAGS_min_log_level_file);
    for (int i = min_level; i <= level_; i++) {
      auto out_file_path =
          boost::filesystem::path(FLAGS_logfile_dir) /
          (FLAGS_logfile_name + "." + _LevelToLongString((Level)i));
      std::ofstream& out_file = LOG_FILES[i];
      if (!out_file.is_open()) {
        out_file.open(out_file_path.string());
      }

      // Check to see if we need to rotate the file.
      auto file_size = boost::filesystem::file_size(out_file_path);
      if (file_size / 1024.0 / 1024.0 > FLAGS_logfile_max_size_mb) {
        // Close the file.
        out_file.close();

        // Move the file to the file + 1.
        boost::filesystem::rename(out_file_path,
                                  out_file_path.string() + ".old");
        out_file.open(out_file_path.string());
      }

      out_file << string::FormatTrimTags(line_formatted) << std::endl;
      out_file.flush();
    }
  }
}

void QueueMessage(const LogMessage& msg) {
  if (FLAGS_async_logging) {
    // Block until the message queue is a sensible size.
    while (LOG_MESSAGE_QUEUE.size() > FLAGS_async_queue_max_len) {
      ;
    }

    LOG_MESSAGE_QUEUE_INSERT_LOCK.lock();
    LOG_MESSAGE_QUEUE.push_back(msg);
    LOG_MESSAGE_QUEUE_INSERT_LOCK.unlock();
    LOG_MESSAGE_QUEUE_INSERT.notify_one();
  } else {
    _DoEmitMessage(msg);
  }

  // If the message was fatal, die.
  if (msg.level() == FATAL) {
    std::exit(EXIT_FAILURE);
  }
}

Logger::~Logger() {
  SHUTTING_DOWN = true;
  if (LOG_EMITTER != nullptr) {
    LOG_MESSAGE_QUEUE_INSERT.notify_one();
    LOG_EMITTER->join();
  }
}

}  // namespace internal

std::unique_ptr<internal::Logger> Init() {
  // Start the thread, if required.
  if (FLAGS_async_logging) {
    internal::LOG_EMITTER = new std::thread(internal::_ProcessMessageQueue);
  }

  // Make the logging output directory.
  if (FLAGS_logtofile) {
    boost::filesystem::create_directories(FLAGS_logfile_dir);

    if (FLAGS_logfile_name.empty()) {
      FLAGS_logfile_name =
          boost::filesystem::basename(gflags::ProgramInvocationName());
    }
  }

  return std::unique_ptr<internal::Logger>(new internal::Logger());
}

int MessagesInQueue() { return internal::LOG_MESSAGE_QUEUE.size(); }

}  // namespace cpplog
