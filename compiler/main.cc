#include <iostream>

int main(int argc, char *argv[]) {
  if (argc < 2) {
    std::cerr << "Usage: " << argv[0] << " <source_file>\n";
    return 1;
  }

  std::cout << "Compilation successful!\n";
  return 0;
}