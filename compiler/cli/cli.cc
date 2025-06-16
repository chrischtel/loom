#include "cli.hh"

#include <algorithm>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <sstream>

#include "common/version.hh"

namespace loom {

CommandLineInterface::CommandLineInterface() { addGlobalFlags(); }

CommandLineInterface::~CommandLineInterface() = default;

void CommandLineInterface::registerCommand(std::unique_ptr<Command> command) {
  std::string name = command->getName();
  commands[name] = std::move(command);
}

void CommandLineInterface::addFlag(const Flag& flag) { flags.push_back(flag); }

void CommandLineInterface::addGlobalFlags() {
  // Help and version
  addFlag(Flag("h", "help", "Show help information", false,
               [](CompilerOptions& opts, const std::string&) {
                 opts.show_help = true;
               }));

  addFlag(Flag("v", "version", "Show version information", false,
               [](CompilerOptions& opts, const std::string&) {
                 opts.show_version = true;
               }));

  // Output options
  addFlag(Flag("o", "output", "Specify output file", true,
               [](CompilerOptions& opts, const std::string& value) {
                 opts.output_file = value;
               }));

  addFlag(Flag("", "output-dir", "Specify output directory", true,
               [](CompilerOptions& opts, const std::string& value) {
                 opts.output_dir = value;
               }));

  // Build mode
  addFlag(Flag("", "debug", "Build in debug mode", false,
               [](CompilerOptions& opts, const std::string&) {
                 opts.build_mode = BuildMode::DEBUG;
               }));

  addFlag(Flag("", "release", "Build in release mode", false,
               [](CompilerOptions& opts, const std::string&) {
                 opts.build_mode = BuildMode::RELEASE;
               }));

  addFlag(Flag("", "profile", "Build in profile mode", false,
               [](CompilerOptions& opts, const std::string&) {
                 opts.build_mode = BuildMode::PROFILE;
               }));

  // Optimization
  addFlag(Flag(
      "O", "optimize", "Optimization level (0-3)", true,
      [](CompilerOptions& opts, const std::string& value) {
        try {
          opts.optimization_level = std::stoi(value);
          if (opts.optimization_level < 0 || opts.optimization_level > 3) {
            std::cerr << "Warning: Optimization level should be 0-3, got "
                      << value << std::endl;
            opts.optimization_level = std::clamp(opts.optimization_level, 0, 3);
          }
        } catch (const std::exception&) {
          std::cerr << "Error: Invalid optimization level: " << value
                    << std::endl;
          opts.optimization_level = 0;
        }
      }));

  addFlag(Flag("", "lto", "Enable link-time optimization", false,
               [](CompilerOptions& opts, const std::string&) {
                 opts.enable_lto = true;
               }));

  // Verbosity
  addFlag(Flag("q", "quiet", "Quiet output (errors only)", false,
               [](CompilerOptions& opts, const std::string&) {
                 opts.verbosity = VerbosityLevel::QUIET;
               }));

  addFlag(Flag("", "verbose", "Verbose output", false,
               [](CompilerOptions& opts, const std::string&) {
                 opts.verbosity = VerbosityLevel::VERBOSE;
               }));

  addFlag(Flag("", "debug-output", "Debug output", false,
               [](CompilerOptions& opts, const std::string&) {
                 opts.verbosity = VerbosityLevel::DEBUG;
               }));

  // Target options
  addFlag(Flag("", "target", "Target platform (windows|linux|macos|wasm)", true,
               [](CompilerOptions& opts, const std::string& value) {
                 if (value == "windows")
                   opts.target_platform = TargetPlatform::WINDOWS;
                 else if (value == "linux")
                   opts.target_platform = TargetPlatform::LINUX;
                 else if (value == "macos")
                   opts.target_platform = TargetPlatform::MACOS;
                 else if (value == "wasm")
                   opts.target_platform = TargetPlatform::WASM;
                 else
                   std::cerr << "Warning: Unknown target platform: " << value
                             << std::endl;
               }));

  addFlag(Flag("", "arch", "Target architecture (x86_64|arm64|wasm32)", true,
               [](CompilerOptions& opts, const std::string& value) {
                 if (value == "x86_64")
                   opts.target_arch = TargetArch::X86_64;
                 else if (value == "arm64")
                   opts.target_arch = TargetArch::ARM64;
                 else if (value == "wasm32")
                   opts.target_arch = TargetArch::WASM32;
                 else
                   std::cerr
                       << "Warning: Unknown target architecture: " << value
                       << std::endl;
               }));

  // Emission options
  addFlag(Flag("", "emit-llvm", "Emit LLVM IR", false,
               [](CompilerOptions& opts, const std::string&) {
                 opts.emit_llvm_ir = true;
               }));

  addFlag(Flag("", "emit-asm", "Emit assembly", false,
               [](CompilerOptions& opts, const std::string&) {
                 opts.emit_assembly = true;
               }));

  addFlag(Flag(
      "c", "compile-only", "Compile only, don't link", false,
      [](CompilerOptions& opts, const std::string&) { opts.no_link = true; }));

  addFlag(Flag("r", "run", "Run executable after successful build", false,
               [](CompilerOptions& opts, const std::string&) {
                 opts.run_after_build = true;
               }));

  // Debug options
  addFlag(Flag("g", "debug-info", "Generate debug information", false,
               [](CompilerOptions& opts, const std::string&) {
                 opts.generate_debug_info = true;
                 if (opts.build_mode == BuildMode::RELEASE) {
                   opts.build_mode = BuildMode::DEBUG;
                 }
               }));

  addFlag(Flag("", "no-debug", "Disable debug information", false,
               [](CompilerOptions& opts, const std::string&) {
                 opts.generate_debug_info = false;
               }));

  // Warning options
  addFlag(Flag("W", "warn", "Enable specific warning", true,
               [](CompilerOptions& opts, const std::string& value) {
                 opts.enabled_warnings.push_back(value);
               }));

  addFlag(Flag("", "Werror", "Treat warnings as errors", false,
               [](CompilerOptions& opts, const std::string&) {
                 opts.warnings_as_errors = true;
               }));

  addFlag(Flag("", "no-warn", "Disable all warnings", false,
               [](CompilerOptions& opts, const std::string&) {
                 opts.disable_warnings = true;
               }));

  // Development and debugging
  addFlag(Flag(
      "", "show-ast", "Show abstract syntax tree", false,
      [](CompilerOptions& opts, const std::string&) { opts.show_ast = true; }));

  addFlag(Flag("", "show-tokens", "Show lexer tokens", false,
               [](CompilerOptions& opts, const std::string&) {
                 opts.show_tokens = true;
               }));

  addFlag(Flag("", "show-sema", "Show semantic analysis info", false,
               [](CompilerOptions& opts, const std::string&) {
                 opts.show_semantic_info = true;
               }));

  addFlag(Flag("", "benchmark", "Benchmark compilation time", false,
               [](CompilerOptions& opts, const std::string&) {
                 opts.benchmark_compilation = true;
               }));

  // Include and library paths
  addFlag(Flag("I", "include", "Add include directory", true,
               [](CompilerOptions& opts, const std::string& value) {
                 opts.include_paths.push_back(value);
               }));

  addFlag(Flag("L", "library-path", "Add library directory", true,
               [](CompilerOptions& opts, const std::string& value) {
                 opts.library_paths.push_back(value);
               }));

  addFlag(Flag("l", "library", "Link with library", true,
               [](CompilerOptions& opts, const std::string& value) {
                 opts.libraries.push_back(value);
               }));

  // Config file
  addFlag(Flag("", "config", "Use configuration file", true,
               [](CompilerOptions& opts, const std::string& value) {
                 opts.config_file = value;
               }));
}

int CommandLineInterface::parseAndExecute(int argc, char* argv[]) {
  if (argc < 1) return 1;

  executable_name = std::filesystem::path(argv[0]).filename().string();

  // Parse flags and find command
  int command_index = 0;
  CompilerOptions options = parseFlags(argc, argv, command_index);
  // Handle global options first
  if (options.show_help) {
    if (command_index > 0 && command_index < argc) {
      showCommandHelp(argv[command_index]);
    } else {
      showHelp();
    }
    return 0;
  }

  if (options.show_version) {
    showVersion();
    return 0;
  }
  // Find and execute command
  std::string command_name;
  if (command_index > 0 && command_index < argc) {
    command_name = argv[command_index];
  } else {
    // No command specified, default to 'build' if input file is provided
    if (!options.input_file.empty()) {
      command_name = "build";
    } else {
      std::cerr << "Error: No command or input file specified.\n";
      std::cerr << "Use '" << executable_name
                << " --help' for usage information.\n";
      return 1;
    }
  }

  auto cmd_it = commands.find(command_name);
  if (cmd_it == commands.end()) {
    std::cerr << "Error: Unknown command '" << command_name << "'\n";
    std::cerr << "Use '" << executable_name
              << " --help' for available commands.\n";
    return 1;
  }

  try {
    return cmd_it->second->execute(options);
  } catch (const std::exception& e) {
    if (options.verbosity >= VerbosityLevel::NORMAL) {
      std::cerr << "Error: " << e.what() << std::endl;
    }
    return 1;
  }
}

CompilerOptions CommandLineInterface::parseFlags(int argc, char* argv[],
                                                 int& command_index) {
  CompilerOptions options;
  bool found_command = false;

  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];

    if (!isFlag(arg)) {
      // Non-flag argument
      if (!found_command) {
        // Check if this is a known command
        if (commands.find(arg) != commands.end()) {
          command_index = i;
          found_command = true;
          continue;
        }
        // Otherwise, treat as input file
        if (options.input_file.empty() && arg.find('.') != std::string::npos) {
          options.input_file = arg;
          continue;
        }
      } else {
        // After command, treat as input file or command argument
        if (options.input_file.empty() && arg.find('.') != std::string::npos) {
          options.input_file = arg;
          continue;
        }
      }
      // If we get here, it's an unknown argument - skip it
      continue;
    }

    // Parse flag
    std::string flag_name;
    bool is_short = false;
    if (arg.length() > 2 && arg.substr(0, 2) == "--") {
      flag_name = getLongFlag(arg);
    } else if (arg.length() > 1 && arg[0] == '-' && arg.substr(0, 2) != "--") {
      flag_name = getShortFlag(arg);
      is_short = true;
    } else {
      continue;
    }

    // Find matching flag
    auto flag_it = std::find_if(flags.begin(), flags.end(), [&](const Flag& f) {
      return (is_short && f.short_name == flag_name) ||
             (!is_short && f.long_name == flag_name);
    });

    if (flag_it == flags.end()) {
      std::cerr << "Warning: Unknown flag: " << arg << std::endl;
      continue;
    }

    std::string value;
    if (flag_it->has_value) {
      if (i + 1 >= argc) {
        std::cerr << "Error: Flag " << arg << " requires a value" << std::endl;
        continue;
      }
      value = argv[++i];
    }

    flag_it->handler(options, value);
  }

  return options;
}

std::string CommandLineInterface::findCommand(int argc, char* argv[]) const {
  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (!isFlag(arg) && commands.find(arg) != commands.end()) {
      return arg;
    }
  }
  return "";
}

bool CommandLineInterface::isFlag(const std::string& arg) const {
  return arg.length() > 1 && arg[0] == '-';
}

std::string CommandLineInterface::getShortFlag(const std::string& arg) const {
  if (arg.length() >= 2 && arg[0] == '-' && arg.substr(0, 2) != "--") {
    return arg.substr(1, 1);
  }
  return "";
}

std::string CommandLineInterface::getLongFlag(const std::string& arg) const {
  if (arg.length() > 2 && arg.substr(0, 2) == "--") {
    size_t eq_pos = arg.find('=');
    if (eq_pos != std::string::npos) {
      return arg.substr(2, eq_pos - 2);
    }
    return arg.substr(2);
  }
  return "";
}

void CommandLineInterface::showHelp() const {
  std::cout << "Loom Programming Language Compiler\n\n";
  std::cout << "USAGE:\n";
  std::cout << "    " << executable_name << " [OPTIONS] <COMMAND> [ARGS...]\n";
  std::cout << "    " << executable_name << " [OPTIONS] <FILE>\n\n";

  std::cout << "COMMANDS:\n";
  for (const auto& [name, cmd] : commands) {
    std::cout << "    " << std::left << std::setw(12) << name
              << cmd->getDescription() << "\n";
  }

  std::cout << "\nGLOBAL OPTIONS:\n";
  for (const auto& flag : flags) {
    std::string flag_text;
    if (!flag.short_name.empty()) {
      flag_text = "-" + flag.short_name;
      if (!flag.long_name.empty()) {
        flag_text += ", --" + flag.long_name;
      }
    } else {
      flag_text = "    --" + flag.long_name;
    }

    if (flag.has_value) {
      flag_text += " <VALUE>";
    }

    std::cout << "    " << std::left << std::setw(20) << flag_text
              << flag.description << "\n";
  }

  std::cout << "\nEXAMPLES:\n";
  std::cout << "    " << executable_name << " build main.loom\n";
  std::cout << "    " << executable_name << " run --release main.loom\n";
  std::cout << "    " << executable_name << " check --verbose src/\n";
  std::cout << "    " << executable_name << " --help build\n";
}

void CommandLineInterface::showVersion() const {
  std::cout << "Loom Programming Language Compiler\n";
  std::cout << "Version: " << version::VERSION << "\n";
  std::cout << "Built with LLVM support\n";
  std::cout << "Target: ";

#ifdef _WIN32
  std::cout << "Windows";
#elif defined(__linux__)
  std::cout << "Linux";
#elif defined(__APPLE__)
  std::cout << "macOS";
#else
  std::cout << "Unknown";
#endif

#if defined(_M_X64) || defined(__x86_64__)
  std::cout << " x86_64";
#elif defined(_M_ARM64) || defined(__aarch64__)
  std::cout << " ARM64";
#endif

  std::cout << "\n";
}

void CommandLineInterface::showCommandHelp(const std::string& command) const {
  auto cmd_it = commands.find(command);
  if (cmd_it == commands.end()) {
    std::cerr << "Error: Unknown command '" << command << "'\n";
    return;
  }

  std::cout << "Loom Compiler - " << command << " command\n\n";
  std::cout << "USAGE:\n";
  std::cout << "    " << cmd_it->second->getUsage() << "\n\n";
  std::cout << "DESCRIPTION:\n";
  std::cout << "    " << cmd_it->second->getDescription() << "\n\n";
}

}  // namespace loom
