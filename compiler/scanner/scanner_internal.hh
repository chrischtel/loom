#pragma once

#include <iostream>
#include <string>
#include <vector>

enum class TokenType {
  TOKEN_WHITESPACE,
  TOKEN_NEWLINE,
  TOKEN_EOF,
};

struct LoomSourceLocation {
  int line;
  int column;

  LoomSourceLocation(int line = 1, int column = 1)
      : line(line), column(column) {}

  std::string toString() const {
    return "Line: " + std::to_string(line) +
           ", Column: " + std::to_string(column);
  }
};

struct LoomToken {
  TokenType type;
  LoomSourceLocation location;
  std::string value;

  LoomToken(TokenType type = TokenType::TOKEN_EOF,
            LoomSourceLocation location = LoomSourceLocation{},
            std::string value = "")
      : type(type), location(location), value(value) {}
};

class Scanner {
private:
  std::string_view filename;
  std::string_view source_buffer;

  size_t current_offset;

  size_t current_line;
  size_t current_column;
  size_t current_line_offset;

public:
  Scanner(std::string_view source, std::string_view filename_ = "");
};
