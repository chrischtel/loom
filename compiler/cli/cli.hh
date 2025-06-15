#pragma once

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace loom {

// Forward declarations
struct CompilerOptions;
class CommandLineInterface;

// Build modes
enum class BuildMode { DEBUG, RELEASE, PROFILE };

// Verbosity levels
enum class VerbosityLevel {
  QUIET = 0,    // Only errors
  NORMAL = 1,   // Standard output
  VERBOSE = 2,  // Detailed output
  DEBUG = 3     // Debug information
};

// Target architectures
enum class TargetArch { X86_64, ARM64, WASM32 };

// Target platforms
enum class TargetPlatform { WINDOWS, LINUX, MACOS, WASM };

// Compiler options and flags
struct CompilerOptions {
  // Input/Output
  std::string input_file;
  std::string output_file;
  std::string output_dir = "./";

  // Build configuration
  BuildMode build_mode = BuildMode::DEBUG;
  TargetArch target_arch = TargetArch::X86_64;
  TargetPlatform target_platform = TargetPlatform::WINDOWS;

  // Compiler behavior
  VerbosityLevel verbosity = VerbosityLevel::NORMAL;
  bool show_help = false;
  bool show_version = false;
  bool emit_llvm_ir = false;
  bool emit_assembly = false;
  bool no_link = false;
  bool run_after_build = false;
  bool check_only = false;

  // Optimization
  int optimization_level = 0;  // 0=none, 1=basic, 2=aggressive, 3=maximum
  bool enable_lto = false;     // Link-time optimization

  // Debugging
  bool generate_debug_info = true;
  bool emit_debug_metadata = false;

  // Warnings and errors
  bool warnings_as_errors = false;
  bool disable_warnings = false;
  std::vector<std::string> enabled_warnings;
  std::vector<std::string> disabled_warnings;

  // Paths
  std::vector<std::string> include_paths;
  std::vector<std::string> library_paths;
  std::vector<std::string> libraries;

  // Advanced
  bool show_ast = false;
  bool show_tokens = false;
  bool show_semantic_info = false;
  bool benchmark_compilation = false;
  std::string config_file;
};

// Command interface
class Command {
 public:
  virtual ~Command() = default;
  virtual std::string getName() const = 0;
  virtual std::string getDescription() const = 0;
  virtual std::string getUsage() const = 0;
  virtual int execute(const CompilerOptions& options) = 0;
  virtual void addSpecificOptions(CommandLineInterface& /*cli*/) {}
};

// Flag handler
struct Flag {
  std::string short_name;
  std::string long_name;
  std::string description;
  bool has_value;
  std::function<void(CompilerOptions&, const std::string&)> handler;

  Flag(const std::string& short_name, const std::string& long_name,
       const std::string& description, bool has_value,
       std::function<void(CompilerOptions&, const std::string&)> handler)
      : short_name(short_name),
        long_name(long_name),
        description(description),
        has_value(has_value),
        handler(handler) {}
};

// Main CLI class
class CommandLineInterface {
 public:
  CommandLineInterface();
  ~CommandLineInterface();

  // Register commands
  void registerCommand(std::unique_ptr<Command> command);

  // Register flags
  void addFlag(const Flag& flag);
  void addGlobalFlags();

  // Parse command line arguments
  int parseAndExecute(int argc, char* argv[]);

  // Help and version
  void showHelp() const;
  void showVersion() const;
  void showCommandHelp(const std::string& command) const;

  // Utility
  std::string getExecutableName() const { return executable_name; }

 private:
  std::string executable_name;
  std::map<std::string, std::unique_ptr<Command>> commands;
  std::vector<Flag> flags;

  // Parse helpers
  CompilerOptions parseFlags(int argc, char* argv[], int& command_index);
  std::string findCommand(int argc, char* argv[]) const;
  bool isFlag(const std::string& arg) const;
  std::string getShortFlag(const std::string& arg) const;
  std::string getLongFlag(const std::string& arg) const;
};

}  // namespace loom
