#ifndef PARSE_H
#define PARSE_H

#include "utils.h"

typedef struct ParseOptions {
    Array include_dirs;
    Array defines;
    Array files;
    Array extra_options;
    bool strip_comments;
} ParseOptions;

void ParseCppFiles(ParseOptions options);

#endif
