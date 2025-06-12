// main.cc

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "codegen/codegen.hh"
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

  // --- PHASE 3: SEMANTIC ANALYSIS ---
  if (!parser.hasError()) {
    std::cout << std::endl << "--- Running Semantic Analyzer ---" << std::endl;
    SemanticAnalyzer sema;
    sema.analyze(ast);

    if (!sema.hasError()) {
      std::cout << "Semantic analysis successful!"
                << std::endl;  // --- PHASE 4: CODE GENERATION (NEU) ---
      std::cout << std::endl << "--- Running Code Generator ---" << std::endl;
      CodeGen code_generator;
      code_generator.generate(ast);

      std::cout << "--- Generated LLVM IR ---" << std::endl;
      code_generator.print_ir();
      std::cout << "-------------------------"
                << std::endl;  // --- PHASE 5: COMPILE TO EXECUTABLE (INTEGRATED
                               // LLVM APPROACH) ---
      std::cout << std::endl << "--- Compiling to Executable ---" << std::endl;

      // Generate output filename (replace .loom with .exe)
      std::string output_name = filename;
      size_t last_dot = output_name.find_last_of('.');
      if (last_dot != std::string::npos) {
        output_name = output_name.substr(0, last_dot);
      }
      output_name += ".exe";

      // Initialize LLVM targets for object file generation
      if (!code_generator.initializeLLVMTargets()) {
        std::cerr << "Error: Failed to initialize LLVM targets" << std::endl;
        return 1;
      }

      // Generate object file directly using LLVM
      std::string object_filename = output_name + ".o";
      if (!code_generator.compileToObjectFile(object_filename)) {
        std::cerr << "Error: Failed to generate object file" << std::endl;
        return 1;
      }

      // Link object file to executable
      if (!code_generator.compileToExecutable(object_filename, output_name)) {
        std::cerr << "Error: Failed to link executable" << std::endl;
        return 1;
      }

      // Clean up object file
      std::filesystem::remove(object_filename);
      std::cout << "Cleaned up temporary object file." << std::endl;

      std::cout << "Successfully compiled to: " << output_name << std::endl;

    } else {
      std::cout << "Semantic analysis failed!" << std::endl;
    }
    std::cout << "--- Semantic Analyzer Finished ---" << std::endl;
  }
  return 0;
}
