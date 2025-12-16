#include "Parse.h"
#include "Generate.h"

// To compile Jolt:
// $> cd JoltPhysics/Build
// $> cmake -B .build -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
// $> cmake --build .build
// When adding compile options, don't forget to fetch the appropriate compiler options
// in JoltPhysics/Build/.build/compile_commands.json and update the CppParseOptions

static const char *Jolt_Source_Files[] = {
    #include "JoltSourceFiles.txt"
};

static const char *Declarations_To_Exclude[] = {
    "std::hash",
    "JPH::Hash",
};

int main() {
    CppDatabase db = {};
    InitCppDatabase(&db);

    CppParseOptions parse_options = {};
    parse_options.preparse_files_for_correct_include_order = false;

    if (parse_options.preparse_files_for_correct_include_order) {
        for (uint64_t i = 0; i < StaticArraySize(Jolt_Source_Files); i += 1) {
            ArrayPush(&parse_options.files, (char *)Jolt_Source_Files[i]);
        }
    } else {
        ArrayPush(&parse_options.files, "Source/JoltHeaders.h");
    }

    ArrayPush(&parse_options.include_dirs, "JoltPhysics");

    // Avoid Windows.h bloat
    ArrayPush(&parse_options.defines, "WIN32_LEAN_AND_MEAN");

    ArrayPush(&parse_options.defines, "JPH_OBJECT_STREAM");
    ArrayPush(&parse_options.defines, "JPH_USE_AVX");
    ArrayPush(&parse_options.defines, "JPH_USE_AVX2");
    ArrayPush(&parse_options.defines, "JPH_USE_F16C");
    ArrayPush(&parse_options.defines, "JPH_USE_FMADD");
    ArrayPush(&parse_options.defines, "JPH_USE_LZCNT");
    ArrayPush(&parse_options.defines, "JPH_USE_SSE4_1");
    ArrayPush(&parse_options.defines, "JPH_USE_SSE4_2");
    ArrayPush(&parse_options.defines, "JPH_USE_TZCNT");

    ArrayPush(&parse_options.extra_options, "-xc++");
    ArrayPush(&parse_options.extra_options, "-std=c++17");

    ParseCppFiles(parse_options, &db);

    GenerateOptions gen_options = {};
    for (uint64_t i = 0; i < StaticArraySize(Declarations_To_Exclude); i += 1) {
        ArrayPush(&gen_options.declarations_to_exclude, (void *)Declarations_To_Exclude[i]);
    }

    ArrayPush(&gen_options.typedefs_to_unwrap, "JPH::uint");
    ArrayPush(&gen_options.typedefs_to_unwrap, "JPH::uint8");
    ArrayPush(&gen_options.typedefs_to_unwrap, "JPH::uint16");
    ArrayPush(&gen_options.typedefs_to_unwrap, "JPH::uint32");
    ArrayPush(&gen_options.typedefs_to_unwrap, "JPH::uint64");

    gen_options.preamble = ReadEntireFile("Source/JoltCPreamble.h", NULL);
    if (!gen_options.preamble) {
        ErrorExit("Could not read file 'Source/JoltCPreamble.h'");
    }

    // gen_options.postamble = ReadEntireFile("Source/JoltCPostamble.h", NULL);
    // if (!gen_options.postamble) {
    //     ErrorExit("Could not read file 'Source/JoltCPostamble.h'");
    // }

    StringBuilder builder = {};
    GenerateCode(gen_options, &builder, &db);

    char *str = SBBuild(&builder);
    WriteEntireFile("JoltC.h", str, strlen(str));
}
