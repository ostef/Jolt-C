#include "Generate.h"
#include "ClangUtils.h"

static
bool ShouldExclude(GenerateOptions options, const char *entity_name) {
    return ArrayFindFirstPredicate(options.declarations_to_exclude, entity_name, StringCompareFunc) >= 0;
}

static
bool ShouldUnwrap(GenerateOptions options, const char *entity_name) {
    return ArrayFindFirstPredicate(options.typedefs_to_unwrap, entity_name, StringCompareFunc) >= 0;
}

static
bool ShouldPrintSpaceAfterType(CppType *type) {
    return type->kind != CppType_Pointer && type->kind != CppType_Reference;
}

// @Todo: make sure types that contain opaque types are also treated as opaque (apart from 0 size base classes)
static
bool ShouldBeOpaque(CppAggregate *aggr) {
    return aggr->virtual_methods.count > 0 || aggr->fields.count == 0 || aggr->type->size == 0;
}

static inline
void SBAppendIndentation(StringBuilder *builder, int indentation) {
    for (int i = 0; i < indentation; i += 1) {
        SBAppendString(builder, "    ");
    }
}

void AppendCppSourceCodeLocation(GenerateContext *ctx, CppSourceCodeLocation loc) {
    SBAppend(ctx->builder, "%s:%d:%d", loc.filename, (int)loc.line, (int)loc.character);
}

void AppendCppType(GenerateContext *ctx, CppType *type, int indentation) {
    AppendCppTypePrefix(ctx, type, indentation);
    AppendCppTypePostfix(ctx, type, indentation);
}

void AppendCppTypePrefix(GenerateContext *ctx, CppType *type, int indentation) {
    if (!type) {
        SBAppendString(ctx->builder, "<null>");
        return;
    }

    switch (type->kind) {
        case CppType_Invalid: {
            SBAppendString(ctx->builder, "<invalid>");
        } break;
        case CppType_Unknown: {
            SBAppend(ctx->builder, "< %s (size=%d, align=%d)>", clang_getCString(clang_getTypeKindSpelling(type->cx_type.kind)), (int)type->size, (int)type->alignment);
        } break;
        default: {
            SBAppendString(ctx->builder, CppTypeKind_Str[type->kind]);
        } break;
        case CppType_Reference: // Append references as pointers
        case CppType_Pointer: {
            AppendCppTypePrefix(ctx, type->type_pointer.pointee_type, indentation);

            bool space;
            if (type->type_pointer.pointee_type->kind == CppType_Function) {
                space = ShouldPrintSpaceAfterType(type->type_pointer.pointee_type->type_function.result_type);
            } else {
                space = ShouldPrintSpaceAfterType(type->type_pointer.pointee_type);
            }

            if (space) {
                SBAppendString(ctx->builder, " ");
            }

            if (type->type_pointer.pointee_type->kind == CppType_Function) {
                SBAppendString(ctx->builder, "(");
            }
            SBAppendString(ctx->builder, "*");
        } break;
        case CppType_RValueReference: {
            AppendCppTypePrefix(ctx, type->type_pointer.pointee_type, indentation);
            SBAppendString(ctx->builder, "&&");
        } break;
        case CppType_Array: {
            AppendCppTypePrefix(ctx, type->type_array.element_type, indentation);
        } break;
        case CppType_Enum: {
            AppendCppEnum(ctx, type->type_enum.e, indentation);
        } break;
        case CppType_Aggregate: {
            AppendCppAggregate(ctx, type->type_aggregate.aggr, indentation);
        } break;
        case CppType_Named: {
            if (type->type_named.entity) {
                CppEntity *e = type->type_named.entity;
                if (e->kind == CppEntity_Typedef && ShouldUnwrap(ctx->options, e->fully_qualified_name)) {
                    CppTypedef *ty = (CppTypedef *)e;
                    AppendCppTypePrefix(ctx, ty->type, indentation);
                    return;
                }

                SBAppendString(ctx->builder, type->type_named.entity->fully_qualified_c_name);
            } else if (type->type_named.name && type->type_named.name[0]) {
                SBAppendString(ctx->builder, type->type_named.name);
            } else {
                SBAppend(ctx->builder, "< ? named (size=%d, align=%d)>", (int)type->size, (int)type->alignment);
            }
        } break;
        case CppType_Function: {
            AppendCppType(ctx, type->type_function.result_type, indentation);
        } break;
        case CppType_Auto: {
            SBAppendString(ctx->builder, "auto");
        } break;
    }
}

void AppendCppTypePostfix(GenerateContext *ctx, CppType *type, int indentation) {
    switch (type->kind) {
        case CppType_Named: {
            if (type->type_named.entity) {
                CppEntity *e = type->type_named.entity;
                if (e->kind == CppEntity_Typedef && ShouldUnwrap(ctx->options, e->fully_qualified_name)) {
                    CppTypedef *ty = (CppTypedef *)e;
                    AppendCppTypePostfix(ctx, ty->type, indentation);
                    return;
                }
            }
        } break;
        case CppType_Function: {
            SBAppendString(ctx->builder, "(");
            foreach (i, type->type_function.parameter_types) {
                CppType *param = ArrayGet(type->type_function.parameter_types, i);

                if (i > 0) {
                    SBAppendString(ctx->builder, ", ");
                }

                AppendCppType(ctx, param, indentation);
            }
            SBAppendString(ctx->builder, ")");
        } break;
        case CppType_Pointer: {
            if (type->type_pointer.pointee_type->kind == CppType_Function) {
                SBAppendString(ctx->builder, ")");
            }
            AppendCppTypePostfix(ctx, type->type_pointer.pointee_type, indentation);
        } break;
        case CppType_Array: {
            AppendCppTypePostfix(ctx, type->type_array.element_type, indentation);
            if (type->type_array.num_elements >= 0) {
                SBAppend(ctx->builder, "[%u]", type->type_array.num_elements);
            } else {
                SBAppendString(ctx->builder, "[]");
            }
        } break;
    }
}

void AppendCppVariable(GenerateContext *ctx, CppVariable *var, bool full_name, int indentation) {
    AppendCppTypePrefix(ctx, var->type, indentation);

    const char *name = full_name ? var->base.fully_qualified_c_name : var->base.name;
    if (name && name[0]) {
        if (ShouldPrintSpaceAfterType(var->type)) {
            SBAppendString(ctx->builder, " ");
        }

        SBAppendString(ctx->builder, name);
    }

    AppendCppTypePostfix(ctx, var->type, indentation);
}

void AppendCppEnum(GenerateContext *ctx, CppEnum *e, int indentation) {
    SBAppendString(ctx->builder, "enum {\n");

    foreach (i, e->constants) {
        CppEnumConstant *value = ArrayGet(e->constants, i);
        SBAppendIndentation(ctx->builder, indentation + 1);
        SBAppendString(ctx->builder, value->base.fully_qualified_c_name);
        SBAppend(ctx->builder, " = %llu,\n", value->value);
    }

    SBAppendIndentation(ctx->builder, indentation);
    SBAppendString(ctx->builder, "}");
}

void AppendCppEnumDecl(GenerateContext *ctx, CppEnum *e, int indentation) {
    SBAppendString(ctx->builder, "typedef ");
    AppendCppTypePrefix(ctx, e->base_type, indentation);

    if (ShouldPrintSpaceAfterType(e->base_type)) {
        SBAppendString(ctx->builder, " ");
    }
    SBAppendString(ctx->builder, e->base.fully_qualified_c_name);

    AppendCppTypePostfix(ctx, e->base_type, indentation);
    SBAppendString(ctx->builder, ";\n");

    SBAppendIndentation(ctx->builder, indentation);
    AppendCppEnum(ctx, e, indentation);
    SBAppendString(ctx->builder, ";\n\n");
}

void AppendCppAggregate(GenerateContext *ctx, CppAggregate *aggr, int indentation) {
    if (aggr->kind == CppAggregate_Union) {
        SBAppend(ctx->builder, "union");
    } else {
        SBAppend(ctx->builder, "struct");
    }

    if (aggr->base.fully_qualified_c_name) {
        SBAppend(ctx->builder, " %s", aggr->base.fully_qualified_c_name);
    }

    if (ShouldBeOpaque(aggr)) {
        return;
    }

    SBAppendString(ctx->builder, " {\n");

    foreach (i, aggr->base_classes) {
        CppBaseClass *base = ArrayGet(aggr->base_classes, i);
        assert(!base->is_virtual && "Virtual base classes are not supported");

        CppAggregate *base_aggr = NULL;
        if (base->type->kind == CppType_Named && base->type->type_named.entity != NULL && base->type->type_named.entity->kind == CppEntity_Aggregate) {
            base_aggr = (CppAggregate *)base->type->type_named.entity;
        }

        // Inherited base classes with no fields and no vtable has size 0 because of EBO, so don't include them
        if (base_aggr && base_aggr->fields.count == 0 && base_aggr->virtual_methods.count == 0) {
            SBAppendIndentation(ctx->builder, indentation + 1);
            SBAppendString(ctx->builder, "// ");
            AppendCppType(ctx, base->type, indentation + 1);
            SBAppendString(ctx->builder, " base class has size 0, so it is not included\n");
        } else {
            SBAppendIndentation(ctx->builder, indentation + 1);
            AppendCppTypePrefix(ctx, base->type, indentation + 1);

            if (ShouldPrintSpaceAfterType(base->type)) {
                SBAppendString(ctx->builder, " ");
            }

            if (aggr->base_classes.count == 1) {
                SBAppendString(ctx->builder, "base");
            } else if (base->type->kind == CppType_Named && base->type->type_named.entity != NULL) {
                SBAppend(ctx->builder, "base%s", base->type->type_named.entity->name);
            } else {
                SBAppend(ctx->builder, "base%d", i);
            }

            AppendCppTypePostfix(ctx, base->type, indentation + 1);
            SBAppendString(ctx->builder, ";\n");
        }
    }

    foreach (i, aggr->fields) {
        if (i == 0 && aggr->base_classes.count > 0) {
            SBAppendString(ctx->builder, "\n");
        }

        CppVariable *var = ArrayGet(aggr->fields, i);

        SBAppendIndentation(ctx->builder, indentation + 1);
        AppendCppVariable(ctx, var, false, indentation + 1);
        SBAppendString(ctx->builder, ";\n");
    }

    SBAppendIndentation(ctx->builder, indentation);
    SBAppendString(ctx->builder, "}");
}

void AppendCppAggregateForwardDecl(GenerateContext *ctx, CppAggregate *aggr) {
    switch (aggr->kind) {
        case CppAggregate_Class:
        case CppAggregate_Struct: {
            SBAppendString(ctx->builder, "struct");
        } break;
        case CppAggregate_Union: {
            SBAppendString(ctx->builder, "union");
        } break;
    }

    SBAppend(ctx->builder, " %s;\n", aggr->base.fully_qualified_c_name);
}

void AppendCppFunction(GenerateContext *ctx, CppFunction *func, int indentation) {
    AppendCppType(ctx, func->result_type, indentation);
    if (ShouldPrintSpaceAfterType(func->result_type)) {
        SBAppendString(ctx->builder, " ");
    }
    SBAppendString(ctx->builder, func->base.fully_qualified_c_name);
    SBAppendString(ctx->builder, "(");

    foreach (i, func->parameters) {
        if (i > 0) {
            SBAppendString(ctx->builder, ", ");
        }

        CppVariable *param = ArrayGet(func->parameters, i);
        AppendCppVariable(ctx, param, false, indentation + 1);
    }

    SBAppendString(ctx->builder, ")");
}

void GenerateCode(GenerateOptions options, StringBuilder *builder, CppDatabase *db) {
    GenerateContext ctx = {};
    ctx.options = options;
    ctx.builder = builder;

    SBAppendString(builder, "// This file was autogenerated by parsing the C++ headers using libclang\n\n");

    SBAppendString(builder, "#ifndef JOLTC_H\n");
    SBAppendString(builder, "#define JOLTC_H\n\n");

    SBAppendString(builder, "#include <stddef.h>\n");
    SBAppendString(builder, "#include <stdint.h>\n");
    SBAppendString(builder, "#include <stdbool.h>\n");
    SBAppendString(builder, "\n");

    if (options.preamble) {
        SBAppendString(builder, options.preamble);
    }

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

        AppendCppAggregateForwardDecl(&ctx, aggr);
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
        AppendCppSourceCodeLocation(&ctx, GetStartLocation(e->base.source_code_range));
        SBAppendString(builder, "\n");

        AppendCppEnumDecl(&ctx, e, 0);
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
                AppendCppSourceCodeLocation(&ctx, GetStartLocation(entity->source_code_range));
                SBAppendString(builder, "\n");

                if (aggr->flags & CppAggregateFlag_Abstract) {
                    SBAppendString(builder, "// Abstract\n");
                } else if (aggr->virtual_methods.count > 0) {
                    SBAppendString(builder, "// Has vtable\n");
                }
                SBAppendString(builder, "typedef ");
                AppendCppAggregate(&ctx, aggr, 0);
                SBAppend(builder, " %s;\n\n", aggr->base.fully_qualified_c_name);

                foreach (j, aggr->entities) {
                    CppEntity *e = ArrayGet(aggr->entities, j);
                    if (e->kind != CppEntity_Function) {
                        continue;
                    }

                    CppFunction *func = (CppFunction *)e;
                    AppendCppFunction(&ctx, func, 0);
                    SBAppendString(ctx.builder, ";\n");
                }
            } break;

            case CppEntity_Typedef: {
                CppTypedef *ty = (CppTypedef *)entity;

                if (ShouldUnwrap(options, ty->base.fully_qualified_name)) {
                    continue;
                }

                SBAppendString(builder, "// ");
                AppendCppSourceCodeLocation(&ctx, GetStartLocation(entity->source_code_range));
                SBAppendString(builder, "\n");

                SBAppendString(builder, "typedef ");
                AppendCppTypePrefix(&ctx, ty->type, 0);

                if (ShouldPrintSpaceAfterType(ty->type)) {
                    SBAppendString(builder, " ");
                }
                SBAppendString(builder, ty->base.fully_qualified_c_name);

                AppendCppTypePostfix(&ctx, ty->type, 0);
                SBAppend(builder, ";\n\n");
            } break;
        }
    }

    if (options.postamble) {
        SBAppendString(builder, options.postamble);
    }

    SBAppendString(builder, "\n#endif // Include guard\n");
}
