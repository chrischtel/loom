#pragma once

#include <iostream>
#include <string>
#include <vector>

enum class TokenType {
  TOKEN_WHITESPACE,
  TOKEN_NEWLINE,
  TOKEN_EOF,
  // 1-char tokens
  TOKEN_SEMICOLON,
  TOKEN_COLON,
  TOKEN_COMMA,
  TOKEN_EQUAL,
  TOKEN_EQUAL_EQUAL,
  TOKEN_LESS,
  TOKEN_LESS_EQUAL,
  TOKEN_GREATER,
  TOKEN_GREATER_EQUAL,
  TOKEN_SLASH,
  TOKEN_PLUS,
  TOKEN_MINUS,
  TOKEN_STAR,
  TOKEN_LEFT_PAREN,
  TOKEN_RIGHT_PAREN,
  TOKEN_LEFT_BRACE,
  TOKEN_RIGHT_BRACE,
  TOKEN_LEFT_BRACKET,   // [
  TOKEN_RIGHT_BRACKET,  // ]
  TOKEN_BANG,

  // Memory operators
  TOKEN_AMPERSAND,  // & (reference)
  TOKEN_HAT,        // ^ (owned pointer)
  TOKEN_QUESTION,   // ? (nullable)
  TOKEN_ARROW,      // -> (pointer access)
  TOKEN_DOT,        // . (member access)
  TOKEN_DOT_DOT,    // .. (slice range)

  // Literals
  TOKEN_NUMBER_INT,
  TOKEN_NUMBER_FLOAT,
  TOKEN_STRING,
  // Identifiers
  TOKEN_IDENTIFIER,
  TOKEN_BUILTIN,  // $$builtin functions

  // Keywords
  TOKEN_KEYWORD_LET,     // unmutable variabiable declaration
  TOKEN_KEYWORD_MUT,     // mutable variabiable declaration
  TOKEN_KEYWORD_DEFINE,  // compile time known constant declaration
  TOKEN_KEYWORD_FUNC,
  TOKEN_KEYWORD_IF,    // if statement
  TOKEN_KEYWORD_ELSE,  // else statement
  TOKEN_KEYWORD_TRUE,
  TOKEN_KEYWORD_FALSE,
  TOKEN_KEYWORD_WHILE,
  TOKEN_KEYWORD_RETURN,
  TOKEN_KEYWORD_DEFER,   // defer statement
  TOKEN_KEYWORD_UNSAFE,  // unsafe block
  TOKEN_KEYWORD_STATIC,  // static allocation
  TOKEN_KEYWORD_NULL,    // null literal

  // Specials
  TOKEN_ERROR
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
  char peek_next_next();
  void skipWhitespace();
  LoomToken scanMultiLineComment();
  bool match(char expected);

  LoomSourceLocation getCurrentLocation();
  LoomToken makeToken(TokenType type);
  LoomToken makeErrorToken(const std::string &message, char offending_char);
  LoomToken scanNumber();
  LoomToken scanIdentifier();
  LoomToken scanString();
  LoomToken scanBuiltin();
};
