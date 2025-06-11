#include <unordered_map>

#include "scanner_internal.hh"

bool Scanner::isAtEnd() { return current_offset >= source_buffer.length(); }

char Scanner::advance() {
  if (isAtEnd()) return '\0';

  char temp_char;

  temp_char = source_buffer[current_offset];

  current_offset++;
  current_column++;

  return temp_char;
}

char Scanner::peek() {
  if (isAtEnd()) return '\0';

  return source_buffer[current_offset];
}

char Scanner::peek_next() {
  if (current_offset + 1 >= source_buffer.size()) return '\0';

  return source_buffer[current_offset + 1];
}

void Scanner::skipWhitespace() {
  while (!isAtEnd()) {
    switch (peek()) {
      case ' ':
      case '\r':
      case '\t':
        advance();
        break;
      case '\n':
        return;
      default:
        return;
    }
  }
}

Scanner::Scanner(std::string_view source, std::string_view filename_)
    : filename(filename_),
      source_buffer(source),
      current_offset(0),
      current_line(1),
      current_column(1),
      current_line_offset(0) {}

LoomSourceLocation Scanner::getCurrentLocation() {
  return LoomSourceLocation(filename, current_line, current_column,
                            current_offset);
}

LoomToken Scanner::makeToken(TokenType type) {
  std::string_view token_text =
      source_buffer.substr(start_offset, current_offset - start_offset);

  size_t start_column = start_offset - current_line_offset + 1;

  LoomSourceLocation loc(filename, current_line, start_column, start_offset);

  return LoomToken(type, loc, std::string(token_text));
}

LoomToken Scanner::makeErrorToken(const std::string &message,
                                  char offending_char) {
  LoomSourceLocation loc(filename, current_line, current_column - 1,
                         current_offset - 1);

  std::string error_message = message + ": '" + offending_char + "'";

  return LoomToken(TokenType::TOKEN_ERROR, loc, error_message);
}

LoomToken Scanner::scanNumber() {
  // TODO: Check for different bases, e.g. 0x, 0b, 0o

  while (isdigit(peek())) {
    advance();
  }

  if (peek() == '.' && isdigit(peek_next())) {
    advance();

    while (isdigit(peek())) {
      advance();
    }

    return makeToken(TokenType::TOKEN_NUMBER_FLOAT);
  }

  return makeToken(TokenType::TOKEN_NUMBER_INT);
}

LoomToken Scanner::scanIdentifier() {
  while (isalnum(peek()) || peek() == '_') {
    advance();
  }

  std::string_view tmp_ident =
      source_buffer.substr(start_offset, current_offset - start_offset);
  static const std::unordered_map<std::string_view, TokenType> keywords = {
      {"let", TokenType::TOKEN_KEYWORD_LET},

  };

  auto it = keywords.find(tmp_ident);

  if (it != keywords.end()) {
    return makeToken(it->second);
  }

  return makeToken(TokenType::TOKEN_IDENTIFIER);
}

LoomToken Scanner::scanNextToken() {
  skipWhitespace();
  start_offset = current_offset;

  if (isAtEnd()) {
    return makeToken(TokenType::TOKEN_EOF);  // So einfach ist das!
  }

  char c = advance();

  if (isdigit(c)) return scanNumber();

  if (isalpha(c)) return scanIdentifier();

  switch (c) {
    case '\n': {
      LoomToken token = makeToken(TokenType::TOKEN_NEWLINE);

      current_line++;
      current_column = 1;
      current_line_offset = current_offset;
      return token;
    }

    default:

      return makeErrorToken("Unexpected character",
                            c);  // PROBLEM, DOESNT ALLOW THE SCANNER to advance
                                 // to isdigit part, how do I fux this?
  }
}

std::string Scanner::loom_toke_type_to_string(TokenType type) {
  switch (type) {
    case TokenType::TOKEN_NEWLINE:
      return "TOKEN_NEWLINE";
    case TokenType::TOKEN_EOF:
      return "TOKEN_EOF";
    case TokenType::TOKEN_NUMBER_INT:
      return "TOKEN_NUMBER_INT";
    case TokenType::TOKEN_NUMBER_FLOAT:
      return "TOKEN_NUMBER_FLOAT";
    case TokenType::TOKEN_KEYWORD_LET:
      return "TOKEN_KEYWORD_LET";
    case TokenType::TOKEN_IDENTIFIER:
      return "TOKEN_KEYWORD_IDENTIFIER";
    default:
      return "TOKEN_UNKNOWN";
  }
}