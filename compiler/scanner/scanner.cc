#include "scanner_internal.hh"

bool Scanner::isAtEnd() { return current_offset >= source_buffer.length(); }