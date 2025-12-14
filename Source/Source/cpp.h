#ifndef CPP_H
#define CPP_H

#include "utils.h"

struct CppType;
struct CppAst;
struct CppNamespaceDecl;

typedef struct CppLocation {
    const char *filename;
    int64_t line;
    int64_t character;
} CppLocation;

typedef uint8_t CppAstKind;
enum {
    CppAst_Invalid,
    CppAst_StructDecl,
    CppAst_UnionDecl,
    CppAst_ClassDecl,
    CppAst_EnumDecl,
    CppAst_EnumConstantDecl,
    CppAst_TypedefDecl,
    CppAst_FunctionDecl,
    CppAst_VariableDecl,
    CppAst_NamespaceDecl,
};

typedef struct CppAst {
    CppAstKind kind;
    CppLocation location;
    struct CppNamespaceDecl *enclosing_namespace;
} CppAst;

typedef uint32_t CppDeclFlags;
enum {
    CppDeclFlag_Static = 1 << 0,
};

typedef struct CppNamespaceDecl {
    CppAst ast;
    const char *name;
    Array declarations;
    Array fields;
    Array enum_constants;
    Array functions;
} CppNamespaceDecl;

typedef uint8_t CppVisibility;
enum {
    CppVisibility_Public,
    CppVisibility_Protected,
    CppVisibility_Private,
};

typedef struct CppVisibilityDecl {
    CppAst ast;
    CppVisibility visibility;
} CppVisibilityDecl;

typedef struct CppBaseClass {
    CppVisibility visibility;
    bool explicit_visibility;
    CppAst *expression;
} CppBaseClass;

typedef struct CppAggregateDecl {
    CppNamespaceDecl namespace_decl;
    Array base_classes;
} CppAggregateDecl;

typedef struct CppEnumDecl {
    CppNamespaceDecl namespace_decl;
    bool is_class;
    CppAst *base_type;
} CppEnumDecl;

typedef struct CppTypedefDecl {
    CppAst ast;
    const char *name;
    CppAst *expression;
} CppTypedefDecl;

typedef struct CppVariableDecl {
    CppAst ast;
    CppDeclFlags decl_flags;
    const char *name;
    CppAst *type_inst;
    CppAst *expression;
} CppVariableDecl;

typedef struct CppFunctionDecl {
    CppAst ast;
    CppDeclFlags decl_flags;
    const char *name;
    CppAst *result;
    Array parameters;
} CppFunctionDecl;

typedef uint8_t CppTypeKind;
enum {
    CppType_Invalid,
    CppType_Void,
    CppType_Bool,
    CppType_Char,
    CppType_UChar,
    CppType_Short,
    CppType_UShort,
    CppType_Int,
    CppType_UInt,
    CppType_Long,
    CppType_ULong,
    CppType_LongLong,
    CppType_ULongLong,
    CppType_Float,
    CppType_Double,
    CppType_Pointer,
    CppType_Reference,
    CppType_LValueReference,
    CppType_Array,
    CppType_Struct,
    CppType_Union,
    CppType_Class,
    CppType_Enum,
};

typedef uint8_t CppTypeFlags;
enum {
    CppTypeFlag_Const    = 1 << 0,
    CppTypeFlag_Volatile = 1 << 1,
    CppTypeFlag_Restrict = 1 << 2,
};

typedef struct CppTypePointer {
    CppTypeKind kind;
    CppTypeFlags flags;
    struct CppType *pointer_to;
} CppTypePointer;

typedef struct CppTypeArray {
    CppTypeKind kind;
    CppTypeFlags flags;
    struct CppAst *array_to;
    struct CppAst *size;
} CppTypeArray;

typedef struct CppTypeNamed {
    CppTypeKind kind;
    CppTypeFlags flags;
    struct CppDeclaration *decl;
    const char *name;
    Array template_arguments;
} CppTypeNamed;

typedef struct CppType {
    CppTypeKind kind;
    CppTypeFlags flags;
    union {
        CppTypePointer type_pointer;
        CppTypePointer type_reference;
        CppTypeArray type_array;
        CppTypeNamed type_struct;
        CppTypeNamed type_union;
        CppTypeNamed type_class;
        CppTypeNamed type_enum;
    };
} CppType;

#endif
