#ifndef GENERATE_H
#define GENERATE_H

#include "Database.h"

typedef struct GenerateOptions {
    Array declarations_to_exclude;
    Array typedefs_to_unwrap;
    const char *preamble;
    const char *postamble;
} GenerateOptions;

typedef struct GenerateContext {
    GenerateOptions options;
    StringBuilder *builder;
} GenerateContext;

void AppendCppTypePrefix(GenerateContext *ctx, CppType *type, int indentation);
void AppendCppTypePostfix(GenerateContext *ctx, CppType *type, int indentation);
void AppendCppType(GenerateContext *ctx, CppType *type, int indentation);
void AppendCppEnum(GenerateContext *ctx, CppEnum *e, int indentation);
void AppendCppEnumDecl(GenerateContext *ctx, CppEnum *e, int indentation);
void AppendCppAggregate(GenerateContext *ctx, CppAggregate *aggr, int indentation);

void GenerateCHeader(GenerateOptions options, StringBuilder *builder, CppDatabase *db);

#endif
