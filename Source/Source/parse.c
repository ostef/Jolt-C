#include <clang-c/Index.h>

#include "parse.h"

typedef struct Context {
    ParseOptions options;
    CXTranslationUnit tu;
    int indentation;
} Context;

static
enum CXChildVisitResult PrintVisitor(CXCursor cursor, CXCursor parent, CXClientData client_data) {
    Context *ctx = (Context *)client_data;

    CXSourceLocation loc = clang_getCursorLocation(cursor);
    bool in_system_header = clang_Location_isInSystemHeader(loc);
    if (in_system_header) {
        return CXChildVisit_Continue;
    }

    CXFile file;
    unsigned int line, column;
    clang_getSpellingLocation(loc, &file, &line, &column, NULL);
    const char *filename = clang_getCString(clang_getFileName(file));

    const char *name = clang_getCString(clang_getCursorSpelling(cursor));
    enum CXCursorKind kind = clang_getCursorKind(cursor);
    if (name && name[0]) {
        printf("%*s%s (%s) at %s:%u:%u\n", ctx->indentation * 2, "", clang_getCString(clang_getCursorKindSpelling(kind)), name, filename, line, column);
    } else {
        printf("%*s%s at %s:%u:%u\n", ctx->indentation * 2, "", clang_getCString(clang_getCursorKindSpelling(kind)), filename, line, column);
    }

    switch (kind) {

    }

    ctx->indentation += 1;
    clang_visitChildren(cursor, PrintVisitor, ctx);
    ctx->indentation -= 1;

    return CXChildVisit_Continue;
}

static
enum CXChildVisitResult VisitAggregateDecl(CXCursor cursor, CXCursor parent, CXClientData client_data) {
    Context *ctx = (Context *)client_data;

    enum CXCursorKind kind = clang_getCursorKind(cursor);
    printf("  %s is %s\n", clang_getCString(clang_getCursorSpelling(cursor)), clang_getCString(clang_getCursorKindSpelling(kind)));

    return CXChildVisit_Continue;
}

static
enum CXChildVisitResult TopLevelVisitor(CXCursor cursor, CXCursor parent, CXClientData client_data) {
    Context *ctx = (Context *)client_data;

    bool in_system_header = clang_Location_isInSystemHeader(clang_getCursorLocation(cursor));
    if (in_system_header) {
        return CXChildVisit_Continue;
    }

    enum CXCursorKind kind = clang_getCursorKind(cursor);
    switch (kind) {
        case CXCursor_Namespace:
        case CXCursor_InclusionDirective:
        case CXCursor_UnexposedDecl:
        case CXCursor_LinkageSpec: {
            return CXChildVisit_Recurse;
        } break;

        case CXCursor_ClassDecl:
        case CXCursor_UnionDecl:
        case CXCursor_StructDecl: {
            printf("%s is %s\n", clang_getCString(clang_getCursorSpelling(cursor)), clang_getCString(clang_getCursorKindSpelling(kind)));
            clang_visitChildren(cursor, VisitAggregateDecl, ctx);
        } break;

        case CXCursor_EnumDecl: {
            // clang_visitChildren(cursor, VisitEnumDecl, ctx);
        } break;

        case CXCursor_TypedefDecl: {
            // clang_visitChildren(cursor, VisitTypedefDecl, ctx);
        } break;
    }

    return CXChildVisit_Continue;
}

void ParseCppFiles(ParseOptions options) {
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

    Context ctx = {};
    ctx.tu = unit;
    ctx.options = options;
    clang_visitChildren(cursor, PrintVisitor, &ctx);

    clang_disposeTranslationUnit(unit);
    clang_disposeIndex(index);
}
