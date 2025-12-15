#include "Parse.h"
#include "Generate.h"

// To compile Jolt:
// $> cd JoltPhysics/Build
// $> cmake -B .build -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
// $> cmake --build .build
// When adding compile options, don't forget to fetch the appropriate compiler options
// in JoltPhysics/Build/.build/compile_commands.json and update the CppParseOptions

static const char *Jolt_Source_Files[] = {
    #include "JoltSourceFiles.inl"
};

int main() {
    CppDatabase db = {};
    InitCppDatabase(&db);

    CppParseOptions options = {};
    options.preparse_files_for_correct_include_order = true;

    if (options.preparse_files_for_correct_include_order) {
        for (int i = 0; i < StaticArraySize(Jolt_Source_Files); i += 1) {
            ArrayPush(&options.files, Jolt_Source_Files[i]);
        }
    } else {
        ArrayPush(&options.files, "Source/Jolt.h");
    }

    ArrayPush(&options.include_dirs, "JoltPhysics");

    // Avoid Windows.h bloat
    ArrayPush(&options.defines, "WIN32_LEAN_AND_MEAN");

    ArrayPush(&options.defines, "JPH_OBJECT_STREAM");
    ArrayPush(&options.defines, "JPH_USE_AVX");
    ArrayPush(&options.defines, "JPH_USE_AVX2");
    ArrayPush(&options.defines, "JPH_USE_F16C");
    ArrayPush(&options.defines, "JPH_USE_FMADD");
    ArrayPush(&options.defines, "JPH_USE_LZCNT");
    ArrayPush(&options.defines, "JPH_USE_SSE4_1");
    ArrayPush(&options.defines, "JPH_USE_SSE4_2");
    ArrayPush(&options.defines, "JPH_USE_TZCNT");

    ArrayPush(&options.extra_options, "-xc++");
    ArrayPush(&options.extra_options, "-std=c++17");

    ParseCppFiles(options, &db);

    StringBuilder builder = {};
    GenerateCode(&builder, &db);

    char *str = SBBuild(&builder);
    WriteEntireFile("JoltC.h", str, strlen(str));
}
