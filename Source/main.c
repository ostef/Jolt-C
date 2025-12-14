#include "Parse.h"
#include "Generate.h"

int main() {
    CppDatabase db = {};
    InitCppDatabase(&db);

    CppParseOptions options = {};
    ArrayPush(&options.files, "Source/Jolt.h");
    ArrayPush(&options.include_dirs, "JoltPhysics");
    ArrayPush(&options.defines, "WIN32_LEAN_AND_MEAN");
    ArrayPush(&options.extra_options, "-xc++");

    ParseCppFiles(options, &db);

    StringBuilder builder = {};
    GenerateCode(&builder, &db);

    char *str = SBBuild(&builder);
    WriteEntireFile("JoltC.h", str, strlen(str));
}
