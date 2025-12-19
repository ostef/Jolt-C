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
    "JPH::BroadPhaseLayer",
    "JPH::BroadPhaseLayer::Type",
    "JPH::ObjectLayer",

    "std::hash",
    "JPH::Hash",

    "JPH::String",
    "JPH::IStringStream",
    "JPH::MutexBase",
    "JPH::SharedMutexBase",

    "JPH::UnorderedMap",
    "JPH::UnorderedSet",

    "JPH::BodyAccess",
    "JPH::BodyAccess::EAccess",
    "JPH::BodyAccess::Grant",

    "sCreateRTTI",
    "sRegister",
};

static const char *Typedefs_To_Unwrap[] = {
    "JPH::uint",
    "JPH::uint8",
    "JPH::uint16",
    "JPH::uint32",
    "JPH::uint64",
    "JPH::Vec3Arg",
    // "JPH::Vec3::Type",
    "JPH::Vec3::ArgType",
    "JPH::DVec3Arg",
    // "JPH::DVec3::Type",
    // "JPH::DVec3::TypeArg",
    "JPH::DVec3::ArgType",
    "JPH::UVec3Arg",
    "JPH::RVec3Arg",
    "JPH::Vec4Arg",
    // "JPH::Vec4::Type",
    "JPH::DVec4Arg",
    "JPH::UVec4Arg",
    "JPH::RVec4Arg",
    "JPH::BVec16Arg",
    "JPH::QuatArg",
    "JPH::Mat44Arg",
    // "JPH::Mat44::Type",
    "JPH::Mat44::ArgType",
    "JPH::DMat44Arg",
    // "JPH::DMat44::Type",
    // "JPH::DMat44::DType",
    // "JPH::DMat44::DTypeArg",
    "JPH::DMat44::ArgType",
    "JPH::RMat44Arg",
    "JPH::ColorArg",
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
        ArrayPush(&parse_options.files, "JoltHeaders.h");
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
    for (uint64_t i = 0; i < StaticArraySize(Typedefs_To_Unwrap); i += 1) {
        ArrayPush(&gen_options.typedefs_to_unwrap, (void *)Typedefs_To_Unwrap[i]);
    }

    gen_options.preamble = ReadEntireFile("Source/JoltCPreamble.h", NULL);
    if (!gen_options.preamble) {
        ErrorExit("Could not read file 'Source/JoltCPreamble.h'");
    }

    gen_options.exclude_non_class_functions = true;

    // gen_options.postamble = ReadEntireFile("Source/JoltCPostamble.h", NULL);
    // if (!gen_options.postamble) {
    //     ErrorExit("Could not read file 'Source/JoltCPostamble.h'");
    // }

    ProcessCppDatabaseBeforeCodegen(gen_options, &db);

    StringBuilder header_builder = {};
    GenerateCHeader(gen_options, &header_builder, &db);

    StringBuilder source_builder = {};
    GenerateCppSource(gen_options, &source_builder, &db);

    char *header = SBBuild(&header_builder);
    WriteEntireFile("JoltC.h", header, strlen(header));

    char *source = SBBuild(&source_builder);
    WriteEntireFile("JoltC.cpp", source, strlen(source));
}
