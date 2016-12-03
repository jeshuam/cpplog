#include "log.h"
#include "format.h"

#include <stdarg.h>
#include <ctime>

#include <iostream>
#include <iomanip>
#include <fstream>

#include <boost/filesystem.hpp>
#include <gflags/gflags.h>

// Basic flags.
DEFINE_string(log_level, "info", "Minimum log level to display.");

// Output control flags.
DEFINE_bool(logtostdout, false, "Whether to log information to stdout or not.");
DEFINE_bool(scoped_logging, false, "Whether or not to enable scoped logging.");
DEFINE_string(
    log_fmt,
    "{nc}{lc}{level}{nc} {bold}{white}@{nc} {gray}{datetime}{nc} "
    ": {white}{italic}{file}{nc} {bold}{white}::{nc} {lc}{indent}{message}{nc}",
    "A Python-ish format string of the log header information.");

// Log file control flags.
DEFINE_string(log_file, "", "File to send log information to.");
DEFINE_int32(log_max_size_mb, 100,
              "Maximum size of the log file generated in MB.");
DEFINE_double(log_file_rotation_threshold, 0.95,
              "Point at which the log file will be rotated. This is to ensure "
              "that the maximum log file size isn't exceeded. Valid values "
              "should be between 0.5 and 0.95.");

// Advanced flags.
DEFINE_int32(
    max_filename_len, 20,
    "Maximum length of the filenames to display in the log. All "
    "filenames will either be padded to this value, or truncated. "
    "Note that this length does not include the path or the line number.");

DEFINE_int32(max_line_number_len, 4,
           "Maximum number of line number characters to print.");

DEFINE_int32(max_formatted_log_message_len, 1024,
             "The maximum length (in bytes) of the formatting log message. "
             "This will influence the size of the log buffer used. This "
             "should only be reduced (or increased) in special cases where "
             "only short messages are logged (and memory is scarce) or very "
             "very long log messages are logged.");

DEFINE_int32(scoped_logging_indent, 2,
             "Number of spaces to indent scopped logging messages.");

DEFINE_string(datetime_fmt, "%a %b %d %T",
              "The format of the datetime to display to the screen. This only "
              "includes the > second components of the time. The global date "
              "format is controlled through --log_header_fmt.");

DEFINE_string(
    datetime_precision, "ms",
    "Precision of the time to display. Should be either one of 's' "
    "(seconds), 'ms' (milliseconds), 'us' (microseconds) or 'ns' "
    "(nanoseconds). This will adjust the log format so that the required "
    "precision is displayed (if supported by the system).");

DEFINE_bool(log_async, true,
            "Log messages to the file/screen asynchronously.");

namespace util {
namespace log {

// Log static members.
Log::Level Log::_min_log_level = INFO;
std::vector<Log::LogMessageTuple>* Log::_pre_init_messages = nullptr;
std::ofstream Log::_output_file;

// OPTIMIZATION: don't actually log messages directly, just add them to a queue
// and have a thread which does logging async. Order will be preserved, and
// times will be correct, but the time that they are emitted may be some time
// after they were logged.
std::list<Log::LogMessageTuple> Log::_log_queue;
std::mutex Log::_log_queue_lock;
std::thread* Log::_log_queue_worker;
std::condition_variable Log::_log_queue_notify;
bool Log::_log_queue_finished = false;

// Number of indents for scoped logging.
int _scoped_level = 0;

// Precision of the datetime to display.
enum { S, MS, US, NS } _datetime_precision_den;

template <class Duration = std::chrono::milliseconds>
static void GetSubSecondTimeIn(int* sub_second_time, int* n_digits) {
  using namespace std::chrono;
  typename Duration::period period;

  auto now = duration_cast<Duration>(system_clock::now().time_since_epoch());
  *sub_second_time = now.count() % period.den;
  *n_digits = std::to_string(period.den).length() - 1;
}

static std::string GetSubSecondTimeAsString() {
  using namespace std::chrono;

  int sub_second_time = 0, n_digits = 0;
  switch (_datetime_precision_den) {
    case S:
      return "";
    case MS:
      GetSubSecondTimeIn<milliseconds>(&sub_second_time, &n_digits);
      break;
    case US:
      GetSubSecondTimeIn<microseconds>(&sub_second_time, &n_digits);
      break;
    case NS:
      GetSubSecondTimeIn<nanoseconds>(&sub_second_time, &n_digits);
      break;
  }

  std::stringstream time_str;
  time_str << "." << std::setw(n_digits) << std::setfill('0')
           << sub_second_time;
  return time_str.str();
}

bool Log::IsReadyToLog() {
  return gflags::ProgramInvocationName() != "UNKNOWN";
}

void Log::Finish() {
  if (FLAGS_log_async) {
    _log_queue_finished = true;
    _log_queue_notify.notify_one();

    if (_log_queue_worker != nullptr) {
      _log_queue_worker->join();
    }
  }
}

void Log::SetMinLogLevel(Level level) { Log::_min_log_level = level; }

Log::Level Log::StringToLogLevel(const std::string& level) {
  std::string level_lower = level;
  std::transform(level_lower.begin(), level_lower.end(), level_lower.begin(),
                 ::tolower);
  if (level_lower == "trace") {
    return TRACE;
  } else if (level_lower == "debug") {
    return DEBUG;
  } else if (level_lower == "info") {
    return INFO;
  } else if (level_lower == "warning") {
    return WARNING;
  } else if (level_lower == "error") {
    return ERROR;
  } else if (level_lower == "fatal") {
    return FATAL;
  }

  throw new std::invalid_argument("Unknown log level");
}

bool Log::IsAnyOutputActive() {
  return FLAGS_log_file.length() > 0 || FLAGS_logtostdout;
}

// Format-style user-facing logging methods.
void Log::EmitFormat(Log::Level level, int line, const char* file,
                     const char* fmt, ...) {
  if (level < Log::_min_log_level) return;

  // If logging is disabled... Why bother doing anything?
  if (IsReadyToLog() && !IsAnyOutputActive()) {
    return;
  }

  // Load the args.
  va_list args;
  va_start(args, fmt);
  char* buffer = new char[FLAGS_max_formatted_log_message_len];
  vsnprintf(buffer, FLAGS_max_formatted_log_message_len, fmt, args);
  Emit(level, line, file, buffer);
  delete[] buffer;
}

void Log::Emit(Log::Level level, int line, const char* file,
               const std::string& message) {
  if (level < Log::_min_log_level) return;

  // If logging is disabled... Why bother doing anything?
  if (IsReadyToLog() && !IsAnyOutputActive()) {
    return;
  }

  // Generate the main log message. This will not substitute the color tags yet.

  // If we aren't ready to log, then save the log the message.
  if (!Log::IsReadyToLog()) {
    _SaveMessage(level, line, file, message);
  } else if (FLAGS_log_async) {
    _EmitSavedMessages();
    _QueueMessage(level, line, file, message);
  } else {
    _EmitSavedMessages();
    _DoEmit(level, _GenerateLogMessage(level, line, file, message));
  }
}

void Log::_DoEmit(Level level, const std::string& message) {
  _OpenOutputFileIfNecessary();
  if (Log::_output_file.is_open()) {
    Log::_output_file << FormatEraseTags(message) << std::endl;

    _RotateLogFileIfNecessary();
  }

  // Write the output to the display.
  if (FLAGS_logtostdout) {
    std::string message_c = Format(message, {{"lc", _GetLevelColor(level)}});
    std::cout << Format(message_c, ColorMapping) << std::endl;
  }

  // If this was a fatal log message, die now.
  if (level == FATAL) {
    throw std::runtime_error("fatal log message emitted");
  }
}

void Log::_SaveMessage(Level level, int line, const char* file,
                       const std::string& message) {
  if (_pre_init_messages == nullptr) {
    _pre_init_messages = new std::vector<LogMessageTuple>();
  }

  _pre_init_messages->push_back(std::make_tuple(level, line, file, message));
}

void Log::_QueueMessage(Level level, int line, const char* file,
                        const std::string& message) {
  // Start the thread which will constantly log messages.
  if (FLAGS_log_async && _log_queue_worker == nullptr) {
    _log_queue_worker = new std::thread(Log::_ProcessQueuedMessages);
  }

  _log_queue_lock.lock();
  _log_queue.push_back(std::make_tuple(level, line, file, message));
  _log_queue_lock.unlock();
  _log_queue_notify.notify_one();
}

void Log::_ProcessQueuedMessages() {
  std::mutex mutex;
  std::unique_lock<std::mutex> lock(mutex);
  while (!_log_queue_finished) {
    // Wait until we have been notified. Only wake up if there is something in
    // the log queue.
    _log_queue_notify.wait(lock, []{ return _log_queue.size() > 0; });

    // Process everything in the log queue.
    while (!_log_queue.empty()) {
      Level level;
      int line;
      const char* file;
      std::string content;

      // Lock the container.
      _log_queue_lock.lock();
      std::tie(level, line, file, content) = _log_queue.front();
      _log_queue.pop_front();
      _log_queue_lock.unlock();
      _DoEmit(level, _GenerateLogMessage(level, line, file, content));
    }
  }
}

void Log::_EmitSavedMessages() {
  if (_pre_init_messages == nullptr) {
    return;
  }

  // Emit a log message to say we've started!
  _DoEmit(INFO, _GenerateLogMessage(INFO, __LINE__, __FILE__,
                                    "Logging system initialized"));

  for (const auto& message : *_pre_init_messages) {
    Level level;
    int line;
    const char* file;
    std::string content;
    std::tie(level, line, file, content) = message;
    _DoEmit(level, _GenerateLogMessage(level, line, file, content));
  }

  delete _pre_init_messages;
  _pre_init_messages = nullptr;
}

// Internal logging methods.
std::string Log::_GetFilenameToDisplay(int line, const char* file) {
  // Calculate the actual filename and line number.
  boost::filesystem::path file_path(file);
  std::string filename = file_path.filename().string();
  std::string line_number = std::to_string(line);

  // Pad the filename to the maximum length.
  if (filename.length() <= FLAGS_max_filename_len) {
    // Pad the filename with spaces (short files!).
    filename = std::string(
        FLAGS_max_filename_len - filename.length(), ' ') + filename;
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

std::string Log::_GenerateLogMessage(Log::Level level, int line,
                                     const char* file,
                                     const std::string& message) {
  std::unordered_map<std::string, std::string> params;
  if (FormatHasTag(FLAGS_log_fmt, "level")) {
    params["level"] = _GetLevelString(level);
  }

  if (FormatHasTag(FLAGS_log_fmt, "datetime")) {
    params["datetime"] = _GetDatetimeString();
  }

  if (FormatHasTag(FLAGS_log_fmt, "file")) {
    params["file"] = _GetFilenameToDisplay(line, file);
  }

  if (FormatHasTag(FLAGS_log_fmt, "message")) {
    params["message"] = message;
  }

  if (FormatHasTag(FLAGS_log_fmt, "indent")) {
    params["indent"] =
        std::string(_scoped_level * FLAGS_scoped_logging_indent, ' ');
  }

  return Format(FLAGS_log_fmt, params);
}

std::string Log::_GetLevelString(Log::Level level) {
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

std::string Log::_GetLevelColor(Log::Level level) {
  switch (level) {
    case TRACE:
    case DEBUG:
      return Gray;
    case INFO:
#ifdef __linux__
      return Blue + Bold;
#elif _WIN32
      return Cyan + Bold;
#endif
    case WARNING:
      return Yellow + Bold;
    case ERROR:
    case FATAL:
      return Red + Bold;
    default:
      return "";
  }
}

std::string Log::_GetDatetimeString() {
  using namespace std::chrono;

  // Use std::time to get the actual time.
  char datetime_buffer[1024];
  std::time_t t = std::time(nullptr);
  std::strftime(datetime_buffer, sizeof(datetime_buffer),
                FLAGS_datetime_fmt.c_str(), std::localtime(&t));

  // Get the number of ns since the epoch.
  std::string sub_second_time = GetSubSecondTimeAsString();

  // Include the us.
  std::stringstream datetime;
  datetime << datetime_buffer << sub_second_time;
  return datetime.str();
}

void Log::_OpenOutputFileIfNecessary() {
  if (FLAGS_log_file.length() > 0 && !Log::_output_file.is_open()) {
    // Make sure the directory exists.
    boost::filesystem::path log_file_path(FLAGS_log_file);
    boost::filesystem::create_directories(log_file_path.parent_path());

    // Open the file for writing.
    Log::_output_file.open(FLAGS_log_file);
  }
}

void Log::_RotateLogFileIfNecessary() {
  // Check whether we have to rotate.
  boost::filesystem::path log_file_path(FLAGS_log_file);
  double size_mb = boost::filesystem::file_size(log_file_path) / 1024.0 / 1024.0;
  if (size_mb < FLAGS_log_max_size_mb * FLAGS_log_file_rotation_threshold) {
    return;
  }

  // Rotate the log! First close it.
  Log::_output_file.close();

  // Move the file.
  boost::filesystem::path new_log_file_path(FLAGS_log_file + ".1");
  boost::filesystem::rename(log_file_path, new_log_file_path);
  Log::_output_file.open(FLAGS_log_file);
}

/**
 * Scoped logging.
 */
ScopedLog::ScopedLog(Log::Level level, const std::string& name, int line,
                     const char* file)
    : _level(level), _name(name), _file(file), _line(line) {
  if (FLAGS_scoped_logging && _level >= Log::MinLogLevel()) {
    Log::EmitFormat(_level, _line, _file.c_str(), "+ %s", _name.c_str());
    _scoped_level++;
  }
}

ScopedLog::~ScopedLog() {
  if (FLAGS_scoped_logging && _level >= Log::MinLogLevel()) {
    _scoped_level--;
    Log::EmitFormat(_level, _line, _file.c_str(), "- %s", _name.c_str());
  }
}

}  // namespace log
}  // namespace util
