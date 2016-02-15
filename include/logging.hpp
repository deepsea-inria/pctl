/* COPYRIGHT (c) 2015 Umut Acar, Arthur Chargueraud, and Michael
 * Rainey
 * All rights reserved.
 *
 * \file logging.hpp
 * \brief Granularity-controlled parallel primitives
 *
 */

#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include "perworker.hpp"

#ifndef _PCTL_LOGGING_
#define _PCTL_LOGGING_

namespace pasl {
namespace pctl {
namespace logging {

typedef enum {
  FORK,
  ESTIM_NAME,
  ESTIM_PREDICT,
  ESTIM_REPORT,
  ESTIM_UPDATE,
  ESTIM_UPDATE_SHARED
} event_type;

static inline std::string name_of(event_type type) {
  switch (type) {
    case FORK:                return std::string("estim_fork          ");
    case ESTIM_NAME:          return std::string("estim_name          %s");
    case ESTIM_PREDICT:       return std::string("estim_predict       %s %f %f %f"); // name complexity prediction constant
    case ESTIM_REPORT:        return std::string("estim_report        %s %f %f %f"); // name complexity estimated estimated/complexity
    case ESTIM_UPDATE:        return std::string("estim_update        %s %f"); // name constant
    case ESTIM_UPDATE_SHARED: return std::string("estim_update_shared %s %f"); // name constant
    default: assert(false);
  }
}

pasl::pctl::perworker::array<std::vector<std::string>, pasl::pctl::perworker::get_my_id> buffers;

namespace {
template <typename ... Args>
std::string printf(const std::string& format, Args ... args) {
  size_t size = snprintf(nullptr, 0, format.c_str(), args ...) + 1; // length of string + '\0'
  std::unique_ptr<char[]> buf(new char[size]);
  snprintf(buf.get(), size, format.c_str(), args ...);
  return std::string(buf.get(), buf.get() + size - 1);
}
}

template <typename ... Args>
void log(event_type type, Args ... args) {
  std::string format = std::string("%ld %d ") + name_of(type);
  auto now = std::chrono::system_clock::now();
  auto now_milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
  std::string message = printf(format, now_milliseconds, buffers.get_my_id(), args ...);
  buffers.mine().push_back(message);
}

void init() {

}

void dump() {
  std::ofstream log_file;
  log_file.open("log.txt");
  buffers.iterate([&] (std::vector<std::string>& events) {
    for (auto value : events) {
      log_file << value << "\n";
    }
  });
  log_file.close();
}


} // end namespace
} // end namespace
} // end namespace
#endif