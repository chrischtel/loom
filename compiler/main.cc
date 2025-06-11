// main.cc

#include <iostream>

#include "scanner/scanner_internal.hh"  // Passe den Pfad ggf. an

int main() {
  // 1. Unser Beispiel-Quellcode. Er enthält Leerzeichen und einen
  // Zeilenumbruch.
  const char *source_code = "\n\n 4.343 let myvar = 7647; define PI = 3.141";
  std::cout << "Scanning source code: \"\\n  \"" << std::endl;
  std::cout << "--------------------------------" << std::endl;

  // 2. Erstelle den Scanner
  Scanner scanner(source_code, "test.loom");

  // 3. Die Haupt-Testschleife
  // for (;;) ist eine C++-Kurzform für eine Endlosschleife (wie while(true))
  for (;;) {
    LoomToken token = scanner.scanNextToken();

    // 4. Gib die Token-Informationen aus
    std::cout << "Type: " << scanner.loom_toke_type_to_string(token.type)
              << "\t Location: " << token.location.toString() << "\t Value: '"
              << token.value << "'" << std::endl;

    // 5. Die Abbruchbedingung
    if (token.type == TokenType::TOKEN_EOF ||
        token.type == TokenType::TOKEN_ERROR) {
      break;  // Verlasse die Schleife bei EOF ODER bei einem Fehler
    }
  }

  std::cout << "--------------------------------" << std::endl;
  std::cout << "Scanning finished." << std::endl;

  return 0;
}