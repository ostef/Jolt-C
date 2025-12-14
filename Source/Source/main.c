#include <clang-c/Index.h>

#include "parse.h"

int main(int argc, const char *const *argv) {
    ParseOptions options = {};
    ArrayPush(&options.files, "Source/Jolt.h");
    ArrayPush(&options.include_dirs, "JoltPhysics");
    ArrayPush(&options.defines, "WIN32_LEAN_AND_MEAN");
    ArrayPush(&options.extra_options, "-xc++");

    ParseCppFiles(options);
}
