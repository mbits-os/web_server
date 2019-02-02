#pragma once

#include <sstream>

class logging {
 public:
  enum level {
    debug,
    warning,
    error
  };

  logging(level lvl, int verbosity, const char* file, int line)
      : lvl_{lvl}, verbosity_{verbosity}, file_{file}, line_{line} {}

  ~logging() { output(); }

  template <typename T>
  logging& operator<<(T&& obj) {
    if (lvl_ == debug && verbosity_ > verbose_)
      return *this;

    dirty_ = true;
    os_ << std::forward<T>(obj);
    return *this;
  }

  static int verbose() { return verbose_; }
  static void verbose(int value) { verbose_ = value; }

 private:
  std::string init_line(bool with_colors);
  void output();
  void output(const std::string& line);
  level lvl_;
  int verbosity_;
  const char* file_;
  int line_;
  std::ostringstream os_;
  bool dirty_{false};
  static int verbose_;
};

#define LOG(Level, Verbose) \
  ::logging { ::logging::Level, Verbose, __FILE__, __LINE__ }

#define LOG_ERR() LOG(error, 0)
#define LOG_WRN() LOG(warning, 0)
#define LOG_DBG(Verbose) LOG(debug, Verbose)
#define LOG_NFO() LOG_DBG(0)
#define LOG_DBG1() LOG_DBG(1)
#define LOG_DBG2() LOG_DBG(2)
#define LOG_DBG3() LOG_DBG(3)
