workspace(name = "loom")

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

# Abseil C++ library (useful for modern C++ utilities)
http_archive(
    name = "com_google_absl",
    urls = ["https://github.com/abseil/abseil-cpp/archive/refs/tags/20240116.2.tar.gz"],
    strip_prefix = "abseil-cpp-20240116.2",
    sha256 = "733726b8c3a6d39a4120d7e45ea8b41a434cdacde401cba500f14236c49b39dc",
)

# LLVM (if you plan to use LLVM for code generation)
http_archive(
    name = "llvm-raw",
    build_file_content = "# Empty BUILD file",
    sha256 = "11a086b9d35115c43dc2da62b92cba5b24e7c19c5a0fbc3a98cc6d44ad41d0c1",
    strip_prefix = "llvm-project-llvmorg-17.0.6",
    urls = ["https://github.com/llvm/llvm-project/archive/llvmorg-17.0.6.tar.gz"],
)

# You can uncomment and configure LLVM rules if needed:
# load("@llvm-raw//utils/bazel:configure.bzl", "llvm_configure")
# llvm_configure(name = "llvm-project")
