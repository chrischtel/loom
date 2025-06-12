#include <unordered_map>

#include "scanner_internal.hh"

static const std::unordered_map<std::string_view, TokenType> keywords = {
    {"let", TokenType::TOKEN_KEYWORD_LET},
    {"mut", TokenType::TOKEN_KEYWORD_MUT},
    {"define", TokenType::TOKEN_KEYWORD_DEFINE},
    {"true", TokenType::TOKEN_KEYWORD_TRUE},
    {"false", TokenType::TOKEN_KEYWORD_FALSE},
};

bool Scanner::isAtEnd() { return current_offset >= source_buffer.length(); }

char Scanner::advance() {
  if (isAtEnd()) return '\0';

  char temp_char;

  temp_char = source_buffer[current_offset];

  current_offset++;
  current_column++;

  return temp_char;
}

bool Scanner::match(char expected) {
  if (isAtEnd()) return false;

  if (peek() != expected) return false;

  advance();
  return true;
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

LoomToken Scanner::makeErrorToken(const std::string& message,
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

  auto it = keywords.find(tmp_ident);

  if (it != keywords.end()) {
    return makeToken(it->second);
  }

  return makeToken(TokenType::TOKEN_IDENTIFIER);
}

LoomToken Scanner::scanString() {
  // Der öffnende " wurde bereits verbraucht
  while (peek() != '"' && !isAtEnd()) {
    if (peek() == '\n') {
      current_line++;
      current_column = 1;
      current_line_offset = current_offset;
    }
    advance();
  }

  if (isAtEnd()) {
    return makeErrorToken("Unterminated string", '"');
  }

  // Schließende " verbrauchen
  advance();

  return makeToken(TokenType::TOKEN_STRING);
}

LoomToken Scanner::scanNextToken() {
  skipWhitespace();
  start_offset = current_offset;

  if (isAtEnd()) {
    return makeToken(TokenType::TOKEN_EOF);
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
    case '=':
      return (makeToken(match('=') ? TokenType::TOKEN_EQUAL_EQUAL
                                   : TokenType::TOKEN_EQUAL));
    case ';':
      return makeToken(TokenType::TOKEN_SEMICOLON);
    case ':':
      return makeToken(TokenType::TOKEN_COLON);
    case '+':
      return makeToken(TokenType::TOKEN_PLUS);
    case '-':
      return makeToken(TokenType::TOKEN_MINUS);
    case '*':
      return makeToken(TokenType::TOKEN_STAR);
    case '(':
      return makeToken(TokenType::TOKEN_LEFT_PAREN);
    case '!':
      return makeToken(TokenType::TOKEN_BANG);
    case ')':
      return makeToken(TokenType::TOKEN_RIGHT_PAREN);
    case '{':
      return makeToken(TokenType::TOKEN_LEFT_BRACE);
    case '}':
      return makeToken(TokenType::TOKEN_RIGHT_BRACE);
    case '"':
      return scanString();

    case '/':
      if (match('/')) {
        // Kommentar bis zum Zeilenende überspringen
        while (peek() != '\n' && !isAtEnd()) {
          advance();
        }
        // Rekursiv nächstes Token scannen (Kommentar überspringen)
        return scanNextToken();
      } else {
        return makeToken(TokenType::TOKEN_SLASH);
      }

    default:
      return makeErrorToken("Unexpected character", c);
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
    case TokenType::TOKEN_STRING:
      return "TOKEN_STRING";
    case TokenType::TOKEN_IDENTIFIER:
      return "TOKEN_IDENTIFIER";
    case TokenType::TOKEN_KEYWORD_LET:
      return "TOKEN_KEYWORD_LET";
    case TokenType::TOKEN_KEYWORD_MUT:
      return "TOKEN_KEYWORD_MUT";
    case TokenType::TOKEN_KEYWORD_DEFINE:
      return "TOKEN_KEYWORD_DEFINE";
    case TokenType::TOKEN_KEYWORD_TRUE:
      return "TOKEN_KEYWORD_DEFINE";
    case TokenType::TOKEN_KEYWORD_FALSE:
      return "TOKEN_KEYWORD_FALSE";
    case TokenType::TOKEN_SEMICOLON:
      return "TOKEN_SEMICOLON";
    case TokenType::TOKEN_COLON:
      return "TOKEN_COLON";
    case TokenType::TOKEN_EQUAL:
      return "TOKEN_EQUAL";
    case TokenType::TOKEN_EQUAL_EQUAL:
      return "TOKEN_EQUAL_EQUAL";
    case TokenType::TOKEN_SLASH:
      return "TOKEN_SLASH";
    case TokenType::TOKEN_PLUS:
      return "TOKEN_PLUS";
    case TokenType::TOKEN_MINUS:
      return "TOKEN_MINUS";
    case TokenType::TOKEN_STAR:
      return "TOKEN_STAR";
    case TokenType::TOKEN_LEFT_PAREN:
      return "TOKEN_LEFT_PAREN";
    case TokenType::TOKEN_RIGHT_PAREN:
      return "TOKEN_RIGHT_PAREN";
    case TokenType::TOKEN_LEFT_BRACE:
      return "TOKEN_LEFT_BRACE";
    case TokenType::TOKEN_RIGHT_BRACE:
      return "TOKEN_RIGHT_BRACE";
    case TokenType::TOKEN_BANG:
      return "TOKEN_BANG";
    case TokenType::TOKEN_ERROR:
      return "TOKEN_ERROR";
    default:
      return "TOKEN_UNKNOWN";
  }
}