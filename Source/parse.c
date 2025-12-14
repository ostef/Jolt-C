#include <clang-c/Index.h>

#include "Parse.h"
#include "ClangUtils.h"

typedef struct CppParseContext {
    CppParseOptions options;
    CXTranslationUnit tu;

    CppDatabase *db;
    CppEntity *parent_entity;
} CppParseContext;

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
    enum CXCursorKind kind = clang_getCursorKind(cursor);

static
enum CXChildVisitResult AggregateVisitor(CXCursor cursor, CXCursor parent, CXClientData client_data) {
    VISITOR_PREAMBLE();

    CppSourceCodeRange range = GetCppSourceCodeRange(cursor);

    const char *name = clang_getCString(clang_getCursorSpelling(cursor));
    if (name && name[0]) {
        printf("%s %d %s (%s) at %s:%d:%d\n", GetDeclName(parent), kind, clang_getCString(clang_getCursorKindSpelling(kind)), name, range.filename, range.start_line, range.start_character);
    } else {
        printf("%s %d %s at %s:%d:%d\n", GetDeclName(parent), kind, clang_getCString(clang_getCursorKindSpelling(kind)), range.filename, range.start_line, range.start_character);
    }

    switch (kind) {
        case CXCursor_CXXBaseSpecifier: { // base class
        } break;
        case CXCursor_CXXAccessSpecifier: { // public, protected, private
            // Nothing to do, access specifier is stored on each cursor
        } break;
        case CXCursor_FieldDecl: {
            CppVariable *var = AllocCppEntity(Variable, cursor);
            PushCppEntity(ctx->db, ctx->parent_entity, &var->base);

            var->type = GetCppType(ctx->db, clang_getCursorType(cursor));
        } break;

        case CXCursor_Constructor:
        case CXCursor_Destructor:
        case CXCursor_CXXMethod: {
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

            PushCppEntity(ctx->db, ctx->parent_entity, &func->base);
        } break;

        case CXCursor_TypeAliasDecl: { // using A = B for types
        } break;
    }

    return CXChildVisit_Continue;
}

static
enum CXChildVisitResult TopLevelVisitor(CXCursor cursor, CXCursor parent, CXClientData client_data) {
    VISITOR_PREAMBLE();

    // const char *name = clang_getCString(clang_getCursorSpelling(cursor));
    // if (name && name[0]) {
    //     printf("%*s%s (%s) at %s:%u:%u\n", ctx->indentation * 2, "", clang_getCString(clang_getCursorKindSpelling(kind)), name, filename, line, column);
    // } else {
    //     printf("%*s%s at %s:%u:%u\n", ctx->indentation * 2, "", clang_getCString(clang_getCursorKindSpelling(kind)), filename, line, column);
    // }

    switch (kind) {
        case CXCursor_InclusionDirective: {
            return CXChildVisit_Recurse;
        } break;

        case CXCursor_Namespace: {
            CppNamespace *ns = AllocCppEntity(Namespace, cursor);
            PushCppEntity(ctx->db, ctx->parent_entity, &ns->base);

            VisitRecurse(cursor, TopLevelVisitor, ctx, &ns->base);
        } break;

        case CXCursor_StructDecl:
        case CXCursor_UnionDecl:
        case CXCursor_ClassDecl: {
            CppAggregate *aggr = AllocCppEntity(Aggregate, cursor);
            PushCppEntity(ctx->db, ctx->parent_entity, &aggr->base);

            VisitRecurse(cursor, AggregateVisitor, ctx, &aggr->base);
        } break;
    }

    // ctx->indentation += 1;
    // clang_visitChildren(cursor, TopLevelVisitor, ctx);
    // ctx->indentation -= 1;

    return CXChildVisit_Continue;
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
