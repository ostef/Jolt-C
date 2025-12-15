#ifndef PARSE_H
#define PARSE_H

#include "Core.h"
#include "Database.h"

typedef struct CppParseOptions {
    Array include_dirs;
    Array defines;
    Array files;
    Array extra_options;
    bool preparse_files_for_correct_include_order;
    bool strip_comments;
} CppParseOptions;

void ParseCppFiles(CppParseOptions options, CppDatabase *db);

#endif
