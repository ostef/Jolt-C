#include "Generate.h"
#include "ClangUtils.h"

static
bool ShouldExcludeEntity(GenerateOptions options, CppEntity *entity) {
    if (entity->flags & CppEntityFlag_ParentIsTemplate) {
        return true;
    }

    if (entity->kind == CppEntity_Aggregate && (((CppAggregate *)entity)->flags & CppAggregateFlag_Template)) {
        return true;
    }

    if (options.exclude_non_public_entities && entity->visibility != CppVisibility_Public) {
        return true;
    }

    if (ArrayFindFirstPredicate(options.declarations_to_exclude, entity->fully_qualified_name, StringCompareFunc) >= 0) {
        return true;
    }
    if (ArrayFindFirstPredicate(options.declarations_to_exclude, entity->name, StringCompareFunc) >= 0) {
        return true;
    }

    if (options.declarations_to_include.count > 0) {
        if (ArrayFindFirstPredicate(options.declarations_to_include, entity->fully_qualified_name, StringCompareFunc) >= 0) {
            return false;
        }
        if (ArrayFindFirstPredicate(options.declarations_to_include, entity->name, StringCompareFunc) >= 0) {
            return false;
        }

        return true;
    } else {
        return false;
    }
}

static
bool ShouldUnwrap(GenerateOptions options, const char *entity_name) {
    return ArrayFindFirstPredicate(options.typedefs_to_unwrap, entity_name, StringCompareFunc) >= 0;
}

static
bool ShouldPrintSpaceAfterType(CppType *type) {
    return type->kind != CppType_Pointer && type->kind != CppType_Reference;
}

CppType *UnwrapTemplate(GenerateOptions options, CppDatabase *db, CppType *type) {
    if (!options.template_unwrap_func) {
        return type;
    }

    if (type->kind != CppType_Named || type->type_named.template_type_arguments.count == 0) {
        return type;
    }

    CppType *new_type = options.template_unwrap_func(options, db, type);
    if (type->size >= 0 && new_type->size != type->size) {
        printf("WARNING: when unwrapping template %s, new type has size %d and old type has size %d\n", type->type_named.name, (int)new_type->size, (int)type->size);
    }
    if (type->alignment >= 0 && new_type->alignment != type->alignment) {
        printf("WARNING: when unwrapping template %s, new type has alignment %d and old type has alignment %d\n", type->type_named.name, (int)new_type->alignment, (int)type->alignment);
    }

    return new_type;
}

// @Todo: make sure types that contain opaque types are also treated as opaque (apart from 0 size base classes)
static
bool ShouldBeOpaque(CppAggregate *aggr) {
    return false;

    // return aggr->virtual_methods.count > 0 || aggr->fields.count == 0 || aggr->type->size == 0;
}

bool CppTypesAreEqual(CppType *a, CppType *b) {
    if (a == b) {
        return true;
    }

    if (a->kind != b->kind) {
        return false;
    }

    if (a->size != b->size || a->alignment != b->alignment) {
        return false;
    }

    if (a->flags != b->flags) {
        return false;
    }

    switch (a->kind) {
        case CppType_Unknown: {
            return false;
        } break;

        case CppType_Invalid:
        case CppType_Void:
        case CppType_Bool:
        case CppType_Char:
        case CppType_UInt8:
        case CppType_UInt16:
        case CppType_UInt32:
        case CppType_UInt64:
        case CppType_UInt128:
        case CppType_Int8:
        case CppType_Int16:
        case CppType_Int32:
        case CppType_Int64:
        case CppType_Int128:
        case CppType_Float:
        case CppType_Double:
        case CppType_Auto: {
            return true;
        } break;

        case CppType_Pointer:
        case CppType_Reference:
        case CppType_RValueReference: {
            return CppTypesAreEqual(a->type_pointer.pointee_type, b->type_pointer.pointee_type);
        } break;
        case CppType_Array: {
            if (a->type_array.num_elements != b->type_array.num_elements) {
                return false;
            }

            return CppTypesAreEqual(a->type_array.element_type, b->type_array.element_type);
        } break;

        case CppType_Aggregate: {
            return a->type_aggregate.aggr == b->type_aggregate.aggr;
        } break;
        case CppType_Enum: {
            return a->type_enum.e == b->type_enum.e;
        } break;
        case CppType_Named: {
            if (a->type_named.entity != b->type_named.entity) {
                return false;
            }

            if (!a->type_named.entity && !StrEq(a->type_named.name, b->type_named.name)) {
                return false;
            }

            if (a->type_named.template_type_arguments.count != b->type_named.template_type_arguments.count) {
                return false;
            }

            foreach (i, a->type_named.template_type_arguments) {
                CppType *a_arg = ArrayGet(a->type_named.template_type_arguments, i);
                CppType *b_arg = ArrayGet(b->type_named.template_type_arguments, i);
                if (!CppTypesAreEqual(a_arg, b_arg)) {
                    return false;
                }
            }

            return true;
        } break;

        case CppType_Function: {
            if (a->type_function.parameter_types.count != b->type_function.parameter_types.count) {
                return false;
            }

            if (!CppTypesAreEqual(a->type_function.result_type, b->type_function.result_type)) {
                return false;
            }

            foreach (i, a->type_function.parameter_types) {
                CppType *a_param = ArrayGet(a->type_function.parameter_types, i);
                CppType *b_param = ArrayGet(b->type_function.parameter_types, i);

                if (!CppTypesAreEqual(a_param, b_param)) {
                    return false;
                }
            }

            return true;
        } break;
    }

    return false;
}

bool CppFunctionsHaveSameParamTypes(CppFunction *a, CppFunction *b) {
    if (a->parameters.count != b->parameters.count) {
        return false;
    }

    foreach (i, a->parameters) {
        CppVariable *a_param = ArrayGet(a->parameters, i);
        CppVariable *b_param = ArrayGet(b->parameters, i);
        if (!CppTypesAreEqual(a_param->type, b_param->type)) {
            return false;
        }
    }

    return true;
}

bool CppFunctionsHaveSameParamNames(CppFunction *a, CppFunction *b) {
    if (a->parameters.count != b->parameters.count) {
        return false;
    }

    foreach (i, a->parameters) {
        CppVariable *a_param = ArrayGet(a->parameters, i);
        CppVariable *b_param = ArrayGet(b->parameters, i);
        if (!StrEq(a_param->base.c_name, b_param->base.c_name)) {
            return false;
        }
    }

    return true;
}

bool CppFunctionsHaveSameParams(CppFunction *a, CppFunction *b) {
    if (a->parameters.count != b->parameters.count) {
        return false;
    }

    foreach (i, a->parameters) {
        CppVariable *a_param = ArrayGet(a->parameters, i);
        CppVariable *b_param = ArrayGet(b->parameters, i);
        if (!CppTypesAreEqual(a_param->type, b_param->type)) {
            return false;
        }
        if (!StrEq(a_param->base.c_name, b_param->base.c_name)) {
            return false;
        }
    }

    return true;
}

void SetUniqueCName(CppEntity *entity, char *name) {
    entity->unique_c_name = name;

    if (entity->parent && entity->parent->fully_qualified_name && entity->parent->fully_qualified_name[0]) {
        entity->unique_fully_qualified_c_name = SPrintf("%s_%s", entity->parent->fully_qualified_c_name, entity->unique_c_name);
    } else {
        entity->unique_fully_qualified_c_name = entity->unique_c_name;
    }
}

void AppendOverloadParamName(StringBuilder *builder, const char *name) {
    if (StrStartsWith(name, "in")) {
        name += 2;
    } else if (StrStartsWith(name, "out")) {
        name += 3;
    }

    if (islower(name[0])) {
        SBAppendByte(builder, toupper(name[0]));
        name += 1;
    }

    SBAppendString(builder, name);
}

void AppendAlphaNumericCType(GenerateContext *ctx, CppType *type) {
    if (!type) {
        SBAppendString(ctx->builder, "<null>");
        return;
    }

    // if (type->flags & CppTypeFlag_Const) {
    //     SBAppendString(ctx->builder, "Const");
    // }

    type = UnwrapTemplate(ctx->options, ctx->db, type);

    switch (type->kind) {
        default: {
            SBAppendPascalCase(ctx->builder, CppTypeKind_Str[type->kind]);
        } break;
        case CppType_SIMDVector: {
            SBAppendString(ctx->builder, clang_getCString(clang_getTypeSpelling(type->cx_type)));
        } break;

        case CppType_Named: {
            if (type->type_named.entity) {
                CppEntity *e = type->type_named.entity;
                if (e->kind == CppEntity_Typedef && ShouldUnwrap(ctx->options, e->fully_qualified_name)) {
                    CppTypedef *ty = (CppTypedef *)e;
                    AppendAlphaNumericCType(ctx, ty->type);
                } else {
                    SBAppendString(ctx->builder, e->c_name);
                }
            } else {
                SBAppendPascalCase(ctx->builder, CppTypeKind_Str[type->kind]);
            }
        } break;
        case CppType_Pointer:{
            AppendAlphaNumericCType(ctx, type->type_pointer.pointee_type);
            SBAppend(ctx->builder, "Ptr");
        } break;
        case CppType_Reference:
        case CppType_RValueReference: {
            AppendAlphaNumericCType(ctx, type->type_pointer.pointee_type);
        } break;
        case CppType_Array: {
            AppendAlphaNumericCType(ctx, type->type_array.element_type);
            SBAppend(ctx->builder, "Array");
        } break;
    }
}

static
void GenerateOpaqueAggrNewFunction(GenerateOptions options, CppDatabase *db, CppAggregate *aggr, CppFunction *func) {
    CppFunction *new_func = AllocCppEntity(Function, clang_getNullCursor());

    new_func->base.name = "New";
    new_func->base.c_name = "New";
    if (func) {
        new_func->base.flags = func->base.flags;
        new_func->base.visibility = func->base.visibility;
        new_func->base.source_code_range = func->base.source_code_range;
    }
    new_func->base.user_flags |= CppEntityUserFlag_NewFunction;

    if (func) {
        new_func->flags = func->flags;
    }
    new_func->flags &= (~CppFunctionFlag_Constructor);
    new_func->flags &= (~CppFunctionFlag_Method);

    new_func->result_type = Alloc(CppType);
    new_func->result_type->kind = CppType_Pointer;
    new_func->result_type->type_pointer.pointee_type = aggr->type;

    if (func) {
        new_func->type = func->type;
        new_func->parameters = func->parameters;
    } else {
        new_func->type = Alloc(CppType);
        new_func->type->kind = CppType_Function;
        new_func->type->type_function.result_type = new_func->result_type;
    }

    PushCppEntity(db, &aggr->base, &new_func->base);
}

static
void GenerateOpaqueAggrDeleteFunction(GenerateOptions options, CppDatabase *db, CppAggregate *aggr) {
    CppFunction *del_func = AllocCppEntity(Function, clang_getNullCursor());

    del_func->base.name = "Delete";
    del_func->base.c_name = "Delete";
    del_func->base.user_flags |= CppEntityUserFlag_DeleteFunction;

    del_func->flags = CppFunctionFlag_Method;

    del_func->result_type = Alloc(CppType);
    del_func->result_type->kind = CppType_Void;

    del_func->type = Alloc(CppType);
    del_func->type->kind = CppType_Function;
    del_func->type->type_function.result_type = del_func->result_type;

    PushCppEntity(db, &aggr->base, &del_func->base);
}

void ProcessCppDatabaseBeforeCodegen(GenerateOptions options, CppDatabase *db) {
    foreach (i, db->all_aggregates) {
        CppAggregate *aggr = ArrayGet(db->all_aggregates, i);

        if (ShouldExcludeEntity(options, &aggr->base)) {
            continue;
        }
        if (aggr->base.flags & CppEntityFlag_ForwardDecl) {
            continue;
        }

        bool acts_as_namespace = true;
        if (aggr->fields.count > 0) {
            acts_as_namespace = false;
        } else if (aggr->virtual_methods.count > 0) {
            acts_as_namespace = false;
        } else {
            foreach (j, aggr->functions) {
                CppFunction *func = ArrayGet(aggr->functions, j);
                if (func->flags & CppFunctionFlag_Method) {
                    acts_as_namespace = false;
                    break;
                }
            }
        }

        if (acts_as_namespace) {
            aggr->base.user_flags |= CppEntityUserFlag_AggregateAsNamespace;
        }

        bool opaque = ShouldBeOpaque(aggr);

        if (!acts_as_namespace) {
            foreach (j, aggr->functions) {
                CppFunction *func = ArrayGet(aggr->functions, j);

                // Generate New functions for each constructor
                if (func->flags & CppFunctionFlag_Constructor && opaque) {
                    GenerateOpaqueAggrNewFunction(options, db, aggr, func);
                }
            }

            if (opaque) {
                GenerateOpaqueAggrDeleteFunction(options, db, aggr);
            }
        }
    }

    foreach_key_value (kv, db->function_overloads) {
        Array *overloads = kv->value;

        if (overloads->count == 1) {
            CppFunction *func = ArrayGet(*overloads, 0);
            SetUniqueCName(&func->base, func->base.c_name);
            continue;
        }

        foreach (i, *overloads) {
            CppFunction *func = ArrayGet(*overloads, i);
            if (func->flags & CppFunctionFlag_Operator) {
                SetUniqueCName(&func->base, func->base.c_name);
                continue;
            }

            bool different_by_param_names = true;
            bool different_by_param_types = true;
            bool different_by_const = false;
            foreach (j, *overloads) {
                if (j == i) {
                    continue;
                }

                CppFunction *other = ArrayGet(*overloads, j);

                if (CppFunctionsHaveSameParams(func, other)) {
                    bool a_const = (func->flags & CppFunctionFlag_Const) != 0;
                    bool b_const = (other->flags & CppFunctionFlag_Const) != 0;
                    different_by_const = a_const != b_const;
                    different_by_param_types = false;
                    different_by_param_names = false;
                } else if (CppFunctionsHaveSameParamTypes(func, other)) {
                    different_by_param_types = false;
                } else if (CppFunctionsHaveSameParamNames(func, other)) {
                    different_by_param_names = false;
                }
            }

            if (!different_by_param_names && !different_by_param_types && !different_by_const) {
                printf("When trying to generate a nice name for overloaded function %s, we could not find a difference with the other overloads\n", func->base.fully_qualified_name);
                assert(false);
            }

            StringBuilder builder = {};
            SBAppendString(&builder, func->base.c_name);
            if (different_by_param_names && func->parameters.count > 0) {
                SBAppendString(&builder, "With");

                foreach (pi, func->parameters) {
                    CppVariable *param = ArrayGet(func->parameters, pi);
                    AppendOverloadParamName(&builder, param->base.c_name);
                }
            } else if (different_by_param_types && func->parameters.count > 0) {
                SBAppendString(&builder, "With");

                GenerateContext ctx = {};
                ctx.options = options;
                ctx.db      = db;
                ctx.builder = &builder;

                foreach (pi, func->parameters) {
                    CppVariable *param = ArrayGet(func->parameters, pi);
                    AppendAlphaNumericCType(&ctx, param->type);
                }
            } else if (different_by_const) {
                if (func->flags & CppFunctionFlag_Const) {
                    SBAppendString(&builder, "Const");
                }
            }

            SetUniqueCName(&func->base, SBBuild(&builder));
        }
    }
}

void AppendCppSourceCodeLocation(GenerateContext *ctx, CppSourceCodeLocation loc) {
    SBAppend(ctx->builder, "%s:%d:%d", loc.filename, (int)loc.line, (int)loc.character);
}

void AppendCType(GenerateContext *ctx, CppType *type, int indentation) {
    AppendCTypePrefix(ctx, type, indentation);
    AppendCTypePostfix(ctx, type, indentation);
}

void AppendCTypePrefix(GenerateContext *ctx, CppType *type, int indentation) {
    if (!type) {
        SBAppendString(ctx->builder, "<null>");
        return;
    }

    if (type->flags & CppTypeFlag_Const) {
        SBAppendString(ctx->builder, "const ");
    }

    type = UnwrapTemplate(ctx->options, ctx->db, type);

    switch (type->kind) {
        default: {
            SBAppendString(ctx->builder, CppTypeKind_Str[type->kind]);
        } break;
        case CppType_Invalid: {
            SBAppendString(ctx->builder, "<invalid>");
        } break;
        case CppType_Unknown: {
            SBAppend(ctx->builder, "< %s (size=%d, align=%d)>", clang_getCString(clang_getTypeKindSpelling(type->cx_type.kind)), (int)type->size, (int)type->alignment);
        } break;
        case CppType_SIMDVector: {
            SBAppendString(ctx->builder, clang_getCString(clang_getTypeSpelling(type->cx_type)));
        } break;
        case CppType_Reference: // Append references as pointers
        case CppType_RValueReference:
        case CppType_Pointer: {
            AppendCTypePrefix(ctx, type->type_pointer.pointee_type, indentation);

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
        case CppType_Array: {
            AppendCTypePrefix(ctx, type->type_array.element_type, indentation);
        } break;
        case CppType_Enum: {
            AppendCEnum(ctx, type->type_enum.e, indentation);
        } break;
        case CppType_Aggregate: {
            AppendCAggregate(ctx, type->type_aggregate.aggr, indentation);
        } break;
        case CppType_Named: {
            if (type->type_named.entity) {
                CppEntity *e = type->type_named.entity;
                if (e->kind == CppEntity_Typedef && ShouldUnwrap(ctx->options, e->fully_qualified_name)) {
                    CppTypedef *ty = (CppTypedef *)e;
                    AppendCTypePrefix(ctx, ty->type, indentation);
                    return;
                }

                SBAppendString(ctx->builder, type->type_named.entity->fully_qualified_c_name);
            } else if (type->type_named.name && type->type_named.name[0]) {
                SBAppendString(ctx->builder, type->type_named.name);
            } else {
                SBAppend(ctx->builder, "< ? named (size=%d, align=%d)>", (int)type->size, (int)type->alignment);
            }

            if (type->type_named.template_type_arguments.count > 0) {
                SBAppendString(ctx->builder, "<");

                foreach (i, type->type_named.template_type_arguments) {
                    if (i > 0) {
                        SBAppendString(ctx->builder, ", ");
                    }

                    CppType *arg_type = ArrayGet(type->type_named.template_type_arguments, i);
                    AppendCType(ctx, arg_type, indentation);
                }

                SBAppendString(ctx->builder, ">");
            }
        } break;
        case CppType_Function: {
            AppendCType(ctx, type->type_function.result_type, indentation);
        } break;
        case CppType_Auto: {
            SBAppendString(ctx->builder, "auto");
        } break;
    }
}

void AppendCTypePostfix(GenerateContext *ctx, CppType *type, int indentation) {
    switch (type->kind) {
        case CppType_Named: {
            if (type->type_named.entity) {
                CppEntity *e = type->type_named.entity;
                if (e->kind == CppEntity_Typedef && ShouldUnwrap(ctx->options, e->fully_qualified_name)) {
                    CppTypedef *ty = (CppTypedef *)e;
                    AppendCTypePostfix(ctx, ty->type, indentation);
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

                AppendCType(ctx, param, indentation);
            }
            SBAppendString(ctx->builder, ")");
        } break;
        case CppType_Pointer: {
            if (type->type_pointer.pointee_type->kind == CppType_Function) {
                SBAppendString(ctx->builder, ")");
            }
            AppendCTypePostfix(ctx, type->type_pointer.pointee_type, indentation);
        } break;
        case CppType_Array: {
            AppendCTypePostfix(ctx, type->type_array.element_type, indentation);
            if (type->type_array.num_elements >= 0) {
                SBAppend(ctx->builder, "[%u]", type->type_array.num_elements);
            } else {
                SBAppendString(ctx->builder, "[]");
            }
        } break;
    }
}

void AppendCVariable(GenerateContext *ctx, CppVariable *var, bool full_name, int indentation) {
    AppendCTypePrefix(ctx, var->type, indentation);

    const char *name = full_name ? var->base.fully_qualified_c_name : var->base.c_name;
    if (name && name[0]) {
        if (ShouldPrintSpaceAfterType(var->type)) {
            SBAppendString(ctx->builder, " ");
        }

        SBAppendString(ctx->builder, name);
    }

    AppendCTypePostfix(ctx, var->type, indentation);
}

void AppendCEnum(GenerateContext *ctx, CppEnum *e, int indentation) {
    SBAppendString(ctx->builder, "enum {\n");

    foreach (i, e->constants) {
        CppEnumConstant *value = ArrayGet(e->constants, i);

        SBAppendComment(ctx->builder, value->base.comment, indentation);

        SBAppendIndentation(ctx->builder, indentation + 1);
        SBAppendString(ctx->builder, value->base.fully_qualified_c_name);
        SBAppend(ctx->builder, " = %llu,\n", value->value);
    }

    SBAppendIndentation(ctx->builder, indentation);
    SBAppendString(ctx->builder, "}");
}

void AppendCEnumDecl(GenerateContext *ctx, CppEnum *e, int indentation) {
    SBAppendString(ctx->builder, "typedef ");
    AppendCTypePrefix(ctx, e->base_type, indentation);

    if (ShouldPrintSpaceAfterType(e->base_type)) {
        SBAppendString(ctx->builder, " ");
    }
    SBAppendString(ctx->builder, e->base.fully_qualified_c_name);

    AppendCTypePostfix(ctx, e->base_type, indentation);
    SBAppendString(ctx->builder, ";\n");

    SBAppendIndentation(ctx->builder, indentation);
    AppendCEnum(ctx, e, indentation);
    SBAppendString(ctx->builder, ";\n\n");
}

void AppendCAggregate(GenerateContext *ctx, CppAggregate *aggr, int indentation) {
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

    if (aggr->flags & CppAggregateFlag_HasVTableType && !(aggr->flags & CppAggregateFlag_SharedVTable)) {
        SBAppendIndentation(ctx->builder, indentation + 1);
        SBAppend(ctx->builder, "const %s_VTable *vtable;\n\n", aggr->base.fully_qualified_c_name);
    }

    foreach (i, aggr->base_classes) {
        CppBaseClass *base = ArrayGet(aggr->base_classes, i);
        assert(!base->is_virtual && "Virtual base classes are not supported");

        CppAggregate *base_aggr = GetBaseAggregate(aggr, i);
        if (i == 0 && aggr->flags & CppAggregateFlag_SharedVTable) {
            SBAppendIndentation(ctx->builder, indentation + 1);
            SBAppendString(ctx->builder, "union {\n");

            indentation += 1;
            SBAppendIndentation(ctx->builder, indentation + 1);
            SBAppend(ctx->builder, "const %s_VTable *vtable;\n", aggr->base.fully_qualified_c_name);
        }

        // Inherited base classes with no fields and no vtable has size 0 because of EBO, so don't include them
        if (base_aggr && base_aggr->fields.count == 0 && base_aggr->virtual_methods.count == 0) {
            SBAppendIndentation(ctx->builder, indentation + 1);
            SBAppendString(ctx->builder, "// ");
            AppendCType(ctx, base->type, indentation + 1);
            SBAppendString(ctx->builder, " base class has size 0, so it is not included\n");
        } else {
            SBAppendIndentation(ctx->builder, indentation + 1);
            AppendCTypePrefix(ctx, base->type, indentation + 1);

            if (ShouldPrintSpaceAfterType(base->type)) {
                SBAppendString(ctx->builder, " ");
            }

            if (aggr->base_classes.count == 1) {
                SBAppendString(ctx->builder, "base");
            } else if (base->type->kind == CppType_Named && base->type->type_named.entity != NULL) {
                SBAppend(ctx->builder, "base%s", base->type->type_named.entity->c_name);
            } else {
                SBAppend(ctx->builder, "base%d", i);
            }

            AppendCTypePostfix(ctx, base->type, indentation + 1);
            SBAppendString(ctx->builder, ";\n");

            if (i == 0 && aggr->flags & CppAggregateFlag_SharedVTable) {
                indentation -= 1;

                SBAppendIndentation(ctx->builder, indentation + 1);
                SBAppendString(ctx->builder, "};\n");
            }
        }
    }

    foreach (i, aggr->fields) {
        if (i == 0 && aggr->base_classes.count > 0) {
            SBAppendString(ctx->builder, "\n");
        }

        CppVariable *var = ArrayGet(aggr->fields, i);

        SBAppendComment(ctx->builder, var->base.comment, indentation + 1);

        SBAppendIndentation(ctx->builder, indentation + 1);
        AppendCVariable(ctx, var, false, indentation + 1);
        SBAppendString(ctx->builder, ";\n");
    }

    SBAppendIndentation(ctx->builder, indentation);
    SBAppendString(ctx->builder, "}");
}

void AppendCAggregateForwardDecl(GenerateContext *ctx, CppAggregate *aggr) {
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

const char *GetAggregateVTableType(CppAggregate *aggr) {
    if (aggr->virtual_methods.count == 0) {
        return "";
    }

    if (aggr->flags & CppAggregateFlag_HasVTableType) {
        return aggr->base.fully_qualified_c_name;
    }

    if (aggr->base_classes.count > 0) {
        CppAggregate *base_aggr = GetBaseAggregate(aggr, 0);
        return GetAggregateVTableType(base_aggr);
    }

    return "";
}

// One vtable per base class, Derive extends the vtable of the first base class
// if it has one
// Itanium C++ ABI (gcc and clang):
//  [0] offset-to-top (8 bytes)
//  [1] RTTI (8 bytes)
// Even if the destructor is not virtual, these slots are reserved:
//  [2] delete destructor (8 bytes) -> called when calling delete
//  [3] complete destructor (8 bytes) -> called when the object goes out of scope
// MSVC is simpler, it only has one destructor only if virtual, has no
// additionnal fields, and adds the new functions in order of appearence even
// for destructors
void AppendCAggregateVTableTypedef(GenerateContext *ctx, CppAggregate *aggr) {
    SBAppend(ctx->builder, "typedef struct %s_VTable {\n", aggr->base.fully_qualified_c_name);

    if (aggr->flags & CppAggregateFlag_SharedVTable) {
        CppAggregate *base_aggr = GetBaseAggregate(aggr, 0);

        SBAppendIndentation(ctx->builder, 1);
        SBAppend(ctx->builder, "%s_VTable base;\n", GetAggregateVTableType(base_aggr));
    } else {
        SBAppendIndentation(ctx->builder, 1);
        SBAppendString(ctx->builder, "JOLTC_VTABLE_HEADER\n");
    }

    if (aggr->flags & CppAggregateFlag_HasDestructorInVTable) {
        SBAppendIndentation(ctx->builder, 1);
        SBAppendString(ctx->builder, "JOLTC_VTABLE_DESTRUCTOR\n");
    }

    foreach (i, aggr->virtual_methods) {
        CppFunction *func = ArrayGet(aggr->virtual_methods, i);

        if (func->flags & CppFunctionFlag_Destructor) { // Already handled
            continue;
        }

        if (func->flags & CppFunctionFlag_Override) { // Not mine!
            continue;
        }

        SBAppendIndentation(ctx->builder, 1);
        AppendCFunctionSignature(ctx, func, 1, true);
        SBAppendString(ctx->builder, ";\n");
    }

    SBAppend(ctx->builder, "} %s_VTable;\n\n", aggr->base.fully_qualified_c_name);
}

void AppendCFunctionSignature(GenerateContext *ctx, CppFunction *func, int indentation, bool for_vtable) {
    AppendCType(ctx, func->result_type, indentation);
    if (ShouldPrintSpaceAfterType(func->result_type)) {
        SBAppendString(ctx->builder, " ");
    }

    if (for_vtable) {
        SBAppendString(ctx->builder, "(*");

        assert(func->base.unique_c_name != NULL);
        SBAppendString(ctx->builder, func->base.unique_c_name);

        SBAppendString(ctx->builder, ")");
    } else {
        assert(func->base.unique_fully_qualified_c_name != NULL);
        SBAppendString(ctx->builder, func->base.unique_fully_qualified_c_name);
    }

    SBAppendString(ctx->builder, "(");

    if (func->flags & CppFunctionFlag_Method) {
        CppAggregate *aggr = (CppAggregate *)func->base.parent;
        assert(aggr->base.kind == CppEntity_Aggregate);

        if (func->flags & CppFunctionFlag_Const) {
            SBAppendString(ctx->builder, "const ");
        }

        if (for_vtable) {
            SBAppendString(ctx->builder, "void");
        } else {
            SBAppendString(ctx->builder, aggr->base.fully_qualified_c_name);
        }

        SBAppendString(ctx->builder, " *self");

        if (func->parameters.count > 0) {
            SBAppendString(ctx->builder, ", ");
        }
    }

    foreach (i, func->parameters) {
        if (i > 0) {
            SBAppendString(ctx->builder, ", ");
        }

        CppVariable *param = ArrayGet(func->parameters, i);
        AppendCVariable(ctx, param, false, indentation + 1);
    }

    SBAppendString(ctx->builder, ")");
}

void GenerateCHeader(GenerateOptions options, StringBuilder *builder, CppDatabase *db) {
    GenerateContext ctx = {};
    ctx.options = options;
    ctx.db      = db;
    ctx.builder = builder;

    SBAppendString(builder, "// This file was autogenerated by parsing the C++ headers using libclang\n\n");

    SBAppendString(builder, "#ifndef JOLTC_H\n");
    SBAppendString(builder, "#define JOLTC_H\n\n");

    SBAppendString(builder, "#include <stddef.h>\n");
    SBAppendString(builder, "#include <stdint.h>\n");
    SBAppendString(builder, "#include <stdbool.h>\n");
    SBAppendString(builder, "\n");

    SBAppendString(builder, "#ifdef __cplusplus\nextern \"C\" {\n#endif\n\n");

    if (options.preamble) {
        SBAppendString(builder, options.preamble);
    }

    SBAppendString(builder, "\n// Forward declarations\n\n");

    foreach (i, db->all_aggregates) {
        CppAggregate *aggr = ArrayGet(db->all_aggregates, i);

        if (ShouldExcludeEntity(options, &aggr->base)) {
            continue;
        }

        if (aggr->base.user_flags & CppEntityUserFlag_AggregateAsNamespace) {
            continue;
        }

        if (aggr->base.flags & CppEntityFlag_ForwardDecl) {
            continue;
        }

        AppendCAggregateForwardDecl(&ctx, aggr);
    }

    SBAppendString(builder, "\n// Enums\n\n");

    foreach (i, db->all_enums) {
        CppEnum *e = ArrayGet(db->all_enums, i);

        if (ShouldExcludeEntity(options, &e->base)) {
            continue;
        }

        if (e->base.flags & CppEntityFlag_ForwardDecl) {
            continue;
        }

        SBAppendString(builder, "// ");
        AppendCppSourceCodeLocation(&ctx, GetStartLocation(e->base.source_code_range));
        SBAppendString(builder, "\n");

        SBAppendComment(ctx.builder, e->base.comment, 0);

        AppendCEnumDecl(&ctx, e, 0);
    }

    SBAppendString(builder, "\n");

    SBAppendString(builder, "\n// Classes and typedefs\n\n");

    int opaques = 0;
    foreach (i, db->all_aggregates) {
        if (ShouldBeOpaque(ArrayGet(db->all_aggregates, i))) {
            opaques += 1;
        }
    }

    printf("%d/%d opaque structs\n", opaques, (int)db->all_aggregates.count);

    foreach (i, db->all_entities) {
        CppEntity *entity = ArrayGet(db->all_entities, i);

        if (ShouldExcludeEntity(options, entity)) {
            continue;
        }

        switch (entity->kind) {
            case CppEntity_Aggregate: {
                CppAggregate *aggr = (CppAggregate *)entity;

                if (aggr->base.flags & CppEntityFlag_ForwardDecl) {
                    continue;
                }

                if (!(aggr->base.user_flags & CppEntityUserFlag_AggregateAsNamespace)) {
                    SBAppendString(builder, "// ");
                    AppendCppSourceCodeLocation(&ctx, GetStartLocation(entity->source_code_range));
                    SBAppendString(builder, "\n");

                    if (aggr->flags & CppAggregateFlag_HasVTableType) {
                        AppendCAggregateVTableTypedef(&ctx, aggr);
                    }

                    SBAppendComment(ctx.builder, aggr->base.comment, 0);

                    if (aggr->flags & CppAggregateFlag_Abstract) {
                        SBAppendString(builder, "// Abstract\n");
                    } else if (aggr->flags & CppAggregateFlag_HasVTableType) {
                        SBAppendString(builder, "// Has vtable\n");
                    }

                    if (ShouldBeOpaque(aggr)) {
                        SBAppendString(builder, "// Opaque\n");
                    }

                    SBAppendString(builder, "typedef ");
                    AppendCAggregate(&ctx, aggr, 0);
                    SBAppend(builder, " %s;\n\n", aggr->base.fully_qualified_c_name);
                }

                int num_functions = 0;
                foreach (j, aggr->functions) {
                    CppFunction *func = ArrayGet(aggr->functions, j);

                    if (ShouldExcludeEntity(ctx.options, &func->base)) {
                        continue;
                    }
                    if (func->flags & CppFunctionFlag_Operator) {
                        continue;
                    }

                    num_functions += 1;

                    SBAppendComment(ctx.builder, func->base.comment, 0);

                    AppendCFunctionSignature(&ctx, func, 0, false);
                    SBAppendString(ctx.builder, ";\n");
                }

                if (num_functions > 0) {
                    SBAppendString(ctx.builder, "\n");
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

                SBAppendComment(ctx.builder, ty->base.comment, 0);

                SBAppendString(builder, "typedef ");
                AppendCTypePrefix(&ctx, ty->type, 0);

                if (ShouldPrintSpaceAfterType(ty->type)) {
                    SBAppendString(builder, " ");
                }
                SBAppendString(builder, ty->base.fully_qualified_c_name);

                AppendCTypePostfix(&ctx, ty->type, 0);
                SBAppend(builder, ";\n\n");
            } break;
        }
    }

    if (!options.exclude_non_class_functions) {
        foreach (i, db->all_functions) {
            CppFunction *func = ArrayGet(db->all_functions, i);

            // Ignore methods
            if (func->base.parent && func->base.parent->kind != CppEntity_Namespace) {
                continue;
            }

            if (func->flags & CppFunctionFlag_Operator) {
                continue;
            }


            SBAppendString(builder, "// ");
            AppendCppSourceCodeLocation(&ctx, GetStartLocation(func->base.source_code_range));
            SBAppendString(builder, "\n");

            SBAppendComment(builder, func->base.comment, 0);

            AppendCFunctionSignature(&ctx, func, 0, false);
            SBAppendString(ctx.builder, ";\n");
        }

        SBAppendString(ctx.builder, "\n");
    }

    if (options.postamble) {
        SBAppendString(builder, options.postamble);
    }

    SBAppendString(builder, "#ifdef __cplusplus\n}\n#endif\n");

    SBAppendString(builder, "\n#endif // Include guard\n");
}

void GenerateCppSource(GenerateOptions options, StringBuilder *builder, CppDatabase *db) {
    GenerateContext ctx = {};
    ctx.options = options;
    ctx.db      = db;
    ctx.builder = builder;

    SBAppendString(builder, "// This file was autogenerated by parsing the C++ headers using libclang\n\n");

    SBAppendString(builder, "#include \"JoltC.h\"\n");
    SBAppendString(builder, "#include \"JoltHeaders.h\"\n");
    SBAppendString(builder, "\n");

    SBAppendString(builder, "// Type assertions\n\n");

    foreach (i, db->all_enums) {
        CppEnum *e = ArrayGet(db->all_enums, i);
        if (ShouldExcludeEntity(options, &e->base)) {
            continue;
        }

        SBAppend(builder, "static_assert(sizeof(%s) == sizeof(%s), \"Type size mismatch for %s\");\n", e->base.fully_qualified_name, e->base.fully_qualified_c_name, e->base.fully_qualified_c_name);
    }

    foreach (i, db->all_aggregates) {
        CppAggregate *aggr = ArrayGet(db->all_aggregates, i);
        if (ShouldExcludeEntity(options, &aggr->base)) {
            continue;
        }

        if (aggr->base.user_flags & CppEntityUserFlag_AggregateAsNamespace) {
            continue;
        }

        if (aggr->base.flags & CppEntityFlag_ForwardDecl) {
            continue;
        }

        SBAppend(builder, "static_assert(sizeof(%s) == sizeof(%s), \"Type mismatch for %s\");\n", aggr->base.fully_qualified_name, aggr->base.fully_qualified_c_name, aggr->base.fully_qualified_c_name);
    }

    SBAppendString(builder, "\n");
    return;

    SBAppendString(builder, "// Cpp conversion functions\n\n");

    foreach (i, db->all_enums) {
        CppEnum *e = ArrayGet(db->all_enums, i);
        if (ShouldExcludeEntity(options, &e->base)) {
            continue;
        }

        SBAppend(builder, "static inline %s ToCpp(%s val) { ", e->base.fully_qualified_name, e->base.fully_qualified_c_name);
        SBAppend(builder, "return static_cast<%s>(val);", e->base.fully_qualified_name);
        SBAppendString(builder, " }\n");
    }

    foreach (i, db->all_aggregates) {
        CppAggregate *aggr = ArrayGet(db->all_aggregates, i);

        if (aggr->base.user_flags & CppEntityUserFlag_AggregateAsNamespace) {
            continue;
        }

        if (aggr->base.flags & CppEntityFlag_ForwardDecl) {
            continue;
        }
        if (ShouldExcludeEntity(options, &aggr->base)) {
            continue;
        }

        SBAppend(builder, "static inline %s *ToCpp(%s *val) { ", aggr->base.fully_qualified_name, aggr->base.fully_qualified_c_name);
        SBAppend(builder, "return reinterpret_cast<%s *>(val);", aggr->base.fully_qualified_name);
        SBAppendString(builder, " }\n");

        SBAppend(builder, "static inline const %s *ToCpp(const %s *val) { ", aggr->base.fully_qualified_name, aggr->base.fully_qualified_c_name);
        SBAppend(builder, "return reinterpret_cast<const %s *>(val);", aggr->base.fully_qualified_name);
        SBAppendString(builder, " }\n");
    }

    SBAppendString(builder, " \n");

    foreach (i, db->all_functions) {
        CppFunction *func = ArrayGet(db->all_functions, i);
        if (ShouldExcludeEntity(options, &func->base)) {
            continue;
        }
        if (ShouldExcludeEntity(options, func->base.parent)) {
            continue;
        }

        if (options.exclude_non_class_functions && (!func->base.parent || func->base.parent->kind != CppEntity_Aggregate)) {
            continue;
        }

        CppAggregate *aggr = (CppAggregate *)func->base.parent;

        if (func->flags & CppFunctionFlag_Operator) {
            continue;
        }

        AppendCFunctionSignature(&ctx, func, 0, false);
        SBAppendString(builder, " {\n");

        SBAppendString(builder, "    ");

        if (func->result_type->kind != CppType_Void) {
            SBAppendString(builder, "return ");
        }

        if (func->base.user_flags & CppEntityUserFlag_DeleteFunction) {
            SBAppendString(builder, "delete self;\n}\n\n");
            continue;
        }

        if (func->base.user_flags & CppEntityUserFlag_NewFunction) {
            SBAppend(builder, "new %s", aggr->base.fully_qualified_name);
        } else if (func->flags & CppFunctionFlag_Constructor) {
            SBAppend(builder, "new(ToCpp(self)) %s", aggr->base.fully_qualified_name);
        } else if (func->flags & CppFunctionFlag_Method && !(func->flags & CppFunctionFlag_Constructor)) {
            SBAppend(builder, "ToCpp(self)->", aggr->base.fully_qualified_name);
            SBAppendString(builder, func->base.name);
        } else {
            SBAppendString(builder, func->base.fully_qualified_name);
        }

        SBAppendString(builder, "(");
        foreach (i, func->parameters) {
            CppVariable *var = ArrayGet(func->parameters, i);

            if (i > 0) {
                SBAppendString(builder, ", ");
            }

            if (var->type->kind == CppType_Reference) {
                SBAppendString(builder, "*");
            }
            SBAppendString(builder, var->base.name);
        }
        SBAppendString(builder, ");\n");

        SBAppendString(builder, "}\n\n");
    }
}
