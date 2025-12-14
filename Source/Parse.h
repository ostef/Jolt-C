#ifndef PARSE_H
#define PARSE_H

#include "Core.h"
#include "Database.h"

typedef struct CppParseOptions {
    Array include_dirs;
    Array defines;
    Array files;
    Array extra_options;
    bool strip_comments;
} CppParseOptions;

void ParseCppFiles(CppParseOptions options, CppDatabase *db);

#endif
