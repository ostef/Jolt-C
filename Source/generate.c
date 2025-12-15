#include "Generate.h"
#include "ClangUtils.h"

static inline
void SBAppendIndentation(StringBuilder *builder, int indentation) {
    for (int i = 0; i < indentation; i += 1) {
        SBAppendString(builder, "    ");
    }
}

void AppendCppSourceCodeLocation(StringBuilder *builder, CppSourceCodeLocation loc) {
    SBAppend(builder, "%s:%d:%d", loc.filename, (int)loc.line, (int)loc.character);
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
        case CppType_Reference: // Append references as pointers
        case CppType_Pointer: {
            AppendCppTypePrefix(builder, type->type_pointer.pointee_type, indentation);
            if (type->type_pointer.pointee_type->kind == CppType_Function) {
                SBAppendString(builder, " (");
            }
            SBAppendString(builder, "*");
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
            } else if (type->type_named.name && type->type_named.name[0]) {
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
        case CppType_Pointer: {
            if (type->type_pointer.pointee_type->kind == CppType_Function) {
                SBAppendString(builder, ")");
            }
            AppendCppTypePostfix(builder, type->type_pointer.pointee_type, indentation);
        } break;
        case CppType_Array: {
            AppendCppTypePostfix(builder, type->type_array.element_type, indentation);
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

    foreach (i, aggr->base_classes) {
        CppBaseClass *base = ArrayGet(aggr->base_classes, i);
        assert(!base->is_virtual && "Virtual base classes are not supported");

        CppAggregate *base_aggr = NULL;
        if (base->type->kind == CppType_Named && base->type->type_named.entity != NULL && base->type->type_named.entity->kind == CppEntity_Aggregate) {
            base_aggr = (CppAggregate *)base->type->type_named.entity;
        }

        // Inherited base classes with no fields size 0 because of EBO, so don't include them
        if (base_aggr && base_aggr->fields.count == 0) {
            SBAppendIndentation(builder, indentation + 1);
            SBAppendString(builder, "// ");
            AppendCppType(builder, base->type, indentation + 1);
            SBAppendString(builder, " base class has size 0, so it is not included\n");
        } else {
            SBAppendIndentation(builder, indentation + 1);
            AppendCppTypePrefix(builder, base->type, indentation + 1);

            if (aggr->base_classes.count == 1) {
                SBAppendString(builder, " base");
            } else if (base->type->kind == CppType_Named && base->type->type_named.entity != NULL) {
                SBAppend(builder, " base%s", base->type->type_named.entity->name);
            } else {
                SBAppend(builder, " base%d", i);
            }

            AppendCppTypePostfix(builder, base->type, indentation + 1);
            SBAppendString(builder, ";\n");
        }
    }

    foreach (i, aggr->fields) {
        if (i == 0 && aggr->base_classes.count > 0) {
            SBAppendString(builder, "\n");
        }

        CppVariable *var = ArrayGet(aggr->fields, i);

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

void AppendCppAggregateForwardDecl(StringBuilder *builder, CppAggregate *aggr) {
    switch (aggr->kind) {
        case CppAggregate_Class:
        case CppAggregate_Struct: {
            SBAppendString(builder, "struct");
        } break;
        case CppAggregate_Union: {
            SBAppendString(builder, "union");
        } break;
    }

    SBAppend(builder, " %s;\n", aggr->base.fully_qualified_c_name);
}

static
bool ShouldExclude(GenerateOptions options, const char *entity_name) {
    foreach (i, options.declarations_to_exclude) {
        const char *name = ArrayGet(options.declarations_to_exclude, i);
        if (StrEq(entity_name, name)) {
            return true;
        }
    }

    return false;
}

void GenerateCode(GenerateOptions options, StringBuilder *builder, CppDatabase *db) {
    SBAppendString(builder, "// This file was autogenerated by parsing the C++ headers using libclang\n\n");

    SBAppendString(builder, "#include <stddef.h>\n");
    SBAppendString(builder, "#include <stdint.h>\n");
    SBAppendString(builder, "#include <stdbool.h>\n");
    SBAppendString(builder, "\n");

    foreach (i, db->all_namespaces) {
        CppNamespace *ns = ArrayGet(db->all_namespaces, i);
        if (ns->base.fully_qualified_name[0]) {
            SBAppend(builder, "// Namespace %s\n", ns->base.fully_qualified_name);
        }
    }

    SBAppendString(builder, "\n// Forward declarations\n\n");

    foreach (i, db->all_aggregates) {
        CppAggregate *aggr = ArrayGet(db->all_aggregates, i);

        if (ShouldExclude(options, aggr->base.fully_qualified_name)) {
            continue;
        }

        if (aggr->base.flags & CppEntityFlag_ForwardDecl) {
            continue;
        }

        AppendCppAggregateForwardDecl(builder, aggr);
    }

    SBAppendString(builder, "\n// Enums\n\n");

    foreach (i, db->all_enums) {
        CppEnum *e = ArrayGet(db->all_enums, i);

        if (ShouldExclude(options, e->base.fully_qualified_name)) {
            continue;
        }

        if (e->base.flags & CppEntityFlag_ForwardDecl) {
            continue;
        }

        SBAppendString(builder, "// ");
        AppendCppSourceCodeLocation(builder, GetStartLocation(e->base.source_code_range));
        SBAppendString(builder, "\n");

        AppendCppEnumDecl(builder, e, 0);
    }

    SBAppendString(builder, "\n");

    SBAppendString(builder, "\n// Classes and typedefs\n\n");

    foreach (i, db->all_entities) {
        CppEntity *entity = ArrayGet(db->all_entities, i);

        if (ShouldExclude(options, entity->fully_qualified_name)) {
            continue;
        }

        switch (entity->kind) {
            case CppEntity_Aggregate: {
                CppAggregate *aggr = (CppAggregate *)entity;

                if (aggr->base.flags & CppEntityFlag_ForwardDecl) {
                    continue;
                }

                SBAppendString(builder, "// ");
                AppendCppSourceCodeLocation(builder, GetStartLocation(entity->source_code_range));
                SBAppendString(builder, "\n");

                SBAppendString(builder, "typedef ");
                AppendCppAggregate(builder, aggr, 0);
                SBAppend(builder, " %s;\n\n", aggr->base.fully_qualified_c_name);
            } break;

            case CppEntity_Typedef: {
                CppTypedef *ty = (CppTypedef *)entity;

                SBAppendString(builder, "// ");
                AppendCppSourceCodeLocation(builder, GetStartLocation(entity->source_code_range));
                SBAppendString(builder, "\n");

                SBAppendString(builder, "typedef ");
                AppendCppTypePrefix(builder, ty->type, 0);
                SBAppend(builder, " %s", ty->base.fully_qualified_c_name);
                AppendCppTypePostfix(builder, ty->type, 0);
                SBAppend(builder, ";\n\n");
            } break;
        }
    }
}
