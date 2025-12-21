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

static const char *Declarations_To_Include[] = {
    // "EXCLUDE_EVERYTHING",

static const char *Opaque_Classes[] = {
    "JobSystem",
    "JobSystemThreadPool",
    "JobSystemSingleThreaded",
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

static CppType g_type_jph_array;
static CppType g_type_jph_hash_table;
static CppType g_type_jph_unordered_map;
static CppType g_type_jph_unordered_set;
static CppType g_type_jph_lock_free_hash_map;
static CppType g_type_jph_strided_ptr;
static CppType g_type_jph_vector2;
static CppType g_type_jph_vector3;
static CppType g_type_jph_matrix22;
static CppType g_type_jph_matrix33;
static CppType g_type_jph_collision_collector;
static CppType g_type_jph_ref_target;
static CppType g_type_jph_function;

void InitTypes() {
    static bool initialized = false;
    if (initialized) {
        return;
    }

    g_type_jph_array.kind = CppType_Named;
    g_type_jph_array.size = 3 * sizeof(uint64_t);
    g_type_jph_array.alignment = sizeof(uint64_t);
    g_type_jph_array.type_named.name = "JPH_Array";

    g_type_jph_hash_table.kind = CppType_Named;
    g_type_jph_hash_table.size = 5 * sizeof(uint64_t);
    g_type_jph_hash_table.alignment = sizeof(uint64_t);
    g_type_jph_hash_table.type_named.name = "JPH_HashTable";

    g_type_jph_unordered_map = g_type_jph_hash_table;
    g_type_jph_unordered_map.type_named.name = "JPH_UnorderedMap";

    g_type_jph_unordered_set = g_type_jph_hash_table;
    g_type_jph_unordered_set.type_named.name = "JPH_UnorderedSet";

    g_type_jph_lock_free_hash_map.kind = CppType_Named;
    g_type_jph_lock_free_hash_map.size = 32;
    g_type_jph_lock_free_hash_map.alignment = sizeof(void *);
    g_type_jph_lock_free_hash_map.type_named.name = "JPH_LockFreeHashMap";

    g_type_jph_strided_ptr.kind = CppType_Named;
    g_type_jph_strided_ptr.size = 2 * sizeof(uint64_t);
    g_type_jph_strided_ptr.alignment = sizeof(uint64_t);
    g_type_jph_strided_ptr.type_named.name = "JPH_StridedPtr";

    g_type_jph_vector2.kind = CppType_Named;
    g_type_jph_vector2.size = 2 * sizeof(float);
    g_type_jph_vector2.alignment = sizeof(float);
    g_type_jph_vector2.type_named.name = "JPH_Vector2";

    g_type_jph_vector3.kind = CppType_Named;
    g_type_jph_vector3.size = 3 * sizeof(float);
    g_type_jph_vector3.alignment = sizeof(float);
    g_type_jph_vector3.type_named.name = "JPH_Vector3";

    g_type_jph_matrix22.kind = CppType_Named;
    g_type_jph_matrix22.size = 2 * 2 * sizeof(float);
    g_type_jph_matrix22.alignment = sizeof(float);
    g_type_jph_matrix22.type_named.name = "JPH_Matrix22";

    g_type_jph_matrix33.kind = CppType_Named;
    g_type_jph_matrix33.size = 3 * 3 * sizeof(float);
    g_type_jph_matrix33.alignment = sizeof(float);
    g_type_jph_matrix33.type_named.name = "JPH_Matrix33";

    g_type_jph_collision_collector.kind = CppType_Named;
    g_type_jph_collision_collector.size = 3 * sizeof(void *);
    g_type_jph_collision_collector.alignment = sizeof(void *);
    g_type_jph_collision_collector.type_named.name = "JPH_CollisionCollector";

    g_type_jph_ref_target.kind = CppType_Named;
    g_type_jph_ref_target.size = sizeof(uint32_t);
    g_type_jph_ref_target.alignment = sizeof(uint32_t);
    g_type_jph_ref_target.type_named.name = "JPH_RefTarget";

    g_type_jph_function.kind = CppType_Named;
    g_type_jph_function.size = 32;
    g_type_jph_function.alignment = 8;
    g_type_jph_function.type_named.name = "JPH_Function";
}

CppType *UnwrapTemplateFunc(GenerateOptions options, CppDatabase *db, CppType *type) {
    InitTypes();

    if (StrEq(type->type_named.name, "Array")) {
        return &g_type_jph_array;
    }
    if (StrEq(type->type_named.name, "HashTable")) {
        return &g_type_jph_hash_table;
    }
    if (StrEq(type->type_named.name, "UnorderedMap")) {
        return &g_type_jph_unordered_map;
    }
    if (StrEq(type->type_named.name, "UnorderedSet")) {
        return &g_type_jph_unordered_set;
    }
    if (StrEq(type->type_named.name, "LockFreeHashMap")) {
        return &g_type_jph_lock_free_hash_map;
    }
    if (StrEq(type->type_named.name, "StridedPtr")) {
        return &g_type_jph_strided_ptr;
    }
    if (StrEq(type->type_named.name, "atomic")) {
        CppType *ty = ArrayGet(type->type_named.template_type_arguments, 0);
        return UnwrapTemplate(options, db, ty);
    }
    if (StrEq(type->type_named.name, "function")) {
        return &g_type_jph_function;
    }
    if (StrEq(type->type_named.name, "StaticArray")) {
        CppType *type_of_array = ArrayGet(type->type_named.template_type_arguments, 0);
        type_of_array = UnwrapTemplate(options, db, type_of_array);
        uint32_t array_count = 1;

        CppType *result = Alloc(CppType);
        result->kind = CppType_Named;
        result->size = AlignForward(sizeof(uint32_t), type_of_array->alignment);
        result->size += type_of_array->size * array_count;
        result->alignment = type->alignment;

        StringBuilder builder = {};
        SBAppendString(&builder, "JPH_StaticArrayT(");

        GenerateContext ctx = {};
        ctx.builder = &builder;
        ctx.db = db;
        ctx.options = options;

        AppendCType(&ctx, type_of_array, 0);
        SBAppend(&builder, ", %u)", array_count);

        result->type_named.name = SBBuild(&builder);

        return result;

    }
    if (StrEq(type->type_named.name, "Vector")) {
        return &g_type_jph_vector2;
    }
    if (StrEq(type->type_named.name, "Matrix")) {
        return &g_type_jph_matrix22;
    }
    if (StrEq(type->type_named.name, "Result")) {
        CppType *type_of_result = ArrayGet(type->type_named.template_type_arguments, 0);
        type_of_result = UnwrapTemplate(options, db, type_of_result);

        CppType *result = Alloc(CppType);
        result->kind = CppType_Named;
        result->size = type->size;
        result->alignment = type->alignment;

        StringBuilder builder = {};
        SBAppendString(&builder, "JPH_ResultT(");

        GenerateContext ctx = {};
        ctx.builder = &builder;
        ctx.db = db;
        ctx.options = options;

        AppendCType(&ctx, type_of_result, 0);
        SBAppendString(&builder, ")");

        result->type_named.name = SBBuild(&builder);

        return result;
    }
    if (StrEq(type->type_named.name, "Ref")) {
        CppType *ref_type = ArrayGet(type->type_named.template_type_arguments, 0);
        ref_type = UnwrapTemplate(options, db, ref_type);

        CppType *result = Alloc(CppType);
        result->kind = CppType_Pointer;
        result->size = sizeof(void *);
        result->alignment = sizeof(void *);
        result->type_pointer.pointee_type = ref_type;

        return result;
    }
    if (StrEq(type->type_named.name, "RefConst")) {
        CppType *ref_type = ArrayGet(type->type_named.template_type_arguments, 0);
        ref_type = UnwrapTemplate(options, db, ref_type);

        CppType *result = Alloc(CppType);
        result->kind = CppType_Pointer;
        result->size = sizeof(void *);
        result->alignment = sizeof(void *);
        result->flags = CppTypeFlag_Const;
        result->type_pointer.pointee_type = ref_type;

        return result;
    }
    if (StrEq(type->type_named.name, "CollisionCollector")) {
        return &g_type_jph_collision_collector;
    }
    if (StrEq(type->type_named.name, "RefTarget")) {
        return &g_type_jph_ref_target;
    }

    return type;
}

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

    // ArrayPush(&parse_options.defines, "JPH_OBJECT_STREAM");
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
    for (uint64_t i = 0; i < StaticArraySize(Declarations_To_Include); i += 1) {
        ArrayPush(&gen_options.declarations_to_include, (void *)Declarations_To_Include[i]);
    }
    for (uint64_t i = 0; i < StaticArraySize(Opaque_Classes); i += 1) {
        ArrayPush(&gen_options.opaque_classes, (void *)Opaque_Classes[i]);
    }
    for (uint64_t i = 0; i < StaticArraySize(Typedefs_To_Unwrap); i += 1) {
        ArrayPush(&gen_options.typedefs_to_unwrap, (void *)Typedefs_To_Unwrap[i]);
    }

    gen_options.exclude_non_class_functions = true;
    gen_options.template_unwrap_func = UnwrapTemplateFunc;

    gen_options.preamble = ReadEntireFile("Source/JoltCPreamble.h", NULL);
    if (!gen_options.preamble) {
        ErrorExit("Could not read file 'Source/JoltCPreamble.h'");
    }

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
