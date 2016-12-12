#include "log.h"

#include <gflags/gflags.h>

#include <chrono>

DECLARE_bool(logtostdout);
DECLARE_string(log_fmt);
DECLARE_string(log_file);
DECLARE_bool(log_async);

using namespace std::chrono;

DEFINE_int32(test, 1, "The test to run.");
DEFINE_int32(n, 10000, "Number of log messages to emit.");

void TestLoggingWhenLoggingDisabled() {
  FLAGS_logtostdout = false;
  auto start_log = system_clock::now();
  for (int i = 0; i < FLAGS_n; i++) {
    LOG_INFO("Test 1");
  }
  auto end_log = system_clock::now();
  FLAGS_logtostdout = true;

  auto start_clean = system_clock::now();
  for (int i = 0; i < FLAGS_n; i++) {
    ;  // do nothing
  }
  auto end_clean = system_clock::now();

  // Calculate time of each segment.
  auto log_time = duration_cast<microseconds>(end_log - start_log);
  auto clean_time = duration_cast<microseconds>(end_clean - start_clean);

  // Print some stats.
  unsigned int log_time_int = log_time.count(),
               clean_time_int = clean_time.count();
  LOG_INFO("Time with LOG_INFO(): %dms (%.2fns per LOG_INFO())", log_time_int,
           double(log_time_int) / FLAGS_n * 1000);
  LOG_INFO("Time with    nothing: %dms (%.2fns per loop)", clean_time_int,
           double(clean_time_int) / FLAGS_n * 1000);
}

void TestLoggingComparedToPrintf() {
  // FLAGS_log_file = "log/log.txt";
  auto start_log = system_clock::now();
  for (int i = 0; i < FLAGS_n; i++) {
    LOG_INFO("Test 2");
  }
  auto end_log = system_clock::now();

  auto start_clean = system_clock::now();
  for (int i = 0; i < FLAGS_n; i++) {
    printf("Test 2\n");
  }
  auto end_clean = system_clock::now();

  // // Calculate time of each segment.
  auto log_time = duration_cast<milliseconds>(end_log - start_log);
  auto clean_time = duration_cast<milliseconds>(end_clean - start_clean);

  // Print some stats.
  unsigned int log_time_int = log_time.count(),
               clean_time_int = clean_time.count();
  LOG_INFO("Time with LOG_INFO(): %dms (%.2fns per LOG_INFO())", log_time_int,
           double(log_time_int) / FLAGS_n * 1000);
  LOG_INFO("Time with   printf(): %dms (%.2fns per printf())", clean_time_int,
           double(clean_time_int) / FLAGS_n * 1000);
}

void TestLoggingComparedToPrintfWithSimpleFormat() {
  std::string old_header_fmt = FLAGS_log_fmt;
  FLAGS_log_fmt = "{message}";
  auto start_log = system_clock::now();
  for (int i = 0; i < FLAGS_n; i++) {
    LOG_INFO("Test 3");
  }
  auto end_log = system_clock::now();

  auto start_clean = system_clock::now();
  for (int i = 0; i < FLAGS_n; i++) {
    printf("Test 3\n");
  }
  auto end_clean = system_clock::now();

  // Calculate time of each segment.
  auto log_time = duration_cast<milliseconds>(end_log - start_log);
  auto clean_time = duration_cast<milliseconds>(end_clean - start_clean);

  // Print some stats.
  unsigned int log_time_int = log_time.count(),
               clean_time_int = clean_time.count();
  LOG_INFO("Time with LOG_INFO(): %dms (%.2fns per LOG_INFO())", log_time_int,
           double(log_time_int) / FLAGS_n * 1000);
  LOG_INFO("Time with   printf(): %dms (%.2fns per printf())", clean_time_int,
           double(clean_time_int) / FLAGS_n * 1000);
}

int main(int argc, char** argv) {
  gflags::ParseCommandLineFlags(&argc, &argv, true);

  FLAGS_logtostdout = true;
  LOG_INFO("Starting speed tests!");

  if (FLAGS_test == 1) {
    LOG_INFO(
        "1. How does LOG_INFO() compare to an empty loop when logging "
        "disabled?");
    TestLoggingWhenLoggingDisabled();
  } else if (FLAGS_test == 2) {
    LOG_INFO("2. How does LOG_INFO() compare to printf()?");
    TestLoggingComparedToPrintf();
  } else if (FLAGS_test == 3) {
    LOG_INFO(
        "3. How does LOG_INFO() with a simple format compare to printf()?");
    TestLoggingComparedToPrintfWithSimpleFormat();
  } else {
    LOG_FATAL("Invalid test id %s", 3);
  }

  util::log::Log::Finish();
}
