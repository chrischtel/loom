// main.cc

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "parser/ast_printer.hh"
#include "parser/parser_internal.hh"
#include "scanner/scanner_internal.hh"
#include "sema/semantic_analyzer.hh"

std::string readFile(const std::string& filename) {
  std::ifstream file(filename);
  if (!file.is_open()) {
    std::cerr << "Error: Could not open file '" << filename << "'" << std::endl;
    return "";
  }

  std::stringstream buffer;
  buffer << file.rdbuf();
  return buffer.str();
}

int main(int argc, char* argv[]) {
  // Check if filename was provided
  std::string filename;
  std::string source_code;

  if (argc < 2) {
    // No file provided, use default test code
    std::cout << "No file provided, using default test code." << std::endl;
    filename = "inline_test.loom";
    source_code = "let answer = 42;";
  } else {
    // Read from provided file
    filename = argv[1];
    source_code = readFile(filename);

    if (source_code.empty()) {
      std::cerr << "Failed to read file or file is empty." << std::endl;
      return 1;
    }
  }

  std::cout << "Compiling file: " << filename << std::endl;
  std::cout << "Source code: \"" << source_code << "\"" << std::endl;
  std::cout << "========================================" << std::endl;
  // --- PHASE 1: SCANNING ---
  std::cout << "--- Running Scanner ---" << std::endl;
  Scanner scanner(source_code, filename);

  std::vector<LoomToken> tokens;
  for (;;) {
    LoomToken token = scanner.scanNextToken();
    std::cout << "Scanned: " << scanner.loom_toke_type_to_string(token.type)
              << " ('" << token.value << "')" << std::endl;

    tokens.push_back(token);

    if (token.type == TokenType::TOKEN_EOF) {
      break;
    }
  }
  std::cout << "--- Scanner Finished ---" << std::endl << std::endl;

  std::cout << "--- Running Parser ---" << std::endl;
  Parser parser(tokens);

  std::vector<std::unique_ptr<StmtNode>> ast = parser.parse();
  if (!parser.hasError()) {
    std::cout << std::endl << "--- Running Semantic Analyzer ---" << std::endl;
    SemanticAnalyzer sema;
    sema.analyze(ast);  // Rufe die Analyse auf dem AST auf

    if (!sema.hasError()) {
      std::cout << "Semantic analysis successful!" << std::endl;
      // Gib den AST aus, um zu sehen, dass alles noch da ist
      std::cout << "--- Abstract Syntax Tree ---" << std::endl;
      ASTPrinter printer;
      printer.print(ast);
    } else {
      std::cout << "Semantic analysis failed!" << std::endl;
    }
    std::cout << "--- Semantic Analyzer Finished ---" << std::endl;
  } else {
    std::cout << "Parse failed! Skipping subsequent stages." << std::endl;
  }

  return 0;
}