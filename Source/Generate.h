#ifndef GENERATE_H
#define GENERATE_H

#include "Database.h"

void AppendCppTypePrefix(StringBuilder *builder, CppType *type, int indentation);
void AppendCppTypePostfix(StringBuilder *builder, CppType *type, int indentation);
void AppendCppType(StringBuilder *builder, CppType *type, int indentation);
void AppendCppAggregate(StringBuilder *builder, CppAggregate *aggr, int indentation);

void GenerateCode(StringBuilder *builder, CppDatabase *db);

#endif
