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
  std::string_view filename;
  size_t line;
  size_t column;
  size_t offset;

  LoomSourceLocation(std::string_view filename, size_t line = 1,
                     size_t column = 1, size_t offset = 0)
      : filename(filename), line(line), column(column), offset(offset) {}

  std::string toString() const {
    return "Line: " + std::to_string(line) +
           ", Column: " + std::to_string(column);
  }
};

struct LoomToken {
  TokenType type;
  LoomSourceLocation location;
  std::string value;

  LoomToken(TokenType type, LoomSourceLocation location, std::string value)
      : type(type), location(location), value(value) {}
};

class Scanner {
private:
  std::string_view filename;
  std::string_view source_buffer;

  size_t current_offset;
  size_t start_offset;

  size_t current_line;
  size_t current_column;
  size_t current_line_offset;

public:
  Scanner(std::string_view source, std::string_view filename_ = "");

  // Scans and returns the next token of the source
  LoomToken scanNextToken();

  // i guess that would be helpful for debugging
  std::string loom_toke_type_to_string(TokenType type);

private:
  // Check if the scanner is at EOF
  bool isAtEnd();
  char advance();
  char peek();
  char peek_next();
  void skipWhitespace();
  LoomSourceLocation getCurrentLocation();
  LoomToken makeToken(TokenType type);
};
