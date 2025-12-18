#ifndef DATABASE_H
#define DATABASE_H

#include <clang-c/Index.h>

#include "Core.h"

// Ast-like representation, only for what we need

struct CppDatabase;
struct CppType;
struct CppSourceCodeLocation;
struct CppSourceCodeRange;
struct CppEntity;
struct CppNamespace;
struct CppValueDefine;
struct CppEnum;
struct CppEnumConstant;
struct CppAggregate;
struct CppTypedef;
struct CppVariable;
struct CppFunction;

typedef struct CppDatabase {
    struct CppNamespace *global_namespace;
    Array all_namespaces;
    Array all_value_defines;
    Array all_enums;
    Array all_aggregates;
    Array all_typedefs;
    Array all_functions;
    Array all_entities;
    // Fully qualified name to Array* of CppFunction *
    // Contains all functions, not just overloaded functions
    HashMap function_overloads;
    HashMap cursor_to_entity;
} CppDatabase;

typedef uint8_t CppTypeKind;
enum {
    CppType_Invalid,
    CppType_Unknown,
    CppType_Void,
    CppType_Bool,
    CppType_Char,
    CppType_UInt8,
    CppType_UInt16,
    CppType_UInt32,
    CppType_UInt64,
    CppType_UInt128,
    CppType_Int8,
    CppType_Int16,
    CppType_Int32,
    CppType_Int64,
    CppType_Int128,
    CppType_Float,
    CppType_Double,
    CppType_Pointer,
    CppType_Reference,
    CppType_RValueReference,
    CppType_Array,
    CppType_Aggregate,
    CppType_Enum,
    CppType_Named,
    CppType_Function,
    CppType_Auto,
    CppType_SIMDVector,
};

static const char *CppTypeKind_Str[] = {
    "invalid",
    "unknown",
    "void",
    "bool",
    "char",
    "uint8_t",
    "uint16_t",
    "uint32_t",
    "uint64_t",
    "uint128_t",
    "int8_t",
    "int16_t",
    "int32_t",
    "int64_t",
    "int128_t",
    "float",
    "double",
    "pointer",
    "reference",
    "r-value reference",
    "array",
    "aggregate",
    "enum",
    "named",
    "function",
    "auto",
    "SIMD vector",
};

typedef uint32_t CppTypeFlags;
enum {
    CppTypeFlag_Const    = 1 << 0,
    CppTypeFlag_Volatile = 1 << 1,
    CppTypeFlag_Restrict = 1 << 2,
    CppTypeFlag_IsPOD    = 1 << 3,
};

typedef struct CppTypePointer {
    struct CppType *pointee_type;
} CppTypePointer;

typedef struct CppTypeArray {
    struct CppType *element_type;
    int64_t num_elements;
} CppTypeArray;

typedef struct CppTypeAggregate {
    CXCursor cursor;
    struct CppAggregate *aggr;
} CppTypeAggregate;

typedef struct CppTypeEnum {
    CXCursor cursor;
    struct CppEnum *e;
} CppTypeEnum;

typedef struct CppTypeNamed {
    const char *name;
    CXCursor cursor;
    struct CppEntity *entity;
} CppTypeNamed;

typedef struct CppTypeFunction {
    bool is_variadic;
    struct CppType *result_type;
    Array parameter_types;
} CppTypeFunction;

typedef struct CppType {
    CppTypeKind kind;
    CppTypeFlags flags;
    CXType cx_type;
    int64_t size;
    int64_t alignment;
    union {
        CppTypePointer type_pointer;
        CppTypeArray type_array;
        CppTypeAggregate type_aggregate;
        CppTypeEnum type_enum;
        CppTypeNamed type_named;
        CppTypeFunction type_function;
    };
} CppType;

typedef struct CppSourceCodeLocation {
    const char *filename;
    int64_t line;
    int64_t character;
    int64_t offset;
} CppSourceCodeLocation;

typedef struct CppSourceCodeRange {
    const char *filename;
    int64_t start_line, end_line;
    int64_t start_character, end_character;
    int64_t start_offset, end_offset;
} CppSourceCodeRange;

static inline
CppSourceCodeLocation GetStartLocation(CppSourceCodeRange range) {
    return (CppSourceCodeLocation){range.filename, range.start_line, range.start_character, range.start_offset};
}

static inline
CppSourceCodeLocation GetEndLocation(CppSourceCodeRange range) {
    return (CppSourceCodeLocation){range.filename, range.end_line, range.end_character, range.end_offset};
}

typedef uint8_t CppVisibility;
enum {
    CppVisibility_Public,
    CppVisibility_Protected,
    CppVisibility_Private,
};

typedef uint8_t CppEntityKind;
enum {
    CppEntity_Invalid,
    CppEntity_Namespace,
    CppEntity_ValueDefine,
    CppEntity_Enum,
    CppEntity_EnumConstant,
    CppEntity_Aggregate,
    CppEntity_Typedef,
    CppEntity_Variable,
    CppEntity_Function,
};

static const char *CppEntityKind_Str[] = {
    "invalid",
    "namespace",
    "value define",
    "enum",
    "enum constant",
    "aggregate",
    "typedef",
    "variable",
    "function",
};

typedef uint32_t CppEntityFlags;
enum {
    CppEntityFlag_Static           = 1 << 0,
    CppEntityFlag_ForwardDecl      = 1 << 1,
    CppEntityFlag_ParentIsTemplate = 1 << 2,
};

typedef struct CppEntity {
    CppEntityKind kind;
    CppEntityFlags flags;
    uint64_t user_flags;
    CppVisibility visibility;
    CXCursor cursor;
    CppSourceCodeRange source_code_range;
    struct CppEntity *parent;
    const char *comment;
    char *name;
    char *c_name;
    char *unique_c_name;
    char *fully_qualified_name;
    char *fully_qualified_c_name;
    char *unique_fully_qualified_c_name;
} CppEntity;

typedef struct CppNamespace {
    CppEntity base;
    Array entities;
} CppNamespace;

typedef uint8_t CppAggregateKind;
enum {
    CppAggregate_Class,
    CppAggregate_Struct,
    CppAggregate_Union,
};

static const char *CppAggregateKind_Str[] = {
    "class",
    "struct",
    "union",
};

typedef struct CppBaseClass {
    CppVisibility visibility;
    bool is_virtual;
    CppType *type;
} CppBaseClass;

typedef uint32_t CppAggregateFlags;
enum {
    CppAggregateFlag_Abstract          = 1 << 0,
    CppAggregateFlag_Template          = 1 << 1,
    CppAggregateFlag_VirtualDestructor = 1 << 2,
};

typedef struct CppAggregate {
    CppEntity base;
    CppAggregateFlags flags;
    CppType *type;
    Array fields;
    Array virtual_methods;
    Array functions;
    Array entities;
    CppAggregateKind kind;
    Array base_classes;
} CppAggregate;

typedef struct CppTypedef {
    CppEntity base;
    CppType *type;
} CppTypedef;

typedef struct CppValueDefine {
    CppEntity base;
    const char *value;
} CppValueDefine;

typedef uint8_t CppEnumFlags;
enum {
    CppEnumFlag_Scoped = 1 << 0,
};

typedef struct CppEnum {
    CppEntity base;
    CppEnumFlags flags;
    CppType *base_type;
    Array constants;
} CppEnum;

typedef struct CppEnumConstant {
    CppEntity base;
    uint64_t value;
} CppEnumConstant;

typedef struct CppVariable {
    CppEntity base;
    CppType *type;
} CppVariable;

typedef uint32_t CppFunctionFlags;
enum {
    CppFunctionFlag_Constructor = 1 << 0,
    CppFunctionFlag_Destructor  = 1 << 1,
    CppFunctionFlag_Method      = 1 << 2,
    CppFunctionFlag_Virtual     = 1 << 3,
    CppFunctionFlag_PureVirtual = 1 << 4,
    CppFunctionFlag_Const       = 1 << 5,
    CppFunctionFlag_Operator    = 1 << 6,
    CppFunctionFlag_Overloaded  = 1 << 7,
};

typedef struct CppFunction {
    CppEntity base;
    CppFunctionFlags flags;
    CppType *type;
    CppType *result_type;
    Array parameters;
} CppFunction;

void InitCppDatabase(CppDatabase *db);
CppEntity *GetCppEntityFromCursor(CppDatabase *db, CXCursor cursor);
void PushCppEntity(CppDatabase *db, CppEntity *parent, CppEntity *entity);

CppEntity *AllocCppEntityOfKind(CppEntityKind kind, int size, CXCursor cursor);
#define AllocCppEntity(kind, cursor) ((Cpp##kind *)AllocCppEntityOfKind(CppEntity_##kind, sizeof(Cpp##kind), (cursor)))

CppNamespace *GetCppNamespace(CppDatabase *db, CppEntity *parent, char *name);

#endif
