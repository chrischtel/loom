// parser.cc
#include <iostream>

#include "parser_internal.hh"

Parser::Parser(const std::vector<LoomToken>& tokens)
    : tokens(tokens), had_error(false) {}

bool Parser::isAtEnd() const { return peek().type == TokenType::TOKEN_EOF; }

const LoomToken& Parser::peek() const { return tokens[current]; }

const LoomToken& Parser::previous() const { return tokens[current - 1]; }

void Parser::advance() {
  if (!isAtEnd()) {
    current++;
  }
}

bool Parser::check(TokenType type) const {
  if (isAtEnd()) return false;
  return peek().type == type;
}

bool Parser::match(TokenType type) {
  if (check(type)) {
    advance();
    return true;
  }
  return false;
}

void Parser::consume(TokenType type, const std::string& message) {
  if (check(type)) {
    advance();
    return;
  }
  error(peek(), message);
}

void Parser::error(const LoomToken& token, const std::string& message) {
  had_error = true;
  std::cerr << "Parse error at " << token.location.toString() << ": " << message
            << std::endl;
  throw ParseError(message);
}

void Parser::synchronize() {
  advance();
  while (!isAtEnd()) {
    if (previous().type == TokenType::TOKEN_SEMICOLON) return;
    switch (peek().type) {
      case TokenType::TOKEN_KEYWORD_LET:
      case TokenType::TOKEN_KEYWORD_MUT:
      case TokenType::TOKEN_KEYWORD_DEFINE:
        return;
      default:
        break;
    }
    advance();
  }
}

// Die Haupt-parse()-Schleife gehört auch hierher.
std::vector<std::unique_ptr<StmtNode>> Parser::parse() {
  std::vector<std::unique_ptr<StmtNode>> statements;
  while (!isAtEnd()) {
    if (match(TokenType::TOKEN_NEWLINE)) {
      continue;
    }

    auto decl = parseDeclaration();
    if (decl) {
      statements.push_back(std::move(decl));
    }
  }
  return statements;
}