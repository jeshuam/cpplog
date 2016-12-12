log: {
  type: c++/library
  srcs: ["log.cc"]
  hdrs: ["log.h"]
  deps: [
    "//third_party/boost/filesystem",
    "//third_party/gflags",
    "//util/string",
  ]
}

speed_test: {
  type: c++/binary
  srcs: ["log_speed_test.cc"]
  deps: ["//:log"]
}
