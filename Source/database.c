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
    CXCursor canonical = clang_getCanonicalCursor(cursor);
    return HashMapFind(&db->cursor_to_entity, &canonical, NULL);
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
    e->cursor = clang_getCanonicalCursor(cursor);
    e->comment = clang_getCString(clang_Cursor_getRawCommentText(e->cursor));
    if (e->comment && !e->comment[0]) {
        e->comment = NULL;
    }

    e->visibility = GetCursorCppVisibility(cursor);

    if (!clang_isCursorDefinition(cursor)) {
        e->flags |= CppEntityFlag_ForwardDecl;
    }

    return e;
}

void PushCppEntity(CppDatabase *db, CppEntity *parent, CppEntity *entity) {
    if (!clang_Cursor_isNull(entity->cursor)) {
        HashMapInsert(&db->cursor_to_entity, &entity->cursor, entity);
    }

    entity->parent = parent;

    if (parent && parent->flags & CppEntityFlag_ParentIsTemplate) {
        entity->flags |= CppEntityFlag_ParentIsTemplate;
    }

    assert(entity->c_name != NULL);
    if (parent && parent->fully_qualified_name && parent->fully_qualified_name[0]) {
        if (entity->name[0]) {
            entity->fully_qualified_name = SPrintf("%s::%s", parent->fully_qualified_name, entity->name);
            entity->fully_qualified_c_name = SPrintf("%s_%s", parent->fully_qualified_c_name, entity->c_name);
        } else {
            entity->fully_qualified_name = parent->fully_qualified_name;
            entity->fully_qualified_c_name = parent->fully_qualified_c_name;
        }
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

                if (aggr->flags & CppAggregateFlag_Template) {
                    entity->flags |= CppEntityFlag_ParentIsTemplate;
                }

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

                        if (!(func->flags & CppFunctionFlag_Override)) {
                            aggr->flags |= CppAggregateFlag_HasVTableType;

                            if (func->flags & CppFunctionFlag_Destructor) {
                                aggr->flags |= CppAggregateFlag_HasDestructorInVTable;
                            }
                        }
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

CppEntity *GetEntityByFullName(CppDatabase *db, const char *name) {
    // @Todo @Speed: use a hash map
    foreach (i, db->all_entities) {
        CppEntity *e = ArrayGet(db->all_entities, i);
        if (StrEq(e->fully_qualified_name, name)) {
            return e;
        }
    }

    return NULL;
}

CppAggregate *GetBaseAggregate(CppAggregate *aggr, int index) {
    CppBaseClass *base = ArrayGet(aggr->base_classes, index);
    if (base->type->kind != CppType_Named || !base->type->type_named.entity) {
        return NULL;
    }

    CppAggregate *base_aggr = (CppAggregate *)base->type->type_named.entity;
    if (base_aggr->base.kind != CppEntity_Aggregate) {
        return NULL;
    }

    return base_aggr;
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
