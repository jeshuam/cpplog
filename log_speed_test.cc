#include "log.h"

#include <gflags/gflags.h>

#include <chrono>

DECLARE_bool(logtostdout);
DECLARE_string(log_fmt);

using namespace std::chrono;

void TestLoggingWhenLoggingDisabled() {
  int n_loops = 100000;

  FLAGS_logtostdout = false;
  auto start_log = system_clock::now();
  for (int i = 0; i < n_loops; i++) {
    LOG_INFO("Test 1");
  }
  auto end_log = system_clock::now();
  FLAGS_logtostdout = true;

  auto start_clean = system_clock::now();
  for (int i = 0; i < n_loops; i++) {
    ;  // do nothing
  }
  auto end_clean = system_clock::now();

  // Calculate time of each segment.
  auto log_time = duration_cast<microseconds>(end_log - start_log);
  auto clean_time = duration_cast<microseconds>(end_clean - start_clean);

  // Print some stats.
  LOG_INFO("Time with LOG_INFO(): %dus", log_time.count());
  LOG_INFO("Time with    nothing: %dus", clean_time.count());

  // Get some stats.
  unsigned int extra_time = (log_time - clean_time).count();
  LOG_INFO("%uus extra spent logging, or %.2fns per LOG_INFO()", extra_time,
           double(extra_time) / n_loops * 1000);
}

void TestLoggingComparedToPrintf() {
  int n_loops = 10000;

  auto start_log = system_clock::now();
  for (int i = 0; i < n_loops; i++) {
    LOG_INFO("Test 2");
  }
  auto end_log = system_clock::now();

  auto start_clean = system_clock::now();
  for (int i = 0; i < n_loops; i++) {
    printf("Test 2\n");
  }
  auto end_clean = system_clock::now();

  // Calculate time of each segment.
  auto log_time = duration_cast<milliseconds>(end_log - start_log);
  auto clean_time = duration_cast<milliseconds>(end_clean - start_clean);

  // Print some stats.
  LOG_INFO("Time with LOG_INFO(): %dms", log_time.count());
  LOG_INFO("Time with    nothing: %dms", clean_time.count());

  // Get some stats.
  unsigned int extra_time = (log_time - clean_time).count();
  LOG_INFO("%dms extra spent logging, or %.2fus per LOG_INFO()", extra_time,
           double(extra_time) / n_loops * 1000);
}

void TestLoggingComparedToPrintfWithSimpleFormat() {
  int n_loops = 100000;

  std::string old_header_fmt = FLAGS_log_fmt;
  FLAGS_log_fmt = "{message}";
  auto start_log = system_clock::now();
  for (int i = 0; i < n_loops; i++) {
    LOG_INFO("Test 3");
  }
  auto end_log = system_clock::now();
  FLAGS_log_fmt = old_header_fmt;

  auto start_clean = system_clock::now();
  for (int i = 0; i < n_loops; i++) {
    printf("Test 3\n");
  }
  auto end_clean = system_clock::now();

  // Calculate time of each segment.
  auto log_time = duration_cast<milliseconds>(end_log - start_log);
  auto clean_time = duration_cast<milliseconds>(end_clean - start_clean);

  // Print some stats.
  LOG_INFO("Time with LOG_INFO(): %dms", log_time.count());
  LOG_INFO("Time with    nothing: %dms", clean_time.count());

  // Get some stats.
  unsigned int extra_time = (log_time - clean_time).count();
  LOG_INFO("%dms extra spent logging, or %.2fus per LOG_INFO()", extra_time,
           double(extra_time) / n_loops * 1000);
}

int main(int argc, char** argv) {
  gflags::ParseCommandLineFlags(&argc, &argv, true);

  FLAGS_logtostdout = true;
  LOG_INFO("Starting speed tests!");

  // LOG_INFO(
  //     "1. How does LOG_INFO() compare to an empty loop when logging disabled?");
  // TestLoggingWhenLoggingDisabled();

  // LOG_INFO("2. How does LOG_INFO() compare to printf()?");
  // TestLoggingComparedToPrintf();

  // LOG_INFO("3. How does LOG_INFO() compare to printf() with a simple format?");
  // TestLoggingComparedToPrintfWithSimpleFormat();
}
