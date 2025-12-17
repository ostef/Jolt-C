#include "Database.h"
#include "ClangUtils.h"

static
uint64_t HashCXCursor(const void *a) {
    return clang_hashCursor(*(CXCursor *)a);
}

static
bool CompareCXCursors(const void *a, const void *b) {
    return clang_equalCursors(*(CXCursor *)a, *(CXCursor *)b);
}

void InitCppDatabase(CppDatabase *db) {
    memset(db, 0, sizeof(CppDatabase));

    db->cursor_to_entity.compare_func = CompareCXCursors;
    db->cursor_to_entity.hash_func = HashCXCursor;

    db->function_overloads.compare_func = StringCompareFunc;
    db->function_overloads.hash_func = StringHashFunc;

    db->global_namespace = Alloc(CppNamespace);
    db->global_namespace->base.name = "";
    db->global_namespace->base.c_name = "";
    db->global_namespace->base.kind = CppEntity_Namespace;
    db->global_namespace->base.cursor = clang_getNullCursor();

    PushCppEntity(db, NULL, &db->global_namespace->base);
}

CppEntity *GetCppEntityFromCursor(CppDatabase *db, CXCursor cursor){
    return HashMapFind(&db->cursor_to_entity, &cursor, NULL);
}

CppEntity *AllocCppEntityOfKind(CppEntityKind kind, int size, CXCursor cursor) {
    CppEntity *e = malloc(size);
    memset(e, 0, size);

    e->kind = kind;
    e->name = GetDeclName(cursor);
    e->c_name = strdup(e->name);

    for (int i = 0; e->c_name[i]; i += 1) {
        if (e->c_name[i] == ' ') {
            e->c_name[i] = '_';
        }
    }

    e->source_code_range = GetCppSourceCodeRange(cursor);
    e->cursor = cursor;
    e->visibility = GetCursorCppVisibility(cursor);

    if (!clang_isCursorDefinition(cursor)) {
        e->flags |= CppEntityFlag_ForwardDecl;
    }

    return e;
}

void PushCppEntity(CppDatabase *db, CppEntity *parent, CppEntity *entity) {
    HashMapInsert(&db->cursor_to_entity, &entity->cursor, entity);

    entity->parent = parent;

    assert(entity->c_name != NULL);
    if (parent && parent->fully_qualified_name && parent->fully_qualified_name[0]) {
        entity->fully_qualified_name = SPrintf("%s::%s", parent->fully_qualified_name, entity->name);
        entity->fully_qualified_c_name = SPrintf("%s_%s", parent->fully_qualified_c_name, entity->c_name);
    } else {
        entity->fully_qualified_name = entity->name;
        entity->fully_qualified_c_name = entity->c_name;
    }

    int64_t parent_aggregate_index = db->all_aggregates.count;
    int64_t parent_entity_index = db->all_entities.count;
    if (parent) {
        switch (parent->kind) {
            case CppEntity_Namespace: {
                CppNamespace *ns = (CppNamespace *)parent;
                ArrayPush(&ns->entities, entity);
            } break;
            case CppEntity_Aggregate: {
                CppAggregate *aggr = (CppAggregate *)parent;

                // We want to insert this entity before the parent for typedefs, enums and
                // aggregates, because the parent aggregate can have references to them
                parent_aggregate_index = db->all_aggregates.count - 1;
                while (parent_aggregate_index > 0) {
                    if (ArrayGet(db->all_aggregates, parent_aggregate_index) == aggr) {
                        break;
                    }

                    parent_aggregate_index -= 1;
                }

                parent_entity_index = db->all_entities.count - 1;
                while (parent_entity_index > 0) {
                    if (ArrayGet(db->all_entities, parent_entity_index) == aggr) {
                        break;
                    }

                    parent_entity_index -= 1;
                }

                ArrayPush(&aggr->entities, entity);

                if (entity->kind == CppEntity_Variable && !(entity->flags & CppEntityFlag_Static)) {
                    ArrayPush(&aggr->fields, entity);
                }

                if (entity->kind == CppEntity_Function) {
                    CppFunction *func = (CppFunction *)entity;
                    if (func->flags & CppFunctionFlag_Virtual) {
                        ArrayPush(&aggr->virtual_methods, entity);
                    } else {
                        ArrayPush(&aggr->functions, entity);
                    }
                }
            } break;
            case CppEntity_Enum: {
                assert(entity->kind == CppEntity_EnumConstant);
                CppEnum *e = (CppEnum *)parent;
                ArrayPush(&e->constants, entity);
            } break;
        }
    }

    switch (entity->kind) {
        case CppEntity_Namespace: {
            ArrayPush(&db->all_namespaces, entity);
            ArrayPush(&db->all_entities, entity);
        } break;
        case CppEntity_ValueDefine: {
            ArrayPush(&db->all_value_defines, entity);
            ArrayPush(&db->all_entities, entity);
        } break;
        case CppEntity_Enum: {
            ArrayPush(&db->all_enums, entity);
            ArrayOrderedInsert(&db->all_entities, entity, parent_entity_index);
        } break;
        case CppEntity_Aggregate: {
            ArrayOrderedInsert(&db->all_aggregates, entity, parent_aggregate_index);
            ArrayOrderedInsert(&db->all_entities, entity, parent_entity_index);
        } break;
        case CppEntity_Typedef: {
            ArrayPush(&db->all_typedefs, entity);
            ArrayOrderedInsert(&db->all_entities, entity, parent_entity_index);
        } break;
        case CppEntity_Function: {
            ArrayPush(&db->all_functions, entity);
            ArrayPush(&db->all_entities, entity);

            Array **overloads = (Array **)HashMapFindOrAdd(&db->function_overloads, entity->fully_qualified_name, NULL);
            if (!(*overloads)) {
                *overloads = Alloc(Array);
            } else {
                CppFunction *func = (CppFunction *)entity;
                func->flags |= CppFunctionFlag_Overloaded;

                if ((*overloads)->count == 1) {
                    CppFunction *first = (CppFunction *)ArrayGet(**overloads, 0);
                    first->flags |= CppFunctionFlag_Overloaded;
                }
            }

            ArrayPush(*overloads, entity);
        } break;
    }
}

CppNamespace *GetCppNamespace(CppDatabase *db, CppEntity *parent, char *name) {
    foreach (i, db->all_namespaces) {
        CppNamespace *ns = ArrayGet(db->all_namespaces, i);
        if (ns->base.parent == parent && StrEq(ns->base.name, name)) {
            return ns;
        }
    }

    CppNamespace *ns = AllocCppEntity(Namespace, clang_getNullCursor());
    ns->base.name = name;
    ns->base.c_name = strdup(name);
    PushCppEntity(db, parent, &ns->base);

    return ns;
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
            if (a->type_named.entity == b->type_named.entity) {
                return true;
            }

            return false;
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

void MakeUniqueOverloadedFunctionNames(CppDatabase *db) {
    foreach_key_value(kv, db->function_overloads) {
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

            if (func->parameters.count == 0) {
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
            if (different_by_param_names) {
                SBAppendString(&builder, "With");

                foreach (pi, func->parameters) {
                    CppVariable *param = ArrayGet(func->parameters, pi);
                    AppendOverloadParamName(&builder, param->base.c_name);
                }
            } else if (different_by_param_types) {
                printf("WARNING: overloaded function %s can only be differentiated by param types, which we do not yet support\n", func->base.fully_qualified_name);

                SBAppendString(&builder, "With");

                foreach (pi, func->parameters) {
                    CppVariable *param = ArrayGet(func->parameters, pi);
                    AppendOverloadParamName(&builder, param->base.c_name);
                }
            } else if (different_by_const) {
                SBAppendString(&builder, "Const");
            }

            SetUniqueCName(&func->base, SBBuild(&builder));
        }
    }
}
