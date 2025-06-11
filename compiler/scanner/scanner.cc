#include "scanner_internal.hh"

bool Scanner::isAtEnd() { return current_offset >= source_buffer.length(); }

char Scanner::advance() {
  if (isAtEnd())
    return '\0';

  char temp_char;

  temp_char = source_buffer[current_offset];

  current_offset++;
  current_column++;

  return temp_char;
}