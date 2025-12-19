#ifndef GENERATE_H
#define GENERATE_H

#include "Database.h"

typedef struct GenerateOptions {
    Array declarations_to_exclude;
    Array typedefs_to_unwrap;
    const char *preamble;
    const char *postamble;
    bool exclude_non_class_functions;
} GenerateOptions;

typedef struct GenerateContext {
    GenerateOptions options;
    StringBuilder *builder;
} GenerateContext;

enum {
    CppEntityUserFlag_NewFunction          = 1 << 0,
    CppEntityUserFlag_DeleteFunction       = 1 << 1,
    CppEntityUserFlag_AggregateAsNamespace = 1 << 2,
};

void ProcessCppDatabaseBeforeCodegen(GenerateOptions options, CppDatabase *db);

void AppendAlphanumericCType(GenerateContext *ctx, CppType *type);
void AppendCTypePrefix(GenerateContext *ctx, CppType *type, int indentation);
void AppendCTypePostfix(GenerateContext *ctx, CppType *type, int indentation);
void AppendCType(GenerateContext *ctx, CppType *type, int indentation);
void AppendCEnum(GenerateContext *ctx, CppEnum *e, int indentation);
void AppendCEnumDecl(GenerateContext *ctx, CppEnum *e, int indentation);
void AppendCAggregate(GenerateContext *ctx, CppAggregate *aggr, int indentation);
void AppendCAggregateVTableTypedef(GenerateContext *ctx, CppAggregate *aggr);
void AppendCFunctionSignature(GenerateContext *ctx, CppFunction *func, int indentation, bool for_vtable);

void GenerateCHeader(GenerateOptions options, StringBuilder *builder, CppDatabase *db);
void GenerateCppSource(GenerateOptions options, StringBuilder *builder, CppDatabase *db);

#endif
