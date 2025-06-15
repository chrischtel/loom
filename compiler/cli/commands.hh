#pragma once

#include <memory>

#include "cli.hh"

namespace loom {

// Build command - compiles Loom source files
class BuildCommand : public Command {
 public:
  std::string getName() const override { return "build"; }
  std::string getDescription() const override {
    return "Compile Loom source files to executable";
  }
  std::string getUsage() const override {
    return "loom build [OPTIONS] <INPUT_FILE>";
  }
  int execute(const CompilerOptions& options) override;
};

// Run command - builds and runs the executable
class RunCommand : public Command {
 public:
  std::string getName() const override { return "run"; }
  std::string getDescription() const override {
    return "Build and run Loom program";
  }
  std::string getUsage() const override {
    return "loom run [OPTIONS] <INPUT_FILE> [-- <PROGRAM_ARGS>]";
  }
  int execute(const CompilerOptions& options) override;
};

// Check command - syntax and semantic checking only
class CheckCommand : public Command {
 public:
  std::string getName() const override { return "check"; }
  std::string getDescription() const override {
    return "Check syntax and semantics without building";
  }
  std::string getUsage() const override {
    return "loom check [OPTIONS] <INPUT_FILE|DIRECTORY>";
  }
  int execute(const CompilerOptions& options) override;
};

// Clean command - remove build artifacts
class CleanCommand : public Command {
 public:
  std::string getName() const override { return "clean"; }
  std::string getDescription() const override {
    return "Remove build artifacts and cache";
  }
  std::string getUsage() const override { return "loom clean [OPTIONS]"; }
  int execute(const CompilerOptions& options) override;
};

// Info command - show project and compiler information
class InfoCommand : public Command {
 public:
  std::string getName() const override { return "info"; }
  std::string getDescription() const override {
    return "Show project and compiler information";
  }
  std::string getUsage() const override { return "loom info [OPTIONS]"; }
  int execute(const CompilerOptions& options) override;
};

// Format command - format Loom source code
class FormatCommand : public Command {
 public:
  std::string getName() const override { return "fmt"; }
  std::string getDescription() const override {
    return "Format Loom source code";
  }
  std::string getUsage() const override {
    return "loom fmt [OPTIONS] <INPUT_FILE|DIRECTORY>";
  }
  int execute(const CompilerOptions& options) override;
};

// Test command - run tests
class TestCommand : public Command {
 public:
  std::string getName() const override { return "test"; }
  std::string getDescription() const override { return "Run tests"; }
  std::string getUsage() const override {
    return "loom test [OPTIONS] [TEST_FILE|DIRECTORY]";
  }
  int execute(const CompilerOptions& options) override;
};

// New command - create new Loom project
class NewCommand : public Command {
 public:
  std::string getName() const override { return "new"; }
  std::string getDescription() const override {
    return "Create a new Loom project";
  }
  std::string getUsage() const override {
    return "loom new [OPTIONS] <PROJECT_NAME>";
  }
  int execute(const CompilerOptions& options) override;
};

// Factory function to create all standard commands
std::vector<std::unique_ptr<Command>> createStandardCommands();

}  // namespace loom
