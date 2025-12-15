#include <clang-c/Index.h>

#include "Parse.h"
#include "ClangUtils.h"

typedef struct CppParseContext {
    CppParseOptions options;
    CXTranslationUnit tu;

    CppDatabase *db;
    CppEntity *parent_entity;
} CppParseContext;

CppAggregate *ParseCppAggregate(CppParseContext *ctx, CXCursor cursor);
CppFunction *ParseCppFunction(CppParseContext *ctx, CXCursor cursor);
CppVariable *ParseCppVariable(CppParseContext *ctx, CXCursor cursor);

static inline
void VisitRecurse(CXCursor cursor, CXCursorVisitor visitor, CppParseContext *ctx, CppEntity *parent) {
    CppParseContext prev = *ctx;

    ctx->parent_entity = parent;
    clang_visitChildren(cursor, visitor, ctx);

    *ctx = prev;
}

#define VISITOR_PREAMBLE() \
    CppParseContext *ctx = (CppParseContext *)client_data; \
    bool in_system_header = clang_Location_isInSystemHeader(clang_getCursorLocation(cursor)); \
    if (in_system_header) { \
        return CXChildVisit_Continue; \
    } \
    enum CXCursorKind parent_kind = clang_getCursorKind(parent); \
    enum CXCursorKind kind = clang_getCursorKind(cursor);

static
enum CXChildVisitResult AggregateVisitor(CXCursor cursor, CXCursor parent, CXClientData client_data) {
    VISITOR_PREAMBLE();

    CppSourceCodeRange range = GetCppSourceCodeRange(cursor);

    // const char *name = clang_getCString(clang_getCursorSpelling(cursor));
    // if (name && name[0]) {
    //     printf("%s %d %s (%s) at %s:%d:%d\n", GetDeclName(parent), kind, clang_getCString(clang_getCursorKindSpelling(kind)), name, range.filename, (int)range.start_line, (int)range.start_character);
    // } else {
    //     printf("%s %d %s at %s:%d:%d\n", GetDeclName(parent), kind, clang_getCString(clang_getCursorKindSpelling(kind)), range.filename, (int)range.start_line, (int)range.start_character);
    // }

    switch (kind) {
        case CXCursor_CXXBaseSpecifier: { // base class
        } break;
        case CXCursor_CXXAccessSpecifier: { // public, protected, private
            // Nothing to do, access specifier is stored on each cursor
        } break;
        case CXCursor_VarDecl:
        case CXCursor_FieldDecl: {
            ParseCppVariable(ctx, cursor);
        } break;

        case CXCursor_Constructor:
        case CXCursor_Destructor:
        case CXCursor_FunctionDecl:
        case CXCursor_CXXMethod: {
            ParseCppFunction(ctx, cursor);
        } break;

        case CXCursor_TypeAliasDecl: { // using A = B for types
        } break;

        case CXCursor_StructDecl:
        case CXCursor_UnionDecl:
        case CXCursor_ClassDecl: {
            ParseCppAggregate(ctx, cursor);
        } break;
    }

    return CXChildVisit_Continue;
}

static
enum CXChildVisitResult TopLevelVisitor(CXCursor cursor, CXCursor parent, CXClientData client_data) {
    VISITOR_PREAMBLE();

    CppSourceCodeRange range = GetCppSourceCodeRange(cursor);

    const char *name = clang_getCString(clang_getCursorSpelling(cursor));
    printf("%s(%d) %s::%s at %s:%d:%d\n", clang_getCString(clang_getCursorKindSpelling(kind)), kind, GetDeclName(parent), name, range.filename, (int)range.start_line, (int)range.start_character);

    switch (kind) {
        case CXCursor_InclusionDirective: {
            return CXChildVisit_Recurse;
        } break;

        case CXCursor_Namespace: {
            CppNamespace *ns = GetCppNamespace(ctx->db, ctx->parent_entity, GetDeclName(cursor));
            VisitRecurse(cursor, TopLevelVisitor, ctx, &ns->base);
        } break;

        case CXCursor_StructDecl:
        case CXCursor_UnionDecl:
        case CXCursor_ClassDecl: {
            ParseCppAggregate(ctx, cursor);
        } break;

        case CXCursor_FunctionDecl: {
            ParseCppFunction(ctx, cursor);
        } break;
    }

    // ctx->indentation += 1;
    // clang_visitChildren(cursor, TopLevelVisitor, ctx);
    // ctx->indentation -= 1;

    return CXChildVisit_Continue;
}

CppAggregate *ParseCppAggregate(CppParseContext *ctx, CXCursor cursor) {
    if (!clang_isCursorDefinition(cursor)) {
        return NULL;
    }

    enum CXCursorKind kind = clang_getCursorKind(cursor);

    CppAggregate *aggr = AllocCppEntity(Aggregate, cursor);
    if (kind == CXCursor_StructDecl) {
        aggr->kind = CppAggregate_Struct;
    } else if (kind == CXCursor_UnionDecl) {
        aggr->kind = CppAggregate_Union;
    } else {
        aggr->kind = CppAggregate_Class;
    }

    if (clang_Cursor_isAnonymous(cursor)) {
        CppVariable *var = AllocCppEntity(Variable, cursor);
        var->type = Alloc(CppType);
        var->type->kind = CppType_Aggregate;
        var->type->type_aggregate.cursor = cursor;
        var->type->type_aggregate.aggr = aggr;

        PushCppEntity(ctx->db, ctx->parent_entity, &var->base);
    } else {
        PushCppEntity(ctx->db, ctx->parent_entity, &aggr->base);
    }

    VisitRecurse(cursor, AggregateVisitor, ctx, &aggr->base);

    return aggr;
}

CppFunction *ParseCppFunction(CppParseContext *ctx, CXCursor cursor) {
    enum CXCursorKind kind = clang_getCursorKind(cursor);

    CppFunction *func = AllocCppEntity(Function, cursor);
    if (kind == CXCursor_Constructor) {
        func->flags |= CppFunctionFlag_Constructor;
    }
    if (kind == CXCursor_Destructor) {
        func->flags |= CppFunctionFlag_Destructor;
    }
    if (kind == CXCursor_CXXMethod) {
        func->flags |= CppFunctionFlag_Method;
    }
    if (clang_Cursor_getStorageClass(cursor) == CX_SC_Static) {
        func->base.flags |= CppEntityFlag_Static;
    }

    PushCppEntity(ctx->db, ctx->parent_entity, &func->base);

    return func;
}

CppVariable *ParseCppVariable(CppParseContext *ctx, CXCursor cursor) {
    CppVariable *var = AllocCppEntity(Variable, cursor);
    if (clang_Cursor_getStorageClass(cursor) == CX_SC_Static) {
        var->base.flags |= CppEntityFlag_Static;
    }
    PushCppEntity(ctx->db, ctx->parent_entity, &var->base);

    var->type = GetCppType(ctx->db, clang_getCursorType(cursor));

    return var;
}

void ParseCppFiles(CppParseOptions options, CppDatabase *db) {
    CXIndex index = clang_createIndex(0, 0);

    Array args = {};
    foreach (i, options.include_dirs) {
        char *dir = ArrayGet(options.include_dirs, i);
        ArrayPush(&args, StrJoin("-I", dir));
    }
    foreach (i, options.defines) {
        char *def = ArrayGet(options.defines, i);
        ArrayPush(&args, StrJoin("-D", def));
    }
    foreach (i, options.extra_options) {
        char *opt = ArrayGet(options.extra_options, i);
        ArrayPush(&args, opt);
    }

    if (!options.strip_comments) {
        ArrayPush(&args, "-fparse-all-comments");
    }

    ArrayPush(&args, "temp.h");

    struct CXUnsavedFile temp_file = {};
    temp_file.Filename = "temp.h";

    StringBuilder builder = {};
    foreach (i, options.files) {
        const char *file = ArrayGet(options.files, i);
        SBAppend(&builder, "#include \"%s\"\n", file);
    }

    temp_file.Contents = SBBuild(&builder);
    temp_file.Length = strlen(temp_file.Contents);

    CXTranslationUnit unit = clang_parseTranslationUnit(index, NULL, (const char *const *)args.data, args.count, &temp_file, 1, CXTranslationUnit_DetailedPreprocessingRecord);
    if (!unit) {
        ErrorExit("Could not parse translation unit");
    }

    CXCursor cursor = clang_getTranslationUnitCursor(unit);

    CppParseContext ctx = {};
    ctx.tu = unit;
    ctx.options = options;
    ctx.db = db;
    ctx.parent_entity = &db->global_namespace->base;
    clang_visitChildren(cursor, TopLevelVisitor, &ctx);

    clang_disposeTranslationUnit(unit);
    clang_disposeIndex(index);
}
