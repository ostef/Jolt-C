#include "Database.h"
#include "ClangUtils.h"

static
uint64_t HashCXCursor(void *a) {
    return clang_hashCursor(*(CXCursor *)a);
}

static
bool CompareCXCursors(void *a, void *b) {
    return clang_equalCursors(*(CXCursor *)a, *(CXCursor *)b);
}

void InitCppDatabase(CppDatabase *db) {
    memset(db, 0, sizeof(CppDatabase));

    db->cursor_to_entity.compare_func = CompareCXCursors;
    db->cursor_to_entity.hash_func = HashCXCursor;

    db->global_namespace = Alloc(CppNamespace);
    db->global_namespace->base.name = "";
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
    e->source_code_range = GetCppSourceCodeRange(cursor);
    e->cursor = cursor;
    e->visibility = GetCursorCppVisibility(cursor);

    return e;
}

void PushCppEntity(CppDatabase *db, CppEntity *parent, CppEntity *entity) {
    HashMapInsert(&db->cursor_to_entity, &entity->cursor, entity);

    entity->parent = parent;

    if (parent) {
        switch (parent->kind) {
            case CppEntity_Namespace: {
                CppNamespace *ns = (CppNamespace *)parent;
                ArrayPush(&ns->entities, entity);
            } break;
            case CppEntity_Aggregate: {
                CppAggregate *aggr = (CppAggregate *)parent;
                ArrayPush(&aggr->entities, entity);
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
            ArrayPush(&db->all_entities, entity);
        } break;
        case CppEntity_Aggregate: {
            ArrayPush(&db->all_aggregates, entity);
            ArrayPush(&db->all_entities, entity);
        } break;
        case CppEntity_Typedef: {
            ArrayPush(&db->all_typedefs, entity);
            ArrayPush(&db->all_entities, entity);
        } break;
        case CppEntity_Function: {
            ArrayPush(&db->all_functions, entity);
            ArrayPush(&db->all_entities, entity);
        } break;
    }

    if (parent && parent->fully_qualified_name && parent->fully_qualified_name[0]) {
        entity->fully_qualified_name = SPrintf("%s::%s", parent->fully_qualified_name, entity->name);
        entity->fully_qualified_c_name = SPrintf("%s_%s", parent->fully_qualified_c_name, entity->name);
    } else {
        entity->fully_qualified_name = entity->name;
        entity->fully_qualified_c_name = entity->name;
    }
}

CppNamespace *GetCppNamespace(CppDatabase *db, CppEntity *parent, const char *name) {
    foreach (i, db->all_namespaces) {
        CppNamespace *ns = ArrayGet(db->all_namespaces, i);
        if (ns->base.parent == parent && StrEq(ns->base.name, name)) {
            return ns;
        }
    }

    CppNamespace *ns = AllocCppEntity(Namespace, clang_getNullCursor());
    ns->base.name = name;
    PushCppEntity(db, parent, &ns->base);

    return ns;
}
