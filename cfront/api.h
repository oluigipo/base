#ifndef CFRONT_API_H
#define CFRONT_API_H

#include "common_basic.h"

// ===========================================================================
// ===========================================================================
// Enumerations

enum CF_TokenKind CONFIG_IF_ENHANCED_ENUMS(: uint16)
{
	CF_TokenKind_Null = 0,
	CF_TokenKind_MalformedToken,

	CF_TokenKind_Identifier,
	CF_TokenKind_Comment,
	CF_TokenKind_MultilineComment,
	CF_TokenKind_Linebreak,
	CF_TokenKind_EscapedLinebreak,

	CF_TokenKind_LitString,
	CF_TokenKind_LitWideString,
	CF_TokenKind_LitUtf8String,
	CF_TokenKind_LitChar,
	CF_TokenKind_LitWideChar,
	CF_TokenKind_LitUtf8Char,
	CF_TokenKind_LitNumber,

	CF_TokenKind__OneBeforeFirstSym,
	// 3-char symbols
	CF_TokenKind_SymTripleDot,
	CF_TokenKind_SymLeftShiftEq,
	CF_TokenKind_SymRightShiftEq,
	// 2-char symbols
	CF_TokenKind_SymDoubleHash,
	CF_TokenKind_SymLessEqual,
	CF_TokenKind_SymGreaterEqual,
	CF_TokenKind_SymDoubleEqual,
	CF_TokenKind_SymBangEqual,
	CF_TokenKind_SymLeftShift,
	CF_TokenKind_SymRightShift,
	CF_TokenKind_SymDoubleAmp,
	CF_TokenKind_SymDoublePipe,
	CF_TokenKind_SymDoublePlus,
	CF_TokenKind_SymDoubleMinus,
	CF_TokenKind_SymPlusEqual,
	CF_TokenKind_SymMinusEqual,
	CF_TokenKind_SymAstEqual,
	CF_TokenKind_SymSlashEqual,
	CF_TokenKind_SymAmpEqual,
	CF_TokenKind_SymPipeEqual,
	CF_TokenKind_SymHatEqual,
	CF_TokenKind_SymPercentEqual,
	CF_TokenKind_SymArrow,
	// 1-char symbols
	CF_TokenKind_SymHash,
	CF_TokenKind_SymLeftParen,
	CF_TokenKind_SymRightParen,
	CF_TokenKind_SymLeftBrkt,
	CF_TokenKind_SymRightBrkt,
	CF_TokenKind_SymLeftCurl,
	CF_TokenKind_SymRightCurl,
	CF_TokenKind_SymColon,
	CF_TokenKind_SymSemicolon,
	CF_TokenKind_SymComma,
	CF_TokenKind_SymDot,
	CF_TokenKind_SymLess,
	CF_TokenKind_SymGreater,
	CF_TokenKind_SymEqual,
	CF_TokenKind_SymAmp,
	CF_TokenKind_SymPipe,
	CF_TokenKind_SymPlus,
	CF_TokenKind_SymMinus,
	CF_TokenKind_SymAst,
	CF_TokenKind_SymSlash,
	CF_TokenKind_SymTilde,
	CF_TokenKind_SymHat,
	CF_TokenKind_SymPercent,
	CF_TokenKind_SymBang,
	CF_TokenKind_SymQuestionMark,
	CF_TokenKind__OnePastLastSym,

	CF_TokenKind__OneBeforeFirstKw,
	CF_TokenKind_KwAlignas,
	CF_TokenKind_KwAlignof,
	CF_TokenKind_KwAuto,
	CF_TokenKind_KwBool,
	CF_TokenKind_KwBreak,
	CF_TokenKind_KwCase,
	CF_TokenKind_KwChar,
	CF_TokenKind_KwConst,
	CF_TokenKind_KwConstexpr,
	CF_TokenKind_KwContinue,
	CF_TokenKind_KwDefault,
	CF_TokenKind_KwDo,
	CF_TokenKind_KwDouble,
	CF_TokenKind_KwElse,
	CF_TokenKind_KwEnum,
	CF_TokenKind_KwExtern,
	CF_TokenKind_KwFalse,
	CF_TokenKind_KwFloat,
	CF_TokenKind_KwFor,
	CF_TokenKind_KwGoto,
	CF_TokenKind_KwIf,
	CF_TokenKind_KwInline,
	CF_TokenKind_KwInt,
	CF_TokenKind_KwLong,
	CF_TokenKind_KwNullptr,
	CF_TokenKind_KwRegister,
	CF_TokenKind_KwRestrict,
	CF_TokenKind_KwReturn,
	CF_TokenKind_KwShort,
	CF_TokenKind_KwSigned,
	CF_TokenKind_KwSizeof,
	CF_TokenKind_KwStatic,
	CF_TokenKind_KwStaticAssert,
	CF_TokenKind_KwStruct,
	CF_TokenKind_KwSwitch,
	CF_TokenKind_KwThreadLocal,
	CF_TokenKind_KwTrue,
	CF_TokenKind_KwTypedef,
	CF_TokenKind_KwTypeof,
	CF_TokenKind_KwTypeofUnqual,
	CF_TokenKind_KwUnion,
	CF_TokenKind_KwUnsigned,
	CF_TokenKind_KwVoid,
	CF_TokenKind_KwVolatile,
	CF_TokenKind_KwWhile,
	CF_TokenKind_KwAtomic,
	// CF_TokenKind_KwBigInt,
	CF_TokenKind_KwComplex,
	CF_TokenKind_KwDecimal128,
	CF_TokenKind_KwDecimal32,
	CF_TokenKind_KwDecimal64,
	CF_TokenKind_KwGeneric,
	CF_TokenKind_KwImaginary,
	CF_TokenKind_KwNoreturn,

	// Microsoft specific
	CF_TokenKind_MsvcKwAlignof,
	CF_TokenKind_MsvcKwAsm,
	CF_TokenKind_MsvcKwAssume,
	CF_TokenKind_MsvcKwBased,
	CF_TokenKind_MsvcKwCdecl,
	CF_TokenKind_MsvcKwDeclspec,
	CF_TokenKind_MsvcKwEvent,
	CF_TokenKind_MsvcKwExcept,
	CF_TokenKind_MsvcKwFastcall,
	CF_TokenKind_MsvcKwFinally,
	CF_TokenKind_MsvcKwForceinline,
	CF_TokenKind_MsvcKwHook,
	CF_TokenKind_MsvcKwIfExists,
	CF_TokenKind_MsvcKwIfNotExists,
	CF_TokenKind_MsvcKwInline,
	CF_TokenKind_MsvcKwInt16,
	CF_TokenKind_MsvcKwInt32,
	CF_TokenKind_MsvcKwInt64,
	CF_TokenKind_MsvcKwInt8,
	CF_TokenKind_MsvcKwInterface,
	CF_TokenKind_MsvcKwLeave,
	CF_TokenKind_MsvcKwMultipleInheritance,
	CF_TokenKind_MsvcKwPtr32,
	CF_TokenKind_MsvcKwPtr64,
	CF_TokenKind_MsvcKwRaise,
	CF_TokenKind_MsvcKwRestrict,
	CF_TokenKind_MsvcKwSingleInheritance,
	CF_TokenKind_MsvcKwSptr,
	CF_TokenKind_MsvcKwStdcall,
	CF_TokenKind_MsvcKwSuper,
	CF_TokenKind_MsvcKwThiscall,
	CF_TokenKind_MsvcKwTypeof,
	CF_TokenKind_MsvcKwUnaligned,
	CF_TokenKind_MsvcKwUnhook,
	CF_TokenKind_MsvcKwUptr,
	CF_TokenKind_MsvcKwUuidof,
	CF_TokenKind_MsvcKwVectorcall,
	CF_TokenKind_MsvcKwVirtualInheritance,
	CF_TokenKind_MsvcKwW64,
	CF_TokenKind_MsvcKwWcharT,

	// GNU specific
	CF_TokenKind_GnuKwAlignof,
	CF_TokenKind_GnuKwAsm,
	CF_TokenKind_GnuKwAttribute,
	CF_TokenKind_GnuKwAutoType,
	CF_TokenKind_GnuKwBuiltinChooseExpr,
	CF_TokenKind_GnuKwBuiltinOffsetof,
	CF_TokenKind_GnuKwBuiltinTypesCompatibleP,
	CF_TokenKind_GnuKwBuiltinVaArg,
	CF_TokenKind_GnuKwComplex,
	CF_TokenKind_GnuKwConst,
	CF_TokenKind_GnuKwExtension,
	CF_TokenKind_GnuKwHasNothrowAssign,
	CF_TokenKind_GnuKwHasNothrowConstructor,
	CF_TokenKind_GnuKwHasNothrowCopy,
	CF_TokenKind_GnuKwHasTrivialAssign,
	CF_TokenKind_GnuKwHasTrivialConstructor,
	CF_TokenKind_GnuKwHasTrivialCopy,
	CF_TokenKind_GnuKwHasTrivialDestructor,
	CF_TokenKind_GnuKwHasVirtualDestructor,
	CF_TokenKind_GnuKwInt128,
	CF_TokenKind_GnuKwIsAbstract,
	CF_TokenKind_GnuKwIsBaseOf,
	CF_TokenKind_GnuKwIsClass,
	CF_TokenKind_GnuKwIsConvertibleTo,
	CF_TokenKind_GnuKwIsEmpty,
	CF_TokenKind_GnuKwIsEnum,
	CF_TokenKind_GnuKwIsPod,
	CF_TokenKind_GnuKwIsPolymorphic,
	CF_TokenKind_GnuKwIsStandardLayout,
	CF_TokenKind_GnuKwIsTrivial,
	CF_TokenKind_GnuKwIsUnion,
	CF_TokenKind_GnuKwIsLiteralType,
	CF_TokenKind_GnuKwUnderlyingType,
	CF_TokenKind_GnuKwImag,
	CF_TokenKind_GnuKwInline,
	CF_TokenKind_GnuKwLabel,
	CF_TokenKind_GnuKwNull,
	CF_TokenKind_GnuKwReal,
	CF_TokenKind_GnuKwRestrict,
	CF_TokenKind_GnuKwSigned,
	CF_TokenKind_GnuKwThread,
	CF_TokenKind_GnuKwTypeof,
	CF_TokenKind_GnuKwVolatile,

	// C++ specific
	CF_TokenKind_CxxKwCatch,
	CF_TokenKind_CxxKwChar8t,
	CF_TokenKind_CxxKwChar16t,
	CF_TokenKind_CxxKwChar32t,
	CF_TokenKind_CxxKwClass,
	CF_TokenKind_CxxKwConcept,
	CF_TokenKind_CxxKwConsteval,
	CF_TokenKind_CxxKwConstinit,
	CF_TokenKind_CxxKwConstCast,
	CF_TokenKind_CxxKwCoAwait,
	CF_TokenKind_CxxKwCoReturn,
	CF_TokenKind_CxxKwCoYield,
	CF_TokenKind_CxxKwDecltype,
	CF_TokenKind_CxxKwDelete,
	CF_TokenKind_CxxKwDynamicCast,
	CF_TokenKind_CxxKwExplicit,
	CF_TokenKind_CxxKwExport,
	CF_TokenKind_CxxKwFriend,
	CF_TokenKind_CxxKwMutable,
	CF_TokenKind_CxxKwNamespace,
	CF_TokenKind_CxxKwNew,
	CF_TokenKind_CxxKwNoexcept,
	CF_TokenKind_CxxKwOperator,
	CF_TokenKind_CxxKwPrivate,
	CF_TokenKind_CxxKwProtected,
	CF_TokenKind_CxxKwPublic,
	CF_TokenKind_CxxKwReinterpretCast,
	CF_TokenKind_CxxKwRequires,
	CF_TokenKind_CxxKwStaticCast,
	CF_TokenKind_CxxKwTemplate,
	CF_TokenKind_CxxKwThis,
	CF_TokenKind_CxxKwThrow,
	CF_TokenKind_CxxKwTry,
	CF_TokenKind_CxxKwTypeid,
	CF_TokenKind_CxxKwTypename,
	CF_TokenKind_CxxKwUsing,
	CF_TokenKind_CxxKwVirtual,
	CF_TokenKind_CxxKwWchart,
	// C++ context-sensetive keywords
	CF_TokenKind_CxxKwFinal,
	CF_TokenKind_CxxKwOverride,
	CF_TokenKind_CxxKwImport,
	CF_TokenKind_CxxKwModule,
	CF_TokenKind__OnePastLastKw,

	CF_TokenKind_Last,
};

enum CF_Standard CONFIG_IF_ENHANCED_ENUMS(: uint16)
{
	CF_Standard_Null = 0,
	CF_Standard_C89,
	CF_Standard_C99,
	CF_Standard_C11,
	CF_Standard_C17,
	CF_Standard_C23,
	CF_Standard_Cxx98,
	CF_Standard_Cxx03,
	CF_Standard_Cxx11,
	CF_Standard_Cxx14,
	CF_Standard_Cxx17,
	CF_Standard_Cxx20,
	CF_Standard_Cxx23,
};

enum
{
	CF_StandardFlag_C89 = (1 << CF_Standard_C89),
	CF_StandardFlag_C99 = (1 << CF_Standard_C99),
	CF_StandardFlag_C11 = (1 << CF_Standard_C11),
	CF_StandardFlag_C17 = (1 << CF_Standard_C17),
	CF_StandardFlag_C23 = (1 << CF_Standard_C23),
	CF_StandardFlag_AllC = (CF_StandardFlag_C89 | CF_StandardFlag_C99 | CF_StandardFlag_C11 | CF_StandardFlag_C17 | CF_StandardFlag_C23),
	CF_StandardFlag_PostC99 = (CF_StandardFlag_C99 | CF_StandardFlag_C11 | CF_StandardFlag_C17 | CF_StandardFlag_C23),
	CF_StandardFlag_PostC11 = (CF_StandardFlag_C11 | CF_StandardFlag_C17 | CF_StandardFlag_C23),
	CF_StandardFlag_PostC17 = (CF_StandardFlag_C17 | CF_StandardFlag_C23),
	CF_StandardFlag_PostC23 = (CF_StandardFlag_C23),

	CF_StandardFlag_Cxx98 = (1 << CF_Standard_Cxx98),
	CF_StandardFlag_Cxx03 = (1 << CF_Standard_Cxx03),
	CF_StandardFlag_Cxx11 = (1 << CF_Standard_Cxx11),
	CF_StandardFlag_Cxx14 = (1 << CF_Standard_Cxx14),
	CF_StandardFlag_Cxx17 = (1 << CF_Standard_Cxx17),
	CF_StandardFlag_Cxx20 = (1 << CF_Standard_Cxx20),
	CF_StandardFlag_Cxx23 = (1 << CF_Standard_Cxx23),
	CF_StandardFlag_AllCxx = (CF_StandardFlag_Cxx98|CF_StandardFlag_Cxx03|CF_StandardFlag_Cxx11|CF_StandardFlag_Cxx14|CF_StandardFlag_Cxx17|CF_StandardFlag_Cxx20|CF_StandardFlag_Cxx23),
	CF_StandardFlag_PostCxx03 = (CF_StandardFlag_Cxx03|CF_StandardFlag_Cxx11|CF_StandardFlag_Cxx14|CF_StandardFlag_Cxx17|CF_StandardFlag_Cxx20|CF_StandardFlag_Cxx23),
	CF_StandardFlag_PostCxx11 = (CF_StandardFlag_Cxx11|CF_StandardFlag_Cxx14|CF_StandardFlag_Cxx17|CF_StandardFlag_Cxx20|CF_StandardFlag_Cxx23),
	CF_StandardFlag_PostCxx14 = (CF_StandardFlag_Cxx14|CF_StandardFlag_Cxx17|CF_StandardFlag_Cxx20|CF_StandardFlag_Cxx23),
	CF_StandardFlag_PostCxx17 = (CF_StandardFlag_Cxx17|CF_StandardFlag_Cxx20|CF_StandardFlag_Cxx23),
	CF_StandardFlag_PostCxx20 = (CF_StandardFlag_Cxx20|CF_StandardFlag_Cxx23),
	CF_StandardFlag_PostCxx23 = (CF_StandardFlag_Cxx23),
};

#ifdef CONFIG_HAS_ENHANCED_ENUMS
enum CF_TokenKind typedef CF_TokenKind;
enum CF_Standard typedef CF_Standard;
#else
uint16 typedef CF_TokenKind;
uint16 typedef CF_Standard;
#endif

enum
{
	CF_LexingFlag_IncludeLinebreaks = 0x0001,
	CF_LexingFlag_IncludeComments = 0x0002,
	CF_LexingFlag_AllowMsvc = 0x0004,
	CF_LexingFlag_AllowGnu = 0x0008,
	CF_LexingFlag_IncludeEscapedLinebreaks = 0x0010,
	CF_LexingFlag_StripEscapedLinebreaksAndMerge = 0x0020,
	CF_LexingFlag_AllowContextSensetiveKeywords = 0x0040,
	CF_LexingFlag_KeepKeywordsAsIdentifiers = 0x0080,
	CF_LexingFlag_AddNullTerminatingToken = 0x0100,
	CF_LexingFlag_AllowCxx = 0x0200,

	CF_LexingFlag_PreprocessorFlags = (CF_LexingFlag_IncludeLinebreaks|CF_LexingFlag_IncludeEscapedLinebreaks|CF_LexingFlag_KeepKeywordsAsIdentifiers),
};

struct CF_SourceRange
{
	// NOTE(ljre): Measured in uint8s from the source string.
	intsize begin;
	intsize end;
}
typedef CF_SourceRange;

struct CF_Loc
{
	uint32 line;
	uint32 col;
}
typedef CF_Loc;

struct CF_LexError typedef CF_LexError;
struct CF_LexError
{
	CF_LexError* next;
	CF_SourceRange range;
	intsize token_index;
	CF_Loc loc;
	String what;
};

enum
{
	CF_PreprocessingFlag_PredefineStandardMacros = 0x0001,
};

struct CF_SourceTraceFile typedef CF_SourceTraceFile;
struct CF_SourceTraceFile
{
	CF_SourceTraceFile* includer;
	CF_Loc inclusion_loc;
	String path;
	String data;
};

struct CF_SourceTrace typedef CF_SourceTrace;
struct CF_SourceTrace
{
	CF_SourceTraceFile* file;
	CF_SourceTrace* macro_expansion;
	CF_SourceRange range;
	CF_Loc loc;
};

struct CF_MacroDefinition
{
	String name;
	CF_SourceRange range;
	CF_SourceTrace definition_trace; // `definition_trace.macro_expansion` is guaranteed to be NULL
	intsize expansion_tokens_begin;
	intsize expansion_tokens_end;
}
typedef CF_MacroDefinition;

struct CF_PreprocessError typedef CF_PreprocessError;
struct CF_PreprocessError
{
	CF_PreprocessError* next;
	CF_SourceTrace trace;
	String what;
};

// ===========================================================================
// ===========================================================================
// Lexing algorithms

struct CF_TokensFromStringResult
{
	intsize token_count;
	intz allocated_token_count;
	CF_TokenKind*   token_kinds;
	CF_SourceRange* source_ranges;
	CF_Loc*         locs;

	intsize lex_error_count;
	CF_LexError* lex_errors;
}
typedef CF_TokensFromStringResult;

struct CF_TokensFromStringArgs
{
	String str;
	uint32 lexing_flags;
	CF_Standard standard;

	SingleResizingAllocator kind_allocator;         // Resize, single allocation
	SingleResizingAllocator source_range_allocator; // Resize, single allocation
	SingleResizingAllocator loc_allocator;          // Resize, single allocation
	MultiAllocator          lex_error_allocator;    // Alloc
}
typedef CF_TokensFromStringArgs;

struct CF_TokensFromStringIncrementalArgs
{
	String str;
	CF_SourceRange changed_range;
	uint32 lexing_flags;
	CF_Standard standard;

	intz token_count;
	intz allocated_token_count;
	CF_TokenKind*   token_kinds;
	CF_SourceRange* source_ranges;
	CF_Loc*         locs;
	SingleResizingAllocator kind_allocator;         // Resize, single allocation
	SingleResizingAllocator source_range_allocator; // Resize, single allocation
	SingleResizingAllocator loc_allocator;          // Resize, single allocation
	MultiAllocator          lex_error_allocator;    // Alloc
}
typedef CF_TokensFromStringIncrementalArgs;

API AllocatorError CF_TokensFromString(CF_TokensFromStringResult* out_result, CF_TokensFromStringArgs const* args);
API AllocatorError CF_TokensFromStringIncremental(CF_TokensFromStringResult* out_result, CF_TokensFromStringIncrementalArgs const* args);
API CF_TokenKind CF_KeywordFromString(String str, CF_Standard standard, uint32 lexing_flags);

struct CF_PreprocessFileResult
{
	intsize token_count;
	CF_TokenKind*   token_kinds;
	CF_SourceTrace* source_traces;

	intsize pp_error_count;
	CF_PreprocessError* pp_errors;

	intsize macro_count;
	CF_MacroDefinition* macros;

	intsize strings_set_cap;
	intsize strings_set_size;
	String* set;
}
typedef CF_PreprocessFileResult;

struct CF_TokensFromFileProcResult
{
	String error; // should be StrNull if no error occured.
	String file_data;
	String path;
	String new_relative_path;
	CF_TokensFromStringResult tokens;
}
typedef CF_TokensFromFileProcResult;

struct CF_PreprocessFileArgs typedef CF_PreprocessFileArgs;
struct CF_PreprocessFileArgs
{
	String input_file_path;
	
	AllocatorError (*tokens_from_file_proc)(CF_TokensFromFileProcResult* out_result, CF_PreprocessFileArgs const* pp_args, String requested_path, String current_relative_path);
	void* user_data;

	String const* predefined_macros;
	intsize predefined_macro_count;
	String const* user_include_dirs;
	intsize user_include_dir_count;
	String const* system_include_dirs;
	intsize system_include_dir_count;

	CF_Standard standard;
	uint32 lexing_flags;
	uint32 preprocessing_flags;

	SingleResizingAllocator kind_allocator;         // Resize, single allocation
	SingleResizingAllocator source_trace_allocator; // Resize, single allocation
	MultiAllocator          trees_allocator;        // Alloc
	MultiAllocator          error_allocator;        // Alloc
	MultiResizingAllocator  dynamic_allocator;      // Alloc, Resize, Free
	ArenaAllocator          scratch_allocator;      // Alloc, Pop
};

API AllocatorError CF_PreprocessFile(CF_PreprocessFileResult* out_result, CF_PreprocessFileArgs const* args);

#endif
