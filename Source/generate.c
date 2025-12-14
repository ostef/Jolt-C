#include "Database.h"
#include "ClangUtils.h"

static
void AppendCppTypePrefix(StringBuilder *builder, CppType *type);

static
void AppendCppTypePostfix(StringBuilder *builder, CppType *type);

static inline
void AppendCppType(StringBuilder *builder, CppType *type) {
    AppendCppTypePrefix(builder, type);
    AppendCppTypePostfix(builder, type);
}

static
void AppendCppTypePrefix(StringBuilder *builder, CppType *type) {
    if (!type) {
        SBAppendString(builder, "<null>");
        return;
    }

    switch (type->kind) {
    case CppType_Invalid: {
        SBAppendString(builder, "<invalid>");
    } break;
    case CppType_Unknown: {
        SBAppend(builder, "< %s >", clang_getCString(clang_getTypeSpelling(type->cx_type)));
    } break;
    case CppType_Void: {
        SBAppendString(builder, "void");
    } break;
    case CppType_Bool: {
        SBAppendString(builder, "bool");
    } break;
    case CppType_Char: {
        SBAppendString(builder, "char");
    } break;
    case CppType_UChar: {
        SBAppendString(builder, "unsigned char");
    } break;
    case CppType_Short: {
        SBAppendString(builder, "short");
    } break;
    case CppType_UShort: {
        SBAppendString(builder, "unsigned short");
    } break;
    case CppType_Int: {
        SBAppendString(builder, "int");
    } break;
    case CppType_UInt: {
        SBAppendString(builder, "unsigned int");
    } break;
    case CppType_Long: {
        SBAppendString(builder, "long");
    } break;
    case CppType_ULong: {
        SBAppendString(builder, "unsigned long");
    } break;
    case CppType_LongLong: {
        SBAppendString(builder, "long long");
    } break;
    case CppType_ULongLong: {
        SBAppendString(builder, "unsigned long long");
    } break;
    case CppType_Int128: {
        SBAppendString(builder, "int128_t");
    } break;
    case CppType_UInt128: {
        SBAppendString(builder, "uint128_t");
    } break;
    case CppType_Float: {
        SBAppendString(builder, "float");
    } break;
    case CppType_Double: {
        SBAppendString(builder, "double");
    } break;
    case CppType_Pointer: {
        AppendCppTypePrefix(builder, type->type_pointer.pointee_type);
        SBAppendString(builder, "*");
    } break;
    case CppType_Reference: {
        AppendCppTypePrefix(builder, type->type_pointer.pointee_type);
        SBAppendString(builder, "&");
    } break;
    case CppType_RValueReference: {
        AppendCppTypePrefix(builder, type->type_pointer.pointee_type);
        SBAppendString(builder, "&&");
    } break;
    case CppType_Array: {
        AppendCppTypePrefix(builder, type->type_array.element_type);
    } break;
    case CppType_Named: {
        if (type->type_named.entity) {
            SBAppendString(builder, type->type_named.entity->fully_qualified_c_name);
        } else {
            SBAppendString(builder, GetDeclName(type->type_named.cursor));
        }
    } break;
    case CppType_Function: {
        AppendCppType(builder, type->type_function.result_type);
    } break;
    case CppType_Auto: {
        SBAppendString(builder, "auto");
    } break;
    }
}

static
void AppendCppTypePostfix(StringBuilder *builder, CppType *type) {
    switch (type->kind) {
        case CppType_Function: {
            SBAppendString(builder, "(");
            foreach (i, type->type_function.parameter_types) {
                CppType *param = ArrayGet(type->type_function.parameter_types, i);

                if (i > 0) {
                    SBAppendString(builder, ", ");
                }

                AppendCppType(builder, param);
            }
            SBAppendString(builder, ")");
        } break;
        case CppType_Array: {
            if (type->type_array.num_elements >= 0) {
                SBAppend(builder, "[%u]", type->type_array.num_elements);
            } else {
                SBAppendString(builder, "[]");
            }
        } break;
    }
}

static
void AppendCppAggregate(StringBuilder *builder, CppAggregate *aggr) {
    if (aggr->kind == CppAggregate_Union) {
        SBAppend(builder, "typedef union %s {\n", aggr->base.fully_qualified_c_name);
    } else {
        SBAppend(builder, "typedef struct %s {\n", aggr->base.fully_qualified_c_name);
    }

    foreach (i, aggr->entities) {
        CppEntity *e = ArrayGet(aggr->entities, i);
        if (e->kind != CppEntity_Variable) {
            continue;
        }

        CppVariable *var = (CppVariable *)e;

        SBAppendString(builder, "    ");
        AppendCppTypePrefix(builder, var->type);
        SBAppend(builder, " %s", var->base.name);
        AppendCppTypePostfix(builder, var->type);
        SBAppendString(builder, ";\n");
    }

    SBAppend(builder, "} %s;\n", aggr->base.fully_qualified_c_name);
}

void GenerateCode(StringBuilder *builder, CppDatabase *db) {
    foreach (i, db->all_entities) {
        CppEntity *entity = ArrayGet(db->all_entities, i);
        // SBAppend(builder, "%s is %s\n", entity->fully_qualified_name, CppEntityKind_Str[entity->kind]);

        switch (entity->kind) {
            case CppEntity_Aggregate: {
                AppendCppAggregate(builder, (CppAggregate *)entity);
                SBAppendString(builder, "\n");
            } break;
        }
    }
}
