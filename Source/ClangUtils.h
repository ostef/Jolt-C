#ifndef CLANG_UTILS_H
#define CLANG_UTILS_H

#include <clang-c/Index.h>

#include "Database.h"

static inline
const char *GetDeclName(CXCursor cursor) {
    if (clang_Cursor_isAnonymous(cursor)) {
        return "";
    }

    CXString spelling = clang_getCursorSpelling(cursor);
    const char *str = clang_getCString(spelling);

    return str;
}

static inline
CppVisibility AccessSpecifierToCppVisibility(enum CX_CXXAccessSpecifier spec) {
    switch (spec) {
    case CX_CXXPublic: return CppVisibility_Public;
    case CX_CXXProtected: return CppVisibility_Protected;
    case CX_CXXPrivate: return CppVisibility_Private;
    }

    return CppVisibility_Public;
}

static inline
CppVisibility GetCursorCppVisibility(CXCursor cursor) {
    enum CX_CXXAccessSpecifier spec = clang_getCXXAccessSpecifier(cursor);

    return AccessSpecifierToCppVisibility(spec);
}

static
CppType *GetCppType(CppDatabase *db, CXType type) {
    CppType *result = Alloc(CppType);
    result->cx_type = type;

    if (clang_isConstQualifiedType(type)) {
        result->flags |= CppTypeFlag_Const;
    }
    if (clang_isVolatileQualifiedType(type)) {
        result->flags |= CppTypeFlag_Volatile;
    }
    if (clang_isRestrictQualifiedType(type)) {
        result->flags |= CppTypeFlag_Restrict;
    }
    if (clang_isPODType(type)) {
        result->flags |= CppTypeFlag_IsPOD;
    }

    switch (type.kind) {
        default: {
            result->kind = CppType_Unknown;
        } break;
        case CXType_Invalid: {
            result->kind = CppType_Invalid;
        } break;
        case CXType_Void: {
            result->kind = CppType_Void;
        } break;
        case CXType_Bool: {
            result->kind = CppType_Bool;
        } break;
        case CXType_Char_U: {
            result->kind = CppType_UChar;
        } break;
        case CXType_UChar: {
            result->kind = CppType_UChar;
        } break;
        case CXType_UShort: {
            result->kind = CppType_UShort;
        } break;
        case CXType_UInt: {
            result->kind = CppType_UInt;
        } break;
        case CXType_ULong: {
            result->kind = CppType_ULong;
        } break;
        case CXType_ULongLong: {
            result->kind = CppType_ULongLong;
        } break;
        case CXType_UInt128: {
            result->kind = CppType_UInt128;
        } break;
        case CXType_Char_S: {
            result->kind = CppType_Char;
        } break;
        case CXType_SChar: {
            result->kind = CppType_Char;
        } break;
        case CXType_Short: {
            result->kind = CppType_Short;
        } break;
        case CXType_Int: {
            result->kind = CppType_Int;
        } break;
        case CXType_Long: {
            result->kind = CppType_Long;
        } break;
        case CXType_LongLong: {
            result->kind = CppType_LongLong;
        } break;
        case CXType_Int128: {
            result->kind = CppType_Int128;
        } break;
        case CXType_Float: {
            result->kind = CppType_Float;
        } break;
        case CXType_Double: {
            result->kind = CppType_Double;
        } break;
        case CXType_Pointer: {
            result->kind = CppType_Pointer;
            result->type_pointer.pointee_type = GetCppType(db, clang_getPointeeType(type));
        } break;
        case CXType_LValueReference: {
            result->kind = CppType_Reference;
            result->type_pointer.pointee_type = GetCppType(db, clang_getPointeeType(type));
        } break;
        case CXType_RValueReference: {
            result->kind = CppType_RValueReference;
            result->type_pointer.pointee_type = GetCppType(db, clang_getPointeeType(type));
        } break;
        case CXType_FunctionNoProto:
        case CXType_FunctionProto: {
            result->kind = CppType_Function;

            result->type_function.result_type = GetCppType(db, clang_getResultType(type));

            int num_parameters = clang_getNumArgTypes(type);
            ArrayReserve(&result->type_function.parameter_types, num_parameters);
            for (int i = 0; i < num_parameters; i += 1) {
                CppType *param_type = GetCppType(db, clang_getArgType(type, i));
                ArrayPush(&result->type_function.parameter_types, param_type);
            }

            result->type_function.is_variadic = clang_isFunctionTypeVariadic(type);
        } break;
        case CXType_IncompleteArray:
        case CXType_VariableArray:
        case CXType_DependentSizedArray:
        case CXType_ConstantArray: {
            result->kind = CppType_Array;
            result->type_array.element_type = GetCppType(db, clang_getElementType(type));
            result->type_array.num_elements = clang_getNumElements(type);
        } break;
        case CXType_Auto: {
            result->kind = CppType_Auto;
        } break;
        case CXType_Record:
        case CXType_Enum:
        case CXType_Typedef:
        case CXType_Elaborated: {
            result->kind = CppType_Named;
            result->type_named.name = clang_getCString(clang_getTypedefName(type));
            result->type_named.cursor = clang_getTypeDeclaration(type);
            result->type_named.entity = GetCppEntityFromCursor(db, result->type_named.cursor);
        } break;
    }

    return result;
}

static inline
CppSourceCodeRange GetCppSourceCodeRange(CXCursor cursor) {
    CXSourceRange range = clang_getCursorExtent(cursor);
    CXSourceLocation range_start = clang_getRangeStart(range);
    CXSourceLocation range_end = clang_getRangeEnd(range);

    CXFile file;
    unsigned int start_line, start_character, start_offset;
    clang_getSpellingLocation(range_start, &file, &start_line, &start_character, &start_offset);

    unsigned int end_line, end_character, end_offset;
    clang_getSpellingLocation(range_start, NULL, &end_line, &end_character, &end_offset);

    const char *filename = clang_getCString(clang_getFileName(file));

    return (CppSourceCodeRange){
        .filename=filename,
        .start_line=start_line, .start_character=start_character, .start_offset=start_offset,
        .end_line=end_line, .end_character=end_character, .end_offset=end_offset,
    };
}
#endif
