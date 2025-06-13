#pragma once

#include <iostream>
#include <string>

enum class LogLevel { ERROR = 0, WARN = 1, INFO = 2, DEBUG = 3 };

class Logger {
 private:
  static LogLevel current_level;

 public:
  static void setLevel(LogLevel level) { current_level = level; }

  static void error(const std::string& message) {
    if (current_level >= LogLevel::ERROR) {
      std::cerr << "Error: " << message << std::endl;
    }
  }

  static void warn(const std::string& message) {
    if (current_level >= LogLevel::WARN) {
      std::cerr << "Warning: " << message << std::endl;
    }
  }

  static void info(const std::string& message) {
    if (current_level >= LogLevel::INFO) {
      std::cout << message << std::endl;
    }
  }

  static void debug(const std::string& message) {
    if (current_level >= LogLevel::DEBUG) {
      std::cout << "[DEBUG] " << message << std::endl;
    }
  }
};
