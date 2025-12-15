#include "Generate.h"
#include "ClangUtils.h"

static inline
void SBAppendIndentation(StringBuilder *builder, int indentation) {
    for (int i = 0; i < indentation; i += 1) {
        SBAppendString(builder, "    ");
    }
}

void AppendCppType(StringBuilder *builder, CppType *type, int indentation) {
    AppendCppTypePrefix(builder, type, indentation);
    AppendCppTypePostfix(builder, type, indentation);
}

void AppendCppTypePrefix(StringBuilder *builder, CppType *type, int indentation) {
    if (!type) {
        SBAppendString(builder, "<null>");
        return;
    }

    switch (type->kind) {
        case CppType_Invalid: {
            SBAppendString(builder, "<invalid>");
        } break;
        case CppType_Unknown: {
            SBAppend(builder, "< %s >", clang_getCString(clang_getTypeKindSpelling(type->cx_type.kind)));
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
            AppendCppTypePrefix(builder, type->type_pointer.pointee_type, indentation);
            SBAppendString(builder, "*");
        } break;
        case CppType_Reference: {
            AppendCppTypePrefix(builder, type->type_pointer.pointee_type, indentation);
            SBAppendString(builder, "&");
        } break;
        case CppType_RValueReference: {
            AppendCppTypePrefix(builder, type->type_pointer.pointee_type, indentation);
            SBAppendString(builder, "&&");
        } break;
        case CppType_Array: {
            AppendCppTypePrefix(builder, type->type_array.element_type, indentation);
        } break;
        case CppType_Enum: {
            AppendCppEnum(builder, type->type_enum.e, indentation);
        } break;
        case CppType_Aggregate: {
            AppendCppAggregate(builder, type->type_aggregate.aggr, indentation);
        } break;
        case CppType_Named: {
            if (type->type_named.entity) {
                SBAppendString(builder, type->type_named.entity->fully_qualified_c_name);
            } else if (!clang_Cursor_isNull(type->type_named.cursor)) {
                SBAppendString(builder, GetDeclName(type->type_named.cursor));
            } else if (type->type_named.name) {
                SBAppendString(builder, type->type_named.name);
            } else {
                SBAppendString(builder, "< ? named>");
            }
        } break;
        case CppType_Function: {
            AppendCppType(builder, type->type_function.result_type, indentation);
        } break;
        case CppType_Auto: {
            SBAppendString(builder, "auto");
        } break;
    }
}

void AppendCppTypePostfix(StringBuilder *builder, CppType *type, int indentation) {
    switch (type->kind) {
        case CppType_Function: {
            SBAppendString(builder, "(");
            foreach (i, type->type_function.parameter_types) {
                CppType *param = ArrayGet(type->type_function.parameter_types, i);

                if (i > 0) {
                    SBAppendString(builder, ", ");
                }

                AppendCppType(builder, param, indentation);
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

void AppendCppEnum(StringBuilder *builder, CppEnum *e, int indentation) {
    SBAppendString(builder, "enum {\n");

    foreach (i, e->constants) {
        CppEnumConstant *value = ArrayGet(e->constants, i);
        SBAppendIndentation(builder, indentation + 1);
        SBAppendString(builder, value->base.fully_qualified_c_name);
        SBAppend(builder, " = %llu,\n", value->value);
    }

    SBAppendIndentation(builder, indentation);
    SBAppendString(builder, "}");
}

void AppendCppEnumDecl(StringBuilder *builder, CppEnum *e, int indentation) {
    SBAppendString(builder, "typedef ");
    AppendCppTypePrefix(builder, e->base_type, indentation);
    SBAppend(builder, " %s", e->base.fully_qualified_c_name);
    AppendCppTypePostfix(builder, e->base_type, indentation);
    SBAppendString(builder, ";\n");

    SBAppendIndentation(builder, indentation);
    AppendCppEnum(builder, e, indentation);
    SBAppendString(builder, ";\n\n");
}

void AppendCppAggregate(StringBuilder *builder, CppAggregate *aggr, int indentation) {
    if (aggr->kind == CppAggregate_Union) {
        SBAppend(builder, "union");
    } else {
        SBAppend(builder, "struct");
    }

    if (aggr->base.fully_qualified_c_name) {
        SBAppend(builder, " %s", aggr->base.fully_qualified_c_name);
    }

    SBAppendString(builder, " {\n");

    foreach (i, aggr->entities) {
        CppEntity *e = ArrayGet(aggr->entities, i);
        if (e->kind != CppEntity_Variable) {
            continue;
        }

        if (e->flags & CppEntityFlag_Static) {
            continue;
        }

        CppVariable *var = (CppVariable *)e;

        SBAppendIndentation(builder, indentation + 1);
        AppendCppTypePrefix(builder, var->type, indentation + 1);
        if (var->base.name && var->base.name[0]) {
            SBAppend(builder, " %s", var->base.name);
        }
        AppendCppTypePostfix(builder, var->type, indentation + 1);
        SBAppendString(builder, ";\n");
    }

    SBAppendIndentation(builder, indentation);
    SBAppendString(builder, "}");
}

void GenerateCode(StringBuilder *builder, CppDatabase *db) {
    foreach (i, db->all_namespaces) {
        CppNamespace *ns = ArrayGet(db->all_namespaces, i);
        SBAppend(builder, "// Namespace %s\n", ns->base.fully_qualified_name);
    }

    SBAppendString(builder, "\n");

    foreach (i, db->all_entities) {
        CppEntity *entity = ArrayGet(db->all_entities, i);

        switch (entity->kind) {
            case CppEntity_Enum: {
                CppEnum *e = (CppEnum *)entity;
                AppendCppEnumDecl(builder, e, 0);
            } break;

            case CppEntity_Aggregate: {
                CppAggregate *aggr = (CppAggregate *)entity;
                SBAppendString(builder, "typedef ");
                AppendCppAggregate(builder, aggr, 0);
                SBAppend(builder, " %s;\n\n", aggr->base.fully_qualified_c_name);
            } break;

            case CppEntity_Typedef: {
                CppTypedef *ty = (CppTypedef *)entity;
                SBAppendString(builder, "typedef ");
                AppendCppTypePrefix(builder, ty->type, 0);
                SBAppend(builder, " %s", ty->base.fully_qualified_c_name);
                AppendCppTypePostfix(builder, ty->type, 0);
                SBAppend(builder, ";\n\n");
            } break;
        }
    }
}
