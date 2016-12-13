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

log_new: {
  type: c++/binary
  srcs: ["log_new.cc"]
  hdrs: ["log_new.h"]
  deps: [
    "//util/string",
    "//third_party/gflags",
  ]
}
