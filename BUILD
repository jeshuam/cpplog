log: {
  type: c++/library
  srcs: ["log.cc", "format.cc"]
  hdrs: ["log.h", "format.h"]
  deps: [
    "//third_party/boost/filesystem",
    "//third_party/gflags",
  ]
}

speed_test: {
  type: c++/binary
  srcs: ["log_speed_test.cc"]
  deps: [":log"]
}
