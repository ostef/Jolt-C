#ifndef CLANG_UTILS_H
#define CLANG_UTILS_H

#include <clang-c/Index.h>

#include "Database.h"

static inline
char *GetDeclName(CXCursor cursor) {
    if (clang_Cursor_isAnonymous(cursor)) {
        return "";
    }

    CXString spelling = clang_getCursorSpelling(cursor);

    return (char *)clang_getCString(spelling);
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
    result->size = clang_Type_getSizeOf(type);
    result->alignment = clang_Type_getAlignOf(type);

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
        case CXType_Unexposed: {
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
        case CXType_Char_U:
        case CXType_UChar:
        case CXType_UShort:
        case CXType_UInt:
        case CXType_ULong:
        case CXType_ULongLong:
        case CXType_UInt128: {
            switch (result->size) {
                case 1: {
                    result->kind = CppType_UInt8;
                } break;
                case 2: {
                    result->kind = CppType_UInt16;
                } break;
                case 4: {
                    result->kind = CppType_UInt32;
                } break;
                case 8: {
                    result->kind = CppType_UInt64;
                } break;
                case 16: {
                    result->kind = CppType_UInt128;
                } break;
                default: {
                    result->kind = CppType_Invalid;
                } break;
            }
        } break;
        case CXType_Char_S:
        case CXType_SChar:
        case CXType_Short:
        case CXType_Int:
        case CXType_Long:
        case CXType_LongLong:
        case CXType_Int128: {
            switch (result->size) {
                case 1: {
                    result->kind = CppType_Int8;
                } break;
                case 2: {
                    result->kind = CppType_Int16;
                } break;
                case 4: {
                    result->kind = CppType_Int32;
                } break;
                case 8: {
                    result->kind = CppType_Int64;
                } break;
                case 16: {
                    result->kind = CppType_Int128;
                } break;
                default: {
                    result->kind = CppType_Invalid;
                } break;
            }
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
    clang_getSpellingLocation(range_end, NULL, &end_line, &end_character, &end_offset);

    const char *filename = clang_getCString(clang_getFileName(file));

    return (CppSourceCodeRange){
        .filename=filename,
        .start_line=start_line, .start_character=start_character, .start_offset=start_offset,
        .end_line=end_line, .end_character=end_character, .end_offset=end_offset,
    };
}
#endif
