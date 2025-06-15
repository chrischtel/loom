// main.cc - New Loom Compiler CLI

#include <iostream>
#include <memory>

#include "cli/cli.hh"
#include "cli/commands.hh"

int main(int argc, char* argv[]) {
  try {
    // Create CLI interface
    loom::CommandLineInterface cli;

    // Register all standard commands
    auto commands = loom::createStandardCommands();
    for (auto& command : commands) {
      cli.registerCommand(std::move(command));
    }

    // Parse arguments and execute command
    return cli.parseAndExecute(argc, argv);

  } catch (const std::exception& e) {
    std::cerr << "Fatal error: " << e.what() << std::endl;
    return 1;
  } catch (...) {
    std::cerr << "Unknown fatal error occurred" << std::endl;
    return 1;
  }
}
