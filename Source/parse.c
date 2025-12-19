#include <clang-c/Index.h>

#include "Parse.h"
#include "ClangUtils.h"

typedef struct CppParseContext {
    CppParseOptions options;
    CXTranslationUnit tu;

    CppDatabase *db;
    CppEntity *parent_entity;
} CppParseContext;

CppType *GetCppType(CppDatabase *db, CXType type);
CppEnum *ParseCppEnum(CppParseContext *ctx, CXCursor cursor);
CppAggregate *ParseCppAggregate(CppParseContext *ctx, CXCursor cursor);
CppFunction *ParseCppFunction(CppParseContext *ctx, CXCursor cursor);
CppVariable *ParseCppVariable(CppParseContext *ctx, CXCursor cursor);
CppTypedef *ParseCppTypeAliasDecl(CppParseContext *ctx, CXCursor cursor);

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
            CppBaseClass *base = Alloc(CppBaseClass);
            base->visibility = GetCursorCppVisibility(cursor);
            base->is_virtual = clang_isVirtualBase(cursor);
            base->type = GetCppType(ctx->db, clang_getCursorType(cursor));
            CppAggregate *aggr = (CppAggregate *)ctx->parent_entity;
            ArrayPush(&aggr->base_classes, base);

            CppAggregate *base_aggr = GetBaseAggregate(aggr, aggr->base_classes.count - 1);
            if (base_aggr && (base_aggr->flags & CppAggregateFlag_SharedVTable || base_aggr->flags & CppAggregateFlag_HasVTableType)) {
                aggr->flags |= CppAggregateFlag_SharedVTable;
            }
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
            ParseCppTypeAliasDecl(ctx, cursor);
        } break;

        case CXCursor_EnumDecl: {
            ParseCppEnum(ctx, cursor);
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
enum CXChildVisitResult EnumVisitor(CXCursor cursor, CXCursor parent, CXClientData client_data) {
    VISITOR_PREAMBLE();

    switch (kind) {
        case CXCursor_EnumConstantDecl: {
            CppEnumConstant *value = AllocCppEntity(EnumConstant, cursor);
            value->value = clang_getEnumConstantDeclUnsignedValue(cursor);
            PushCppEntity(ctx->db, ctx->parent_entity, &value->base);
        } break;
    }

    return CXChildVisit_Continue;
}

static
enum CXChildVisitResult FunctionVisitor(CXCursor cursor, CXCursor parent, CXClientData client_data) {
    VISITOR_PREAMBLE();

    CppFunction *func = (CppFunction *)ctx->parent_entity;

    // printf("%s(%d) %s\n", clang_getCString(clang_getCursorKindSpelling(kind)), kind, clang_getCString(clang_getCursorSpelling(cursor)));
    switch (kind) {
        case CXCursor_ParmDecl: {
            CppVariable *var = ParseCppVariable(ctx, cursor);
            ArrayPush(&func->parameters, var);
        } break;
    }

    return CXChildVisit_Continue;
}

static
enum CXChildVisitResult TopLevelVisitor(CXCursor cursor, CXCursor parent, CXClientData client_data) {
    VISITOR_PREAMBLE();

    // CppSourceCodeRange range = GetCppSourceCodeRange(cursor);
    // const char *name = clang_getCString(clang_getCursorSpelling(cursor));
    // printf("%s(%d) %s::%s at %s:%d:%d\n", clang_getCString(clang_getCursorKindSpelling(kind)), kind, GetDeclName(parent), name, range.filename, (int)range.start_line, (int)range.start_character);

    switch (kind) {
        case CXCursor_InclusionDirective: {
            return CXChildVisit_Recurse;
        } break;

        case CXCursor_ClassTemplate: {
            ParseCppAggregate(ctx, cursor);
        } break;

        case CXCursor_Namespace: {
            CppNamespace *ns = GetCppNamespace(ctx->db, ctx->parent_entity, GetDeclName(cursor));
            VisitRecurse(cursor, TopLevelVisitor, ctx, &ns->base);
        } break;

        case CXCursor_EnumDecl: {
            ParseCppEnum(ctx, cursor);
        } break;

        case CXCursor_StructDecl:
        case CXCursor_UnionDecl:
        case CXCursor_ClassDecl: {
            ParseCppAggregate(ctx, cursor);
        } break;

        case CXCursor_TypeAliasDecl: { // using A = B for types
            ParseCppTypeAliasDecl(ctx, cursor);
        } break;

        case CXCursor_FunctionDecl: {
            ParseCppFunction(ctx, cursor);
        } break;
    }

    return CXChildVisit_Continue;
}

CppType *GetCppType(CppDatabase *db, CXType type) {
    CppType *result = Alloc(CppType);
    result->cx_type = type;
    result->size = clang_Type_getSizeOf(type);
    result->alignment = clang_Type_getAlignOf(type);

    if (clang_isConstQualifiedType(type)) {
        result->flags |= CppTypeFlag_Const;
    }
    if (clang_isVolatileQualifiedType(type)) {
        result->flags |= CppTypeFlag_Volatile;
    }
    if (clang_isRestrictQualifiedType(type)) {
        result->flags |= CppTypeFlag_Restrict;
    }
    if (clang_isPODType(type)) {
        result->flags |= CppTypeFlag_IsPOD;
    }

    switch (type.kind) {
        default: {
            result->kind = CppType_Unknown;
        } break;
        case CXType_Unexposed: {
            result->kind = CppType_Unknown;
        } break;
        case CXType_Vector: {
            result->kind = CppType_SIMDVector;
        } break;
        case CXType_Invalid: {
            result->kind = CppType_Invalid;
        } break;
        case CXType_Void: {
            result->kind = CppType_Void;
        } break;
        case CXType_Bool: {
            result->kind = CppType_Bool;
        } break;
        case CXType_Char_U:
        case CXType_UChar:
        case CXType_UShort:
        case CXType_UInt:
        case CXType_ULong:
        case CXType_ULongLong:
        case CXType_UInt128: {
            switch (result->size) {
                case 1: {
                    result->kind = CppType_UInt8;
                } break;
                case 2: {
                    result->kind = CppType_UInt16;
                } break;
                case 4: {
                    result->kind = CppType_UInt32;
                } break;
                case 8: {
                    result->kind = CppType_UInt64;
                } break;
                case 16: {
                    result->kind = CppType_UInt128;
                } break;
                default: {
                    result->kind = CppType_Invalid;
                } break;
            }
        } break;
        case CXType_Char_S:
        case CXType_SChar:
        case CXType_Short:
        case CXType_Int:
        case CXType_Long:
        case CXType_LongLong:
        case CXType_Int128: {
            switch (result->size) {
                case 1: {
                    result->kind = CppType_Int8;
                } break;
                case 2: {
                    result->kind = CppType_Int16;
                } break;
                case 4: {
                    result->kind = CppType_Int32;
                } break;
                case 8: {
                    result->kind = CppType_Int64;
                } break;
                case 16: {
                    result->kind = CppType_Int128;
                } break;
                default: {
                    result->kind = CppType_Invalid;
                } break;
            }
        } break;
        case CXType_Float: {
            result->kind = CppType_Float;
        } break;
        case CXType_Double: {
            result->kind = CppType_Double;
        } break;
        case CXType_Pointer: {
            result->kind = CppType_Pointer;
            result->type_pointer.pointee_type = GetCppType(db, clang_getPointeeType(type));
        } break;
        case CXType_LValueReference: {
            result->kind = CppType_Reference;
            result->type_pointer.pointee_type = GetCppType(db, clang_getPointeeType(type));
        } break;
        case CXType_RValueReference: {
            result->kind = CppType_RValueReference;
            result->type_pointer.pointee_type = GetCppType(db, clang_getPointeeType(type));
        } break;
        case CXType_FunctionNoProto:
        case CXType_FunctionProto: {
            result->kind = CppType_Function;

            result->type_function.result_type = GetCppType(db, clang_getResultType(type));

            int num_parameters = clang_getNumArgTypes(type);
            ArrayReserve(&result->type_function.parameter_types, num_parameters);
            for (int i = 0; i < num_parameters; i += 1) {
                CppType *param_type = GetCppType(db, clang_getArgType(type, i));
                ArrayPush(&result->type_function.parameter_types, param_type);
            }

            result->type_function.is_variadic = clang_isFunctionTypeVariadic(type);
        } break;
        case CXType_IncompleteArray:
        case CXType_VariableArray:
        case CXType_DependentSizedArray:
        case CXType_ConstantArray: {
            result->kind = CppType_Array;
            result->type_array.element_type = GetCppType(db, clang_getElementType(type));
            result->type_array.num_elements = clang_getNumElements(type);
        } break;
        case CXType_Auto: {
            result->kind = CppType_Auto;
        } break;
        case CXType_Typedef: {
            result->kind = CppType_Named;
            result->type_named.name = clang_getCString(clang_getTypedefName(type));
            result->type_named.cursor = clang_getCanonicalCursor(clang_getTypeDeclaration(type));
            result->type_named.entity = GetCppEntityFromCursor(db, result->type_named.cursor);
        } break;
        case CXType_Elaborated:
            type = clang_Type_getNamedType(type);
            // fallthrough

        case CXType_Record:
        case CXType_Enum: {
            result->kind = CppType_Named;
            result->type_named.name = clang_getCString(clang_getTypeSpelling(type));
            result->type_named.cursor = clang_getCanonicalCursor(clang_getTypeDeclaration(type));
            result->type_named.entity = GetCppEntityFromCursor(db, result->type_named.cursor);

            // if (!result->type_named.entity) {
            //     printf("Could not get entity for %s (%s)\n", clang_getCString(clang_getCursorSpelling(result->type_named.cursor)), clang_getCString(clang_getCursorKindSpelling(clang_getCursorKind(result->type_named.cursor))));
            // }
        } break;
    }

    return result;
}

CppEnum *ParseCppEnum(CppParseContext *ctx, CXCursor cursor) {
    CppEnum *e = AllocCppEntity(Enum, cursor);
    CXType base_type = clang_getEnumDeclIntegerType(cursor);
    base_type = clang_getCanonicalType(base_type);
    e->base_type = GetCppType(ctx->db, base_type);

    if (clang_EnumDecl_isScoped(cursor)) {
        e->flags |= CppEnumFlag_Scoped;
    }

    if (clang_Cursor_isAnonymous(cursor)) {
        CppVariable *var = AllocCppEntity(Variable, cursor);
        var->type = Alloc(CppType);
        var->type->kind = CppType_Enum;
        var->type->type_enum.cursor = cursor;
        var->type->type_enum.e = e;

        PushCppEntity(ctx->db, ctx->parent_entity, &var->base);
    } else {
        PushCppEntity(ctx->db, ctx->parent_entity, &e->base);
    }

    VisitRecurse(cursor, EnumVisitor, ctx, &e->base);

    return e;
}

CppAggregate *ParseCppAggregate(CppParseContext *ctx, CXCursor cursor) {
    enum CXCursorKind kind = clang_getCursorKind(cursor);

    CppAggregate *aggr = AllocCppEntity(Aggregate, cursor);
    if (kind == CXCursor_StructDecl) {
        aggr->kind = CppAggregate_Struct;
    } else if (kind == CXCursor_UnionDecl) {
        aggr->kind = CppAggregate_Union;
    } else if (kind == CXCursor_ClassTemplate) {
        aggr->kind = CppAggregate_Class;
        aggr->flags |= CppAggregateFlag_Template;
    } else {
        aggr->kind = CppAggregate_Class;
    }

    if (clang_CXXRecord_isAbstract(cursor)) {
        aggr->flags |= CppAggregateFlag_Abstract;
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

    aggr->type = GetCppType(ctx->db, clang_getCursorType(cursor));

    VisitRecurse(cursor, AggregateVisitor, ctx, &aggr->base);

    if (!(aggr->flags & CppAggregateFlag_HasVTableType)) {
        aggr->flags &= ~CppAggregateFlag_SharedVTable;
    }

    return aggr;
}

CppFunction *ParseCppFunction(CppParseContext *ctx, CXCursor cursor) {
    enum CXCursorKind kind = clang_getCursorKind(cursor);

    CppFunction *func = AllocCppEntity(Function, cursor);
    if (StrStartsWith(func->base.name, "operator")) {
        func->flags |= CppFunctionFlag_Operator;
    }
    if (clang_CXXMethod_isVirtual(cursor)) {
        func->flags |= CppFunctionFlag_Virtual;

        CXCursor *overridden = NULL;
        unsigned int num_overridden = 0;
        clang_getOverriddenCursors(cursor, &overridden, &num_overridden);
        clang_disposeOverriddenCursors(overridden);

        if (num_overridden > 0) {
            func->flags |= CppFunctionFlag_Override;
        }
    }
    if (clang_CXXMethod_isPureVirtual(cursor)) {
        func->flags |= CppFunctionFlag_PureVirtual;
    }
    if (clang_CXXMethod_isStatic(cursor)) {
        func->base.flags |= CppEntityFlag_Static;
    }
    if (clang_CXXMethod_isConst(cursor)) {
        func->flags |= CppFunctionFlag_Const;
    }
    if (clang_Cursor_getStorageClass(cursor) == CX_SC_Static) {
        func->base.flags |= CppEntityFlag_Static;
    }
    if (kind == CXCursor_Constructor) {
        func->flags |= CppFunctionFlag_Constructor;
        func->flags |= CppFunctionFlag_Method;
        func->base.c_name = "Construct";
    }
    if (kind == CXCursor_Destructor) {
        func->flags |= CppFunctionFlag_Destructor;
        func->flags |= CppFunctionFlag_Method;
        func->base.c_name = "Destruct";
    }
    if (kind == CXCursor_CXXMethod && !(func->base.flags & CppEntityFlag_Static)) {
        func->flags |= CppFunctionFlag_Method;
    }

    PushCppEntity(ctx->db, ctx->parent_entity, &func->base);

    func->type = GetCppType(ctx->db, clang_getCursorType(cursor));
    assert(func->type->kind == CppType_Function);

    func->result_type = func->type->type_function.result_type;

    VisitRecurse(cursor, FunctionVisitor, ctx, &func->base);

    return func;
}

CppVariable *ParseCppVariable(CppParseContext *ctx, CXCursor cursor) {
    enum CXCursorKind kind = clang_getCursorKind(cursor);
    CppVariable *var = AllocCppEntity(Variable, cursor);
    if (clang_Cursor_getStorageClass(cursor) == CX_SC_Static) {
        var->base.flags |= CppEntityFlag_Static;
    }

    if (kind != CXCursor_ParmDecl) {
        PushCppEntity(ctx->db, ctx->parent_entity, &var->base);
    }

    var->type = GetCppType(ctx->db, clang_getCursorType(cursor));

    return var;
}

CppTypedef *ParseCppTypeAliasDecl(CppParseContext *ctx, CXCursor cursor) {
    CppTypedef *ty = AllocCppEntity(Typedef, cursor);
    PushCppEntity(ctx->db, ctx->parent_entity, &ty->base);

    CXType cx_type = clang_getCursorType(cursor);
    cx_type = clang_getCanonicalType(cx_type);
    ty->type = GetCppType(ctx->db, cx_type);

    return ty;
}

typedef struct CppParseIncludeContext {
    Array *inclusions;
    CXTranslationUnit unit;
} CppParseIncludeContext;

void InclusionVisitor(CXFile included_file, CXSourceLocation *inclusion_stack, unsigned include_len, CXClientData client_data) {
    CppParseIncludeContext *ctx = (CppParseIncludeContext *)client_data;

    if (included_file == NULL) {
        return;
    }

    if (!clang_isFileMultipleIncludeGuarded(ctx->unit, included_file)) {
        return;
    }

    if (clang_Location_isInSystemHeader(clang_getLocation(ctx->unit, included_file, 1, 1))) {
        return;
    }

    CXString path = clang_getFileName(included_file);
    const char *c_path = clang_getCString(path);

    if (!StrEndsWith(c_path, ".h") && !StrEndsWith(c_path, ".hpp")) {
        return;
    }

    bool found = false;
    foreach (i, *ctx->inclusions) {
        const char *header = ArrayGet(*ctx->inclusions, i);
        if (StrEq(header, c_path)) {
            found = true;
            break;
        }
    }

    if (!found) {
        printf("%s\n", c_path);
        ArrayPush(ctx->inclusions, (char *)c_path);
    }
}

Array PreParseCppFilesForIncludes(CppParseOptions options) {
    Array inclusions = {};

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

    CXIndex index = clang_createIndex(0, 0);

    foreach (i, options.files) {
        CXTranslationUnit unit = clang_parseTranslationUnit(index, ArrayGet(options.files, i), (const char *const *)args.data, args.count, NULL, 0, 0);
        if (!unit) {
            ErrorExit("Could not parse translation unit");
        }

        CppParseIncludeContext ctx = {};
        ctx.inclusions = &inclusions;
        ctx.unit = unit;

        clang_getInclusions(unit, InclusionVisitor, &ctx);
    }

    return inclusions;
}

void ParseCppFiles(CppParseOptions options, CppDatabase *db) {
    Array files;
    if (options.preparse_files_for_correct_include_order) {
        files = PreParseCppFilesForIncludes(options);
        foreach (i, files) {
            printf("%s\n", (const char *)ArrayGet(files, i));
        }

        StringBuilder builder = {};
        foreach (i, files) {
            const char *file = ArrayGet(files, i);
            foreach (i, options.include_dirs) {
                const char *include_dir = ArrayGet(options.include_dirs, i);
                if (StrStartsWith(file, include_dir)) {
                    file += strlen(include_dir);
                    if (file[0] == '/' || file[0] == '\\') {
                        file += 1;
                    }

                    break;
                }
            }

            SBAppend(&builder, "#include \"%s\"\n", file);
        }

        const char *str = SBBuild(&builder);
        WriteEntireFile("Source/JoltHeaders.h", str, strlen(str));
    } else {
        files = options.files;
    }

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
    foreach (i, files) {
        const char *file = ArrayGet(files, i);
        SBAppend(&builder, "#include \"%s\"\n", file);
    }

    temp_file.Contents = SBBuild(&builder);
    temp_file.Length = strlen(temp_file.Contents);

    CXTranslationUnit unit;
    enum CXErrorCode error_code = clang_parseTranslationUnit2(index, NULL, (const char *const *)args.data, args.count, &temp_file, 1, CXTranslationUnit_DetailedPreprocessingRecord, &unit);
    if (error_code != CXError_Success) {
        ErrorExit("Could not parse translation unit (error code %d)", error_code);
    }

    int num_diagnostics = clang_getNumDiagnostics(unit);
    if (num_diagnostics > 0) {
        printf("Clang diagnostics:\n");

        for (int i = 0; i < num_diagnostics; i += 1) {
            CXDiagnostic diag = clang_getDiagnostic(unit, i);
            enum CXDiagnosticSeverity severity = clang_getDiagnosticSeverity(diag);
            switch (severity) {
                case CXDiagnostic_Fatal:
                case CXDiagnostic_Error:
                case CXDiagnostic_Warning: {
                    CXString str = clang_formatDiagnostic(diag, clang_defaultDiagnosticDisplayOptions());
                    const char *cstr = clang_getCString(str);
                    printf("%s\n", cstr);
                } break;
            }
        }
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
