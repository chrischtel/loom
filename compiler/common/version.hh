#pragma once

#include <string>

namespace loom {
namespace version {

// Version information (populated by CMake)
extern const char* const VERSION;
extern const char* const GIT_HASH;
extern const char* const BUILD_DATE;
extern const char* const BUILD_TIME;
extern const char* const COMPILER_VERSION;
extern const bool IS_DEBUG_BUILD;

// Utility functions
std::string getFullVersionString();
std::string getBuildInfo();
void printVersionInfo();

}  // namespace version
}  // namespace loom
