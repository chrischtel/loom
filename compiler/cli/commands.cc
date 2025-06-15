#include "commands.hh"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

#include "../codegen/codegen.hh"
#include "../parser/ast_printer.hh"
#include "../parser/parser_internal.hh"
#include "../scanner/scanner_internal.hh"
#include "../sema/semantic_analyzer.hh"

#ifdef _WIN32
#include <process.h>  // For Windows _spawnv
#endif

namespace loom {

// Utility functions
namespace {

std::string readFile(const std::string& filename) {
  std::ifstream file(filename);
  if (!file.is_open()) {
    throw std::runtime_error("Could not open file: " + filename);
  }

  std::stringstream buffer;
  buffer << file.rdbuf();
  return buffer.str();
}

void logMessage(VerbosityLevel level, VerbosityLevel required,
                const std::string& message) {
  if (level >= required) {
    std::cout << message << std::endl;
  }
}

void logError(const std::string& message) {
  std::cerr << "Error: " << message << std::endl;
}

[[maybe_unused]] void logWarning(const std::string& message) {
  std::cerr << "Warning: " << message << std::endl;
}

std::string generateOutputFilename(const std::string& input,
                                   const std::string& extension) {
  std::filesystem::path input_path(input);
  std::string output = input_path.stem().string() + extension;
  return output;
}

bool compileFile(const std::string& filename, const CompilerOptions& options) {
  auto start_time = std::chrono::high_resolution_clock::now();

  try {
    // Read source code
    std::string source_code = readFile(filename);

    logMessage(options.verbosity, VerbosityLevel::VERBOSE,
               "Compiling file: " + filename);

    // Phase 1: Scanning
    logMessage(options.verbosity, VerbosityLevel::DEBUG,
               "--- Running Scanner ---");

    Scanner scanner(source_code, filename);
    std::vector<LoomToken> tokens;

    for (;;) {
      LoomToken token = scanner.scanNextToken();

      if (options.show_tokens) {
        std::cout << "Token: " << scanner.loom_toke_type_to_string(token.type)
                  << " ('" << token.value << "')" << std::endl;
      }

      tokens.push_back(token);

      if (token.type == TokenType::TOKEN_EOF) {
        break;
      }
    }

    logMessage(options.verbosity, VerbosityLevel::DEBUG,
               "Scanner completed successfully");

    // Phase 2: Parsing
    logMessage(options.verbosity, VerbosityLevel::DEBUG,
               "--- Running Parser ---");

    Parser parser(tokens);
    std::vector<std::unique_ptr<StmtNode>> ast = parser.parse();

    if (parser.hasError()) {
      logError("Parser failed");
      return false;
    }

    if (options.show_ast) {
      std::cout << "=== Abstract Syntax Tree ===" << std::endl;
      for (const auto& stmt : ast) {
        std::cout << stmt->toString() << std::endl;
      }
      std::cout << "=============================" << std::endl;
    }

    logMessage(options.verbosity, VerbosityLevel::DEBUG,
               "Parser completed successfully");

    // Phase 3: Semantic Analysis
    logMessage(options.verbosity, VerbosityLevel::DEBUG,
               "--- Running Semantic Analyzer ---");

    SemanticAnalyzer sema;
    sema.analyze(ast);

    if (sema.hasError()) {
      logError("Semantic analysis failed");
      return false;
    }

    if (options.show_semantic_info) {
      std::cout << "=== Semantic Analysis Info ===" << std::endl;
      std::cout << "Semantic analysis completed successfully" << std::endl;
      std::cout << "===============================" << std::endl;
    }

    logMessage(options.verbosity, VerbosityLevel::DEBUG,
               "Semantic analysis completed successfully");

    // For check-only mode, stop here
    if (options.check_only) {
      logMessage(options.verbosity, VerbosityLevel::NORMAL,
                 "Check completed successfully");
      return true;
    }    // Phase 4: Code Generation
    logMessage(options.verbosity, VerbosityLevel::DEBUG,
               "--- Running Code Generator ---");

    CodeGen code_generator(options.verbosity);
    code_generator.generate(ast);

    if (options.emit_llvm_ir) {
      std::string ir_filename = generateOutputFilename(filename, ".ll");
      std::cout << "=== Generated LLVM IR ===" << std::endl;
      code_generator.print_ir();
      std::cout << "=========================" << std::endl;

      // Save IR to file
      std::ofstream ir_file(ir_filename);
      if (ir_file.is_open()) {
        code_generator.write_ir_to_stream(ir_file);
        logMessage(options.verbosity, VerbosityLevel::NORMAL,
                   "LLVM IR written to: " + ir_filename);
      }
    }

    logMessage(options.verbosity, VerbosityLevel::DEBUG,
               "Code generation completed successfully");

    // Phase 5: Compilation to Executable
    if (!options.no_link) {
      logMessage(options.verbosity, VerbosityLevel::DEBUG,
                 "--- Compiling to Executable ---");

      // Generate output filename
      std::string output_name = options.output_file;
      if (output_name.empty()) {
        output_name = generateOutputFilename(filename, ".exe");
      }

      // Ensure output directory exists
      std::filesystem::path output_path(output_name);
      if (output_path.has_parent_path()) {
        std::filesystem::create_directories(output_path.parent_path());
      }

      // Initialize LLVM targets
      if (!code_generator.initializeLLVMTargets()) {
        logError("Failed to initialize LLVM targets");
        return false;
      }

      // Generate object file
      std::string object_filename = output_name + ".o";
      if (!code_generator.compileToObjectFile(object_filename)) {
        logError("Failed to generate object file");
        return false;
      }

      // Link to executable
      if (!code_generator.compileToExecutable(object_filename, output_name)) {
        logError("Failed to link executable");
        return false;
      }

      // Clean up object file
      std::filesystem::remove(object_filename);

      logMessage(options.verbosity, VerbosityLevel::NORMAL,
                 "Successfully compiled to: " + output_name);
    }

    // Benchmark compilation time
    if (options.benchmark_compilation) {
      auto end_time = std::chrono::high_resolution_clock::now();
      auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
          end_time - start_time);
      std::cout << "Compilation time: " << duration.count() << "ms"
                << std::endl;
    }

    return true;

  } catch (const std::exception& e) {
    logError(std::string("Compilation failed: ") + e.what());
    return false;
  }
}

}  // anonymous namespace

// Build Command Implementation
int BuildCommand::execute(const CompilerOptions& options) {
  if (options.input_file.empty()) {
    logError("No input file specified for build command");
    return 1;
  }

  if (!std::filesystem::exists(options.input_file)) {
    logError("Input file does not exist: " + options.input_file);
    return 1;
  }

  bool success = compileFile(options.input_file, options);
  return success ? 0 : 1;
}

// Run Command Implementation
int RunCommand::execute(const CompilerOptions& options) {
  if (options.input_file.empty()) {
    logError("No input file specified for run command");
    return 1;
  }

  // First, build the program
  CompilerOptions build_options = options;
  build_options.run_after_build = false;  // Avoid infinite recursion

  BuildCommand build_cmd;
  int build_result = build_cmd.execute(build_options);

  if (build_result != 0) {
    logError("Build failed, cannot run program");
    return build_result;
  }

  // Determine executable name
  std::string exe_name = options.output_file;
  if (exe_name.empty()) {
    exe_name = generateOutputFilename(options.input_file, ".exe");
  }

  // Run the executable
  logMessage(options.verbosity, VerbosityLevel::VERBOSE,
             "Running: " + exe_name);

#ifdef _WIN32
  int result = _spawnl(_P_WAIT, exe_name.c_str(), exe_name.c_str(), nullptr);
#else
  int result = system(("./" + exe_name).c_str());
#endif

  if (result == -1) {
    logError("Failed to execute program: " + exe_name);
    return 1;
  }

  return result;
}

// Check Command Implementation
int CheckCommand::execute(const CompilerOptions& options) {
  if (options.input_file.empty()) {
    logError("No input file specified for check command");
    return 1;
  }

  CompilerOptions check_options = options;
  check_options.check_only = true;

  // Check if input is a directory
  if (std::filesystem::is_directory(options.input_file)) {
    bool all_success = true;

    for (const auto& entry :
         std::filesystem::recursive_directory_iterator(options.input_file)) {
      if (entry.is_regular_file() && entry.path().extension() == ".loom") {
        check_options.input_file = entry.path().string();
        bool success = compileFile(check_options.input_file, check_options);
        all_success &= success;
      }
    }

    return all_success ? 0 : 1;
  } else {
    // Single file
    bool success = compileFile(options.input_file, check_options);
    return success ? 0 : 1;
  }
}

// Clean Command Implementation
int CleanCommand::execute(const CompilerOptions& options) {
  std::vector<std::string> patterns = {"*.exe", "*.o", "*.ll", "*.s", "*.obj"};

  std::string clean_dir = options.output_dir;
  if (clean_dir.empty()) {
    clean_dir = "./";
  }

  int cleaned_count = 0;

  try {
    for (const auto& entry : std::filesystem::directory_iterator(clean_dir)) {
      if (entry.is_regular_file()) {
        std::string filename = entry.path().filename().string();
        std::string extension = entry.path().extension().string();

        // Check if file matches any clean pattern
        if (extension == ".exe" || extension == ".o" || extension == ".ll" ||
            extension == ".s" || extension == ".obj") {
          logMessage(options.verbosity, VerbosityLevel::VERBOSE,
                     "Removing: " + filename);

          std::filesystem::remove(entry.path());
          cleaned_count++;
        }
      }
    }

    logMessage(options.verbosity, VerbosityLevel::NORMAL,
               "Cleaned " + std::to_string(cleaned_count) + " files");

  } catch (const std::exception& e) {
    logError("Failed to clean directory: " + std::string(e.what()));
    return 1;
  }

  return 0;
}

// Info Command Implementation
int InfoCommand::execute(const CompilerOptions& options) {
  std::cout << "Loom Compiler Information" << std::endl;
  std::cout << "=========================" << std::endl;
  std::cout << "Version: 0.1.0-alpha" << std::endl;
  std::cout << "Built with: LLVM" << std::endl;

  std::cout << "Host platform: ";
#ifdef _WIN32
  std::cout << "Windows";
#elif defined(__linux__)
  std::cout << "Linux";
#elif defined(__APPLE__)
  std::cout << "macOS";
#else
  std::cout << "Unknown";
#endif
  std::cout << std::endl;

  std::cout << "Host architecture: ";
#if defined(_M_X64) || defined(__x86_64__)
  std::cout << "x86_64";
#elif defined(_M_ARM64) || defined(__aarch64__)
  std::cout << "ARM64";
#else
  std::cout << "Unknown";
#endif
  std::cout << std::endl;

  std::cout << "Default target: ";
  switch (options.target_platform) {
    case TargetPlatform::WINDOWS:
      std::cout << "Windows";
      break;
    case TargetPlatform::LINUX:
      std::cout << "Linux";
      break;
    case TargetPlatform::MACOS:
      std::cout << "macOS";
      break;
    case TargetPlatform::WASM:
      std::cout << "WebAssembly";
      break;
  }
  std::cout << " ";
  switch (options.target_arch) {
    case TargetArch::X86_64:
      std::cout << "x86_64";
      break;
    case TargetArch::ARM64:
      std::cout << "ARM64";
      break;
    case TargetArch::WASM32:
      std::cout << "WASM32";
      break;
  }
  std::cout << std::endl;

  std::cout << "Build mode: ";
  switch (options.build_mode) {
    case BuildMode::DEBUG:
      std::cout << "Debug";
      break;
    case BuildMode::RELEASE:
      std::cout << "Release";
      break;
    case BuildMode::PROFILE:
      std::cout << "Profile";
      break;
  }
  std::cout << std::endl;

  if (!options.input_file.empty() &&
      std::filesystem::exists(options.input_file)) {
    std::cout << std::endl << "Project Information" << std::endl;
    std::cout << "===================" << std::endl;
    std::cout << "Input file: " << options.input_file << std::endl;
    auto file_size = std::filesystem::file_size(options.input_file);
    std::cout << "File size: " << file_size << " bytes" << std::endl;    // Note: std::format may not be available in all compilers yet
    (void)std::filesystem::last_write_time(options.input_file); // Suppress unused warning
    std::cout << "Last modified: [timestamp]" << std::endl;
  }

  return 0;
}

// Format Command Implementation (stub)
int FormatCommand::execute(const CompilerOptions& /*options*/) {
  logError("Format command not yet implemented");
  return 1;
}

// Test Command Implementation (stub)
int TestCommand::execute(const CompilerOptions& /*options*/) {
  logError("Test command not yet implemented");
  return 1;
}

// New Command Implementation (stub)
int NewCommand::execute(const CompilerOptions& /*options*/) {
  logError("New command not yet implemented");
  return 1;
}

// Factory function
std::vector<std::unique_ptr<Command>> createStandardCommands() {
  std::vector<std::unique_ptr<Command>> commands;

  commands.emplace_back(std::make_unique<BuildCommand>());
  commands.emplace_back(std::make_unique<RunCommand>());
  commands.emplace_back(std::make_unique<CheckCommand>());
  commands.emplace_back(std::make_unique<CleanCommand>());
  commands.emplace_back(std::make_unique<InfoCommand>());
  commands.emplace_back(std::make_unique<FormatCommand>());
  commands.emplace_back(std::make_unique<TestCommand>());
  commands.emplace_back(std::make_unique<NewCommand>());

  return commands;
}

}  // namespace loom
