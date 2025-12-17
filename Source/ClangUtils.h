#ifndef CLANG_UTILS_H
#define CLANG_UTILS_H

#include <clang-c/Index.h>

#include "Database.h"

static inline
char *GetDeclName(CXCursor cursor) {
    if (clang_Cursor_isAnonymous(cursor)) {
        return "";
    }

    CXString spelling = clang_getCursorSpelling(cursor);

    return (char *)clang_getCString(spelling);
}

static inline
CppVisibility AccessSpecifierToCppVisibility(enum CX_CXXAccessSpecifier spec) {
    switch (spec) {
    case CX_CXXPublic: return CppVisibility_Public;
    case CX_CXXProtected: return CppVisibility_Protected;
    case CX_CXXPrivate: return CppVisibility_Private;
    }

    return CppVisibility_Public;
}

static inline
CppVisibility GetCursorCppVisibility(CXCursor cursor) {
    enum CX_CXXAccessSpecifier spec = clang_getCXXAccessSpecifier(cursor);

    return AccessSpecifierToCppVisibility(spec);
}

static inline
CppSourceCodeRange GetCppSourceCodeRange(CXCursor cursor) {
    CXSourceRange range = clang_getCursorExtent(cursor);
    CXSourceLocation range_start = clang_getRangeStart(range);
    CXSourceLocation range_end = clang_getRangeEnd(range);

    CXFile file;
    unsigned int start_line, start_character, start_offset;
    clang_getSpellingLocation(range_start, &file, &start_line, &start_character, &start_offset);

    unsigned int end_line, end_character, end_offset;
    clang_getSpellingLocation(range_end, NULL, &end_line, &end_character, &end_offset);

    const char *filename = clang_getCString(clang_getFileName(file));

    return (CppSourceCodeRange){
        .filename=filename,
        .start_line=start_line, .start_character=start_character, .start_offset=start_offset,
        .end_line=end_line, .end_character=end_character, .end_offset=end_offset,
    };
}
#endif
