[![Build Status](https://travis-ci.org/jeshuam/cpplog.svg?branch=master)](https://travis-ci.org/jeshuam/cpplog)
[![Documentation](https://codedocs.xyz/jeshuam/cpplog.svg)](https://codedocs.xyz/jeshuam/cpplog/)

# cpplog: A C++ logging library
`cpplog` is a small, simple C++ string library. See the Doxygen documentation for more details.

## How to use
Logging is mostly done using macros. There are 6 levels in `cpplog`:

- Trace (very spammy messages useful for tracing crashes)
- Debug (messages useful for debugging, but too spammy for normal operation)
- Info (messages which are nice to know, but require no action)
- Warning (something weird went wrong, but it might not be an error)
- Error (something went wrong, but it was recoverable)
- Fatal (something went wrong, and it was not recoverable)

All messages will just print, but Fatal messages will terminate the program immediately and should be used with care.

There are 5 main logging functions:

- `LOG_INFO` takes as input a C-style format string with argv args.
- `LOG_INFO_STREAM` allows you to use C++-style streams to log messages.
- `LOG_INFO_SCOPED` will indent all log messages while the scope it was created in exists.
- `LOG_INFO_EVERY` will log a message at least some delay apart.
- `LOG_INFO_STREAM_EVERY` is a stream version of `LOG_INFO_EVERY`.

You can replace `INFO` with one of the other levels (e.g. `TRACE`, `FATAL` etc.) to log other levels.

Some examples:

```c++
int main() {
  LOG_INFO("Starting!");
  
  std::string message = "log message!"
  LOG_DEBUG_STREAM(message << ", " << message);
  
  {
    LOG_INFO_SCOPED();
    LOG_WARNING("Something went wrong! %d", 1);
  }
  
  while (for int i = 0; i < 100000000; i++) {
    LOG_ERROR_EVERY(10ms, "Oh no: %s", "error");
  }
}
```
