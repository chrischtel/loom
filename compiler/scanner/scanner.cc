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

char Scanner::peek() {
  if (isAtEnd())
    return '\0';

  return source_buffer[current_offset];
}

char Scanner::peek_next() {
  if (current_offset + 1 >= source_buffer.size())
    return '\0';

  return source_buffer[current_offset + 1];
}

Scanner::Scanner(std::string_view source, std::string_view filename_)
    : filename(filename_), source_buffer(source), current_offset(0),
      current_line(1), current_column(1), current_line_offset(0) {}