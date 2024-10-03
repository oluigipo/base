#include "common.h"
#include "api.h"

static bool
IsWhitespace_(uint8 ch)
{
	bool r = false;
	r |= (ch == ' ');
	r |= (ch == '\t');
	r |= (ch == '\v');
	r |= (ch == '\r');
	return r;
}

static bool
IsNumber_(uint8 ch, int32 base)
{
	bool r = false;
	if (base >= 10)
		r |= (ch >= '0' && ch <= '9');
	else if (base == 8)
		r |= (ch >= '0' && ch <= '7');
	else if (base == 2)
		r |= (ch == '0' || ch == '1');
	if (base == 16)
		r |= (ch >= 'a' && ch <= 'f' || ch >= 'A' && ch <= 'F');
	return r;
}

static bool
IsIdentifierChar_(uint8 ch, bool is_first)
{
	bool r = false;
	r |= (ch >= 'a' && ch <= 'z');
	r |= (ch >= 'A' && ch <= 'Z');
	r |= (ch == '_');
	if (!is_first)
		r |= (ch >= '0' && ch <= '9');
	return r;
}

static CF_SourceRange
SubstringToRange_(String str, uint8 const* begin, uint8 const* end)
{
	return (CF_SourceRange) {
		.begin = begin - str.data,
		.end = end - str.data,
	};
}

// static bool
// IsCStandard_(CF_Standard std)
// {
// 	switch (std)
// 	{
// 		default: return false;
// 		case CF_Standard_C89:
// 		case CF_Standard_C99:
// 		case CF_Standard_C11:
// 		case CF_Standard_C17:
// 		case CF_Standard_C23: return true;
// 	}
// 	return false;
// }

// static bool
// IsCxxStandard_(CF_Standard std)
// {
// 	switch (std)
// 	{
// 		default: return false;
// 		case CF_Standard_Cxx98:
// 		case CF_Standard_Cxx03:
// 		case CF_Standard_Cxx11:
// 		case CF_Standard_Cxx14:
// 		case CF_Standard_Cxx17:
// 		case CF_Standard_Cxx20:
// 		case CF_Standard_Cxx23: return true;
// 	}
// 	return false;
// }

struct KeywordSpellingsEntry_
{
	String spelling;
	CF_TokenKind token_kind;
	uint16 lexing_flags;
	uint32 standard_flags;
}
typedef KeywordSpellingsEntry_;

static KeywordSpellingsEntry_ const g_keyword_spellings_table[] = {
	// C & baseline
	{ StrInit("alignas"), CF_TokenKind_KwAlignas, 0, CF_StandardFlag_PostC23|CF_StandardFlag_PostCxx11 },
	{ StrInit("alignof"), CF_TokenKind_KwAlignas, 0, CF_StandardFlag_PostC23|CF_StandardFlag_PostCxx11 },
	{ StrInit("auto"), CF_TokenKind_KwAuto, 0, CF_StandardFlag_AllC|CF_StandardFlag_AllCxx },
	{ StrInit("bool"), CF_TokenKind_KwBool, 0, CF_StandardFlag_PostC23|CF_StandardFlag_AllCxx },
	{ StrInit("break"), CF_TokenKind_KwBreak, 0, CF_StandardFlag_AllC|CF_StandardFlag_AllCxx },
	{ StrInit("case"), CF_TokenKind_KwCase, 0, CF_StandardFlag_AllC|CF_StandardFlag_AllCxx },
	{ StrInit("char"), CF_TokenKind_KwChar, 0, CF_StandardFlag_AllC|CF_StandardFlag_AllCxx },
	{ StrInit("const"), CF_TokenKind_KwConst, 0, CF_StandardFlag_AllC|CF_StandardFlag_AllCxx },
	{ StrInit("constexpr"), CF_TokenKind_KwConstexpr, 0, CF_StandardFlag_PostC23|CF_StandardFlag_PostCxx11 },
	{ StrInit("continue"), CF_TokenKind_KwContinue, 0, CF_StandardFlag_AllC|CF_StandardFlag_AllCxx },
	{ StrInit("default"), CF_TokenKind_KwDefault, 0, CF_StandardFlag_AllC|CF_StandardFlag_AllCxx },
	{ StrInit("do"), CF_TokenKind_KwDo, 0, CF_StandardFlag_AllC|CF_StandardFlag_AllCxx },
	{ StrInit("double"), CF_TokenKind_KwDouble, 0, CF_StandardFlag_AllC|CF_StandardFlag_AllCxx },
	{ StrInit("else"), CF_TokenKind_KwElse, 0, CF_StandardFlag_AllC|CF_StandardFlag_AllCxx },
	{ StrInit("enum"), CF_TokenKind_KwEnum, 0, CF_StandardFlag_AllC|CF_StandardFlag_AllCxx },
	{ StrInit("extern"), CF_TokenKind_KwExtern, 0, CF_StandardFlag_AllC|CF_StandardFlag_AllCxx },
	{ StrInit("false"), CF_TokenKind_KwFalse, 0, CF_StandardFlag_PostC23|CF_StandardFlag_AllCxx },
	{ StrInit("float"), CF_TokenKind_KwFloat, 0, CF_StandardFlag_AllC|CF_StandardFlag_AllCxx },
	{ StrInit("for"), CF_TokenKind_KwFor, 0, CF_StandardFlag_AllC|CF_StandardFlag_AllCxx },
	{ StrInit("goto"), CF_TokenKind_KwGoto, 0, CF_StandardFlag_AllC|CF_StandardFlag_AllCxx },
	{ StrInit("if"), CF_TokenKind_KwIf, 0, CF_StandardFlag_AllC|CF_StandardFlag_AllCxx },
	{ StrInit("inline"), CF_TokenKind_KwInline, 0, CF_StandardFlag_PostC99|CF_StandardFlag_AllCxx },
	{ StrInit("int"), CF_TokenKind_KwInt, 0, CF_StandardFlag_AllC|CF_StandardFlag_AllCxx },
	{ StrInit("long"), CF_TokenKind_KwLong, 0, CF_StandardFlag_AllC|CF_StandardFlag_AllCxx },
	{ StrInit("nullptr"), CF_TokenKind_KwNullptr, 0, CF_StandardFlag_PostC23|CF_StandardFlag_PostCxx11 },
	{ StrInit("register"), CF_TokenKind_KwRegister, 0, CF_StandardFlag_AllC|CF_StandardFlag_AllCxx },
	{ StrInit("restrict"), CF_TokenKind_KwRestrict, 0, CF_StandardFlag_PostC99 },
	{ StrInit("return"), CF_TokenKind_KwReturn, 0, CF_StandardFlag_AllC|CF_StandardFlag_AllCxx },
	{ StrInit("short"), CF_TokenKind_KwShort, 0, CF_StandardFlag_AllC|CF_StandardFlag_AllCxx },
	{ StrInit("signed"), CF_TokenKind_KwSigned, 0, CF_StandardFlag_AllC|CF_StandardFlag_AllCxx },
	{ StrInit("sizeof"), CF_TokenKind_KwSizeof, 0, CF_StandardFlag_AllC|CF_StandardFlag_AllCxx },
	{ StrInit("static"), CF_TokenKind_KwStatic, 0, CF_StandardFlag_AllC|CF_StandardFlag_AllCxx },
	{ StrInit("static_assert"), CF_TokenKind_KwStaticAssert, 0, CF_StandardFlag_PostC23|CF_StandardFlag_PostCxx11 },
	{ StrInit("struct"), CF_TokenKind_KwStruct, 0, CF_StandardFlag_AllC|CF_StandardFlag_AllCxx },
	{ StrInit("switch"), CF_TokenKind_KwSwitch, 0, CF_StandardFlag_AllC|CF_StandardFlag_AllCxx },
	{ StrInit("thread_local"), CF_TokenKind_KwThreadLocal, 0, CF_StandardFlag_PostC23|CF_StandardFlag_PostCxx11 },
	{ StrInit("true"), CF_TokenKind_KwTrue, 0, CF_StandardFlag_PostC23|CF_StandardFlag_AllCxx },
	{ StrInit("typedef"), CF_TokenKind_KwTypedef, 0, CF_StandardFlag_AllC|CF_StandardFlag_AllCxx },
	{ StrInit("typeof"), CF_TokenKind_KwTypeof, 0, CF_StandardFlag_PostC23 },
	{ StrInit("typeof_unqual"), CF_TokenKind_KwTypeofUnqual, 0, CF_StandardFlag_PostC23 },
	{ StrInit("union"), CF_TokenKind_KwUnion, 0, CF_StandardFlag_AllC|CF_StandardFlag_AllCxx },
	{ StrInit("unsigned"), CF_TokenKind_KwUnsigned, 0, CF_StandardFlag_AllC|CF_StandardFlag_AllCxx },
	{ StrInit("void"), CF_TokenKind_KwVoid, 0, CF_StandardFlag_AllC|CF_StandardFlag_AllCxx },
	{ StrInit("volatile"), CF_TokenKind_KwVolatile, 0, CF_StandardFlag_AllC|CF_StandardFlag_AllCxx },
	{ StrInit("while"), CF_TokenKind_KwWhile, 0, CF_StandardFlag_AllC|CF_StandardFlag_AllCxx },
	// C alternative spelling
	{ StrInit("_Alignas"), CF_TokenKind_KwAlignas, 0, CF_StandardFlag_PostC11 },
	{ StrInit("_Alignof"), CF_TokenKind_KwAlignof, 0, CF_StandardFlag_PostC11 },
	{ StrInit("_Atomic"), CF_TokenKind_KwAtomic, 0, CF_StandardFlag_PostC11 },
	{ StrInit("_Bool"), CF_TokenKind_KwBool, 0, CF_StandardFlag_PostC99 },
	{ StrInit("_Complex"), CF_TokenKind_KwComplex, 0, CF_StandardFlag_PostC99 },
	{ StrInit("_Decimal128"), CF_TokenKind_KwDecimal128, 0, CF_StandardFlag_PostC23 },
	{ StrInit("_Decimal32"), CF_TokenKind_KwDecimal32, 0, CF_StandardFlag_PostC23 },
	{ StrInit("_Decimal64"), CF_TokenKind_KwDecimal64, 0, CF_StandardFlag_PostC23 },
	{ StrInit("_Generic"), CF_TokenKind_KwGeneric, 0, CF_StandardFlag_PostC11 },
	{ StrInit("_Imaginary"), CF_TokenKind_KwImaginary, 0, CF_StandardFlag_PostC99 },
	{ StrInit("_Noreturn"), CF_TokenKind_KwNoreturn, 0, CF_StandardFlag_PostC11 },
	{ StrInit("_Static_assert"), CF_TokenKind_KwStaticAssert, 0, CF_StandardFlag_PostC11 },
	{ StrInit("_Thread_local"), CF_TokenKind_KwThreadLocal, 0, CF_StandardFlag_PostC11 },
	// C++
	{ StrInit("catch"), CF_TokenKind_CxxKwCatch, 0, CF_StandardFlag_AllCxx },
	{ StrInit("char8_t"), CF_TokenKind_CxxKwChar8t, 0, CF_StandardFlag_PostCxx20 },
	{ StrInit("char16_t"), CF_TokenKind_CxxKwChar16t, 0, CF_StandardFlag_PostCxx11 },
	{ StrInit("char32_t"), CF_TokenKind_CxxKwChar32t, 0, CF_StandardFlag_PostCxx11 },
	{ StrInit("class"), CF_TokenKind_CxxKwClass, 0, CF_StandardFlag_AllCxx },
	{ StrInit("concept"), CF_TokenKind_CxxKwConcept, 0, CF_StandardFlag_PostCxx20 },
	{ StrInit("consteval"), CF_TokenKind_CxxKwConsteval, 0, CF_StandardFlag_PostCxx20 },
	{ StrInit("constinit"), CF_TokenKind_CxxKwConstinit, 0, CF_StandardFlag_PostCxx20 },
	{ StrInit("const_cast"), CF_TokenKind_CxxKwConstCast, 0, CF_StandardFlag_AllCxx },
	{ StrInit("co_await"), CF_TokenKind_CxxKwCoAwait, 0, CF_StandardFlag_PostCxx20 },
	{ StrInit("co_return"), CF_TokenKind_CxxKwCoReturn, 0, CF_StandardFlag_PostCxx20 },
	{ StrInit("co_yield"), CF_TokenKind_CxxKwCoYield, 0, CF_StandardFlag_PostCxx20 },
	{ StrInit("decltype"), CF_TokenKind_CxxKwDecltype, 0, CF_StandardFlag_PostCxx11 },
	{ StrInit("delete"), CF_TokenKind_CxxKwDelete, 0, CF_StandardFlag_AllCxx },
	{ StrInit("dynamic_cast"), CF_TokenKind_CxxKwDynamicCast, 0, CF_StandardFlag_AllCxx },
	{ StrInit("explicit"), CF_TokenKind_CxxKwExplicit, 0, CF_StandardFlag_AllCxx },
	{ StrInit("export"), CF_TokenKind_CxxKwExport, 0, CF_StandardFlag_AllCxx },
	{ StrInit("friend"), CF_TokenKind_CxxKwFriend, 0, CF_StandardFlag_AllCxx },
	{ StrInit("mutable"), CF_TokenKind_CxxKwMutable, 0, CF_StandardFlag_AllCxx },
	{ StrInit("namespace"), CF_TokenKind_CxxKwNamespace, 0, CF_StandardFlag_AllCxx },
	{ StrInit("new"), CF_TokenKind_CxxKwNew, 0, CF_StandardFlag_AllCxx },
	{ StrInit("noexcept"), CF_TokenKind_CxxKwNoexcept, 0, CF_StandardFlag_PostCxx11 },
	{ StrInit("operator"), CF_TokenKind_CxxKwOperator, 0, CF_StandardFlag_AllCxx },
	{ StrInit("private"), CF_TokenKind_CxxKwPrivate, 0, CF_StandardFlag_AllCxx },
	{ StrInit("protected"), CF_TokenKind_CxxKwProtected, 0, CF_StandardFlag_AllCxx },
	{ StrInit("public"), CF_TokenKind_CxxKwPublic, 0, CF_StandardFlag_AllCxx },
	{ StrInit("reinterpret_cast"), CF_TokenKind_CxxKwReinterpretCast, 0, CF_StandardFlag_AllCxx },
	{ StrInit("requires"), CF_TokenKind_CxxKwRequires, 0, CF_StandardFlag_PostCxx20 },
	{ StrInit("static_cast"), CF_TokenKind_CxxKwStaticCast, 0, CF_StandardFlag_AllCxx },
	{ StrInit("template"), CF_TokenKind_CxxKwTemplate, 0, CF_StandardFlag_AllCxx },
	{ StrInit("this"), CF_TokenKind_CxxKwTemplate, 0, CF_StandardFlag_AllCxx },
	{ StrInit("throw"), CF_TokenKind_CxxKwThrow, 0, CF_StandardFlag_AllCxx },
	{ StrInit("typeid"), CF_TokenKind_CxxKwTypeid, 0, CF_StandardFlag_AllCxx },
	{ StrInit("typename"), CF_TokenKind_CxxKwTypename, 0, CF_StandardFlag_AllCxx },
	{ StrInit("using"), CF_TokenKind_CxxKwUsing, 0, CF_StandardFlag_AllCxx },
	{ StrInit("virtual"), CF_TokenKind_CxxKwVirtual, 0, CF_StandardFlag_AllCxx },
	{ StrInit("wchar_t"), CF_TokenKind_CxxKwWchart, 0, CF_StandardFlag_AllCxx },
	{ StrInit("final"), CF_TokenKind_CxxKwFinal, CF_LexingFlag_AllowContextSensetiveKeywords, CF_StandardFlag_AllCxx },
	{ StrInit("override"), CF_TokenKind_CxxKwOverride, CF_LexingFlag_AllowContextSensetiveKeywords, CF_StandardFlag_AllCxx },
	{ StrInit("import"), CF_TokenKind_CxxKwImport, CF_LexingFlag_AllowContextSensetiveKeywords, CF_StandardFlag_PostCxx20 },
	{ StrInit("module"), CF_TokenKind_CxxKwModule, CF_LexingFlag_AllowContextSensetiveKeywords, CF_StandardFlag_PostCxx20 },
	// MSVC
	{ StrInit("__alignof"), CF_TokenKind_MsvcKwAlignof, CF_LexingFlag_AllowMsvc, CF_StandardFlag_AllC|CF_StandardFlag_AllCxx },
	{ StrInit("__asm"), CF_TokenKind_MsvcKwAsm, CF_LexingFlag_AllowMsvc, CF_StandardFlag_AllC|CF_StandardFlag_AllCxx },
	{ StrInit("__assume"), CF_TokenKind_MsvcKwAssume, CF_LexingFlag_AllowMsvc, CF_StandardFlag_AllC|CF_StandardFlag_AllCxx },
	{ StrInit("__based"), CF_TokenKind_MsvcKwBased, CF_LexingFlag_AllowMsvc, CF_StandardFlag_AllC|CF_StandardFlag_AllCxx },
	{ StrInit("__cdecl"), CF_TokenKind_MsvcKwCdecl, CF_LexingFlag_AllowMsvc, CF_StandardFlag_AllC|CF_StandardFlag_AllCxx },
	{ StrInit("__declspec"), CF_TokenKind_MsvcKwDeclspec, CF_LexingFlag_AllowMsvc, CF_StandardFlag_AllC|CF_StandardFlag_AllCxx },
	{ StrInit("__event"), CF_TokenKind_MsvcKwEvent, CF_LexingFlag_AllowMsvc, CF_StandardFlag_AllC|CF_StandardFlag_AllCxx },
	{ StrInit("__except"), CF_TokenKind_MsvcKwExcept, CF_LexingFlag_AllowMsvc, CF_StandardFlag_AllC|CF_StandardFlag_AllCxx },
	{ StrInit("__fastcall"), CF_TokenKind_MsvcKwFastcall, CF_LexingFlag_AllowMsvc, CF_StandardFlag_AllC|CF_StandardFlag_AllCxx },
	{ StrInit("__finally"), CF_TokenKind_MsvcKwFinally, CF_LexingFlag_AllowMsvc, CF_StandardFlag_AllC|CF_StandardFlag_AllCxx },
	{ StrInit("__forceinline"), CF_TokenKind_MsvcKwForceinline, CF_LexingFlag_AllowMsvc, CF_StandardFlag_AllC|CF_StandardFlag_AllCxx },
	{ StrInit("__hook"), CF_TokenKind_MsvcKwHook, CF_LexingFlag_AllowMsvc, CF_StandardFlag_AllC|CF_StandardFlag_AllCxx },
	{ StrInit("__if_exists"), CF_TokenKind_MsvcKwIfExists, CF_LexingFlag_AllowMsvc, CF_StandardFlag_AllC|CF_StandardFlag_AllCxx },
	{ StrInit("__if_not_exists"), CF_TokenKind_MsvcKwIfNotExists, CF_LexingFlag_AllowMsvc, CF_StandardFlag_AllC|CF_StandardFlag_AllCxx },
	{ StrInit("__inline"), CF_TokenKind_MsvcKwInline, CF_LexingFlag_AllowMsvc, CF_StandardFlag_AllC|CF_StandardFlag_AllCxx },
	{ StrInit("__int16"), CF_TokenKind_MsvcKwInt16, CF_LexingFlag_AllowMsvc, CF_StandardFlag_AllC|CF_StandardFlag_AllCxx },
	{ StrInit("__int32"), CF_TokenKind_MsvcKwInt32, CF_LexingFlag_AllowMsvc, CF_StandardFlag_AllC|CF_StandardFlag_AllCxx },
	{ StrInit("__int64"), CF_TokenKind_MsvcKwInt64, CF_LexingFlag_AllowMsvc, CF_StandardFlag_AllC|CF_StandardFlag_AllCxx },
	{ StrInit("__int8"), CF_TokenKind_MsvcKwInt8, CF_LexingFlag_AllowMsvc, CF_StandardFlag_AllC|CF_StandardFlag_AllCxx },
	{ StrInit("__interface"), CF_TokenKind_MsvcKwInterface, CF_LexingFlag_AllowMsvc, CF_StandardFlag_AllC|CF_StandardFlag_AllCxx },
	{ StrInit("__leave"), CF_TokenKind_MsvcKwLeave, CF_LexingFlag_AllowMsvc, CF_StandardFlag_AllC|CF_StandardFlag_AllCxx },
	{ StrInit("__multiple_inheritance"), CF_TokenKind_MsvcKwMultipleInheritance, CF_LexingFlag_AllowMsvc, CF_StandardFlag_AllC|CF_StandardFlag_AllCxx },
	{ StrInit("__ptr32"), CF_TokenKind_MsvcKwPtr32, CF_LexingFlag_AllowMsvc, CF_StandardFlag_AllC|CF_StandardFlag_AllCxx },
	{ StrInit("__ptr64"), CF_TokenKind_MsvcKwPtr64, CF_LexingFlag_AllowMsvc, CF_StandardFlag_AllC|CF_StandardFlag_AllCxx },
	{ StrInit("__raise"), CF_TokenKind_MsvcKwRaise, CF_LexingFlag_AllowMsvc, CF_StandardFlag_AllC|CF_StandardFlag_AllCxx },
	{ StrInit("__restrict"), CF_TokenKind_MsvcKwRestrict, CF_LexingFlag_AllowMsvc, CF_StandardFlag_AllC|CF_StandardFlag_AllCxx },
	{ StrInit("__single_inheritance"), CF_TokenKind_MsvcKwSingleInheritance, CF_LexingFlag_AllowMsvc, CF_StandardFlag_AllC|CF_StandardFlag_AllCxx },
	{ StrInit("__sptr"), CF_TokenKind_MsvcKwSptr, CF_LexingFlag_AllowMsvc, CF_StandardFlag_AllC|CF_StandardFlag_AllCxx },
	{ StrInit("__stdcall"), CF_TokenKind_MsvcKwStdcall, CF_LexingFlag_AllowMsvc, CF_StandardFlag_AllC|CF_StandardFlag_AllCxx },
	{ StrInit("__super"), CF_TokenKind_MsvcKwSuper, CF_LexingFlag_AllowMsvc, CF_StandardFlag_AllC|CF_StandardFlag_AllCxx },
	{ StrInit("__thiscall"), CF_TokenKind_MsvcKwThiscall, CF_LexingFlag_AllowMsvc, CF_StandardFlag_AllC|CF_StandardFlag_AllCxx },
	{ StrInit("__typeof"), CF_TokenKind_MsvcKwTypeof, CF_LexingFlag_AllowMsvc, CF_StandardFlag_AllC|CF_StandardFlag_AllCxx },
	{ StrInit("__unaligned"), CF_TokenKind_MsvcKwUnaligned, CF_LexingFlag_AllowMsvc, CF_StandardFlag_AllC|CF_StandardFlag_AllCxx },
	{ StrInit("__unhook"), CF_TokenKind_MsvcKwUnhook, CF_LexingFlag_AllowMsvc, CF_StandardFlag_AllC|CF_StandardFlag_AllCxx },
	{ StrInit("__uptr"), CF_TokenKind_MsvcKwUptr, CF_LexingFlag_AllowMsvc, CF_StandardFlag_AllC|CF_StandardFlag_AllCxx },
	{ StrInit("__uuidof"), CF_TokenKind_MsvcKwUuidof, CF_LexingFlag_AllowMsvc, CF_StandardFlag_AllC|CF_StandardFlag_AllCxx },
	{ StrInit("__vectorcall"), CF_TokenKind_MsvcKwVectorcall, CF_LexingFlag_AllowMsvc, CF_StandardFlag_AllC|CF_StandardFlag_AllCxx },
	{ StrInit("__virtual_inheritance"), CF_TokenKind_MsvcKwVirtualInheritance, CF_LexingFlag_AllowMsvc, CF_StandardFlag_AllC|CF_StandardFlag_AllCxx },
	{ StrInit("__w64"), CF_TokenKind_MsvcKwW64, CF_LexingFlag_AllowMsvc, CF_StandardFlag_AllC|CF_StandardFlag_AllCxx },
	{ StrInit("__wchar_t"), CF_TokenKind_MsvcKwWcharT, CF_LexingFlag_AllowMsvc, CF_StandardFlag_AllC|CF_StandardFlag_AllCxx },
	// GNU
	{ StrInit("__alignof__"), CF_TokenKind_GnuKwAlignof, CF_LexingFlag_AllowGnu, CF_StandardFlag_AllC|CF_StandardFlag_AllCxx },
	{ StrInit("__asm__"), CF_TokenKind_GnuKwAlignof, CF_LexingFlag_AllowGnu, CF_StandardFlag_AllC|CF_StandardFlag_AllCxx },
	{ StrInit("__attribute__"), CF_TokenKind_GnuKwAlignof, CF_LexingFlag_AllowGnu, CF_StandardFlag_AllC|CF_StandardFlag_AllCxx },
	{ StrInit("__auto_type"), CF_TokenKind_GnuKwAutoType, CF_LexingFlag_AllowGnu, CF_StandardFlag_AllC|CF_StandardFlag_AllCxx },
	{ StrInit("__builtin_choose_expr"), CF_TokenKind_GnuKwBuiltinChooseExpr, CF_LexingFlag_AllowGnu, CF_StandardFlag_AllC|CF_StandardFlag_AllCxx },
	{ StrInit("__builtin_offsetof"), CF_TokenKind_GnuKwBuiltinOffsetof, CF_LexingFlag_AllowGnu, CF_StandardFlag_AllC|CF_StandardFlag_AllCxx },
	{ StrInit("__builtin_types_compatible_p"), CF_TokenKind_GnuKwBuiltinTypesCompatibleP, CF_LexingFlag_AllowGnu, CF_StandardFlag_AllC|CF_StandardFlag_AllCxx },
	{ StrInit("__builtin_va_arg"), CF_TokenKind_GnuKwBuiltinVaArg, CF_LexingFlag_AllowGnu, CF_StandardFlag_AllC|CF_StandardFlag_AllCxx },
	{ StrInit("__complex__"), CF_TokenKind_GnuKwComplex, CF_LexingFlag_AllowGnu, CF_StandardFlag_AllC|CF_StandardFlag_AllCxx },
	{ StrInit("__const__"), CF_TokenKind_GnuKwConst, CF_LexingFlag_AllowGnu, CF_StandardFlag_AllC|CF_StandardFlag_AllCxx },
	{ StrInit("__extension__"), CF_TokenKind_GnuKwExtension, CF_LexingFlag_AllowGnu, CF_StandardFlag_AllC|CF_StandardFlag_AllCxx },
	// TODO(ljre): rest of gnu stuff
};

struct TokenToAppend_
{
	CF_TokenKind kind;
	CF_Loc loc;
	CF_SourceRange range;
}
typedef TokenToAppend_;

struct LexingCtx_
{
	uint32 flags;
	AllocatorError err;

	intsize token_count;
	intsize token_allocated_count;
	CF_TokenKind*   token_kinds;
	CF_SourceRange* source_ranges;
	CF_Loc*         locs;

	intsize lex_error_count;
	CF_LexError* lex_errors;
	CF_LexError** lex_error_append_head;

	Allocator kind_allocator;
	Allocator source_range_allocator;
	Allocator loc_allocator;
	Allocator lex_error_allocator;
}
typedef LexingCtx_;

static void
PushToken_(LexingCtx_* ctx, TokenToAppend_ const* tok)
{
	Trace();
	if (ctx->token_count+1 > ctx->token_allocated_count)
	{
		intsize current_size = ctx->token_allocated_count;
		intsize amount_to_alloc = (ctx->token_allocated_count >> 1) + 1;
		intsize desired_new_count = amount_to_alloc + ctx->token_allocated_count;
		if (!ctx->err && ctx->kind_allocator.proc)
			AllocatorResizeOk(&ctx->kind_allocator, desired_new_count*SignedSizeof(CF_TokenKind), alignof(CF_TokenKind), &ctx->token_kinds, current_size*SignedSizeof(CF_TokenKind), &ctx->err);
		if (!ctx->err && ctx->source_range_allocator.proc)
			AllocatorResizeOk(&ctx->source_range_allocator, desired_new_count*SignedSizeof(CF_SourceRange), alignof(CF_SourceRange), &ctx->source_ranges, current_size*SignedSizeof(CF_SourceRange), &ctx->err);
		if (!ctx->err && ctx->loc_allocator.proc)
			AllocatorResizeOk(&ctx->loc_allocator, desired_new_count*SignedSizeof(CF_Loc), alignof(CF_Loc), &ctx->locs, current_size*SignedSizeof(CF_Loc), &ctx->err);
		if (ctx->err)
			return;
		ctx->token_allocated_count = desired_new_count;
	}

	intsize index = ctx->token_count++;
	if (ctx->token_kinds)
		ctx->token_kinds[index] = tok->kind;
	if (ctx->source_ranges)
		ctx->source_ranges[index] = tok->range;
	if (ctx->locs)
		ctx->locs[index] = tok->loc;
}

static void
PushError_(LexingCtx_* ctx, CF_LexError const* err)
{
	Trace();
	++ctx->lex_error_count;
	if (!ctx->err && ctx->lex_error_allocator.proc)
	{
		CF_LexError* new_err = AllocatorAlloc(&ctx->lex_error_allocator, sizeof(CF_LexError), alignof(CF_LexError), &ctx->err);
		if (new_err)
		{
			*ctx->lex_error_append_head = new_err;
			*new_err = *err;
			ctx->lex_error_append_head = &new_err->next;
		}
	}
}

API AllocatorError
CF_TokensFromString(CF_TokensFromStringResult* out_result, CF_TokensFromStringArgs const* args)
{
	Trace();
	LexingCtx_ ctx = {
		.flags = args->lexing_flags,
		.kind_allocator = args->kind_allocator,
		.source_range_allocator = args->source_range_allocator,
		.loc_allocator = args->loc_allocator,
		.lex_error_allocator = args->lex_error_allocator,
		.lex_error_append_head = &ctx.lex_errors,
	};

	uint8 const* head = args->str.data;
	uint8 const* end = args->str.data + args->str.size;

	// after consuming all leading whitespace when parsing a token, we'll advance `loc_head` up to
	// `head` and accumulate the line and col counters
	CF_Loc loc = { 1, 1 };
	uint8 const* loc_head = args->str.data;

	while (!ctx.err && head < end)
	{
		while (head < end && IsWhitespace_(head[0]))
			++head;
		for (; loc_head < head; ++loc_head)
		{
			if (loc_head[0] == '\n')
			{
				loc.line += 1;
				loc.col = 1;
			}
			else
				loc.col += 1;
		}
		if (head >= end)
			break;

		// Linebreak
		if (head[0] == '\n')
		{
			uint8 const* tok_begin = head;
			++head;
			uint8 const* tok_end = head;
			if (args->lexing_flags & CF_LexingFlag_IncludeLinebreaks)
			{
				PushToken_(&ctx, &(TokenToAppend_) {
					.kind = CF_TokenKind_Linebreak,
					.loc = loc,
					.range = SubstringToRange_(args->str, tok_begin, tok_end),
				});
			}
			continue;
		}

		// Escape
		if (head[0] == '\\')
		{
			if (head+1 < end && head[1] == '\n' || head+2 < end && head[1] == '\r' && head[2] == '\n')
			{
				uint8 const* tok_begin = head;
				if (head[1] == '\r')
					head += 3;
				else
					head += 2;
				uint8 const* tok_end = head;
				if (args->lexing_flags & CF_LexingFlag_IncludeEscapedLinebreaks)
				{
					PushToken_(&ctx, &(TokenToAppend_) {
						.kind = CF_TokenKind_EscapedLinebreak,
						.loc = loc,
						.range = SubstringToRange_(args->str, tok_begin, tok_end),
					});
				}
				continue;
			}
			goto lbl_malformed_token;
		}

		// Comments
		if (head+1 < end)
		{
			if (head[0] == '/' && head[1] == '/')
			{
				uint8 const* tok_begin = head;
				head += 2;
				while (head < end && head[0] != '\n')
				{
					if (head[0] == '\\')
						head += 2;
					else
						head += 1;
				}
				uint8 const* tok_end = head;
				if (args->lexing_flags & CF_LexingFlag_IncludeComments)
				{
					PushToken_(&ctx, &(TokenToAppend_) {
						.kind = CF_TokenKind_Comment,
						.loc = loc,
						.range = SubstringToRange_(args->str, tok_begin, tok_end),
					});
				}
				continue;
			}
			else if (head[0] == '/' && head[1] == '*')
			{
				uint8 const* tok_begin = head;
				head += 2;
				while (head < end)
				{
					if (head+1 < end && head[0] == '*' && head[1] == '/')
					{
						head += 2;
						break;
					}
					++head;
				}
				uint8 const* tok_end = head;
				if (args->lexing_flags & CF_LexingFlag_IncludeComments)
				{
					PushToken_(&ctx, &(TokenToAppend_) {
						.kind = CF_TokenKind_MultilineComment,
						.loc = loc,
						.range = SubstringToRange_(args->str, tok_begin, tok_end),
					});
				}
				continue;
			}
		}

		// Simple strings or chars
		if (head[0] == '"' || head[0] == '\'')
		{
			uint8 const* tok_begin = head;
			uint8 term = head[0];
			++head;
			while (head < end)
			{
				if (head[0] == '\\')
					head += 2;
				else if (head[0] == term)
				{
					head += 1;
					break;
				}
				else
					head += 1;
			}
			uint8 const* tok_end = head;
			PushToken_(&ctx, &(TokenToAppend_) {
				.kind = (term == '"') ? CF_TokenKind_LitString : CF_TokenKind_LitChar,
				.loc = loc,
				.range = SubstringToRange_(args->str, tok_begin, tok_end),
			});
			continue;
		}

		// Wide strings or chars
		if (head+1 < end && head[0] == 'L' && (head[1] == '"' || head[1] == '\''))
		{
			uint8 const* tok_begin = head;
			uint8 term = head[1];
			head += 2;
			while (head < end)
			{
				if (head[0] == '\\')
					head += 2;
				else if (head[0] == term)
				{
					head += 1;
					break;
				}
				else
					head += 1;
			}
			uint8 const* tok_end = head;
			PushToken_(&ctx, &(TokenToAppend_) {
				.kind = (term == '"') ? CF_TokenKind_LitWideString : CF_TokenKind_LitWideChar,
				.loc = loc,
				.range = SubstringToRange_(args->str, tok_begin, tok_end),
			});
			continue;
		}

		// UTF-8 strings or chars
		if (head+2 < end && head[0] == 'u' && head[1] == '8' && (head[2] == '"' || head[2] == '\''))
		{
			uint8 const* tok_begin = head;
			uint8 term = head[2];
			head += 3;
			while (head < end)
			{
				if (head[0] == '\\')
					head += 2;
				else if (head[0] == term)
				{
					head += 1;
					break;
				}
				else
					head += 1;
			}
			uint8 const* tok_end = head;
			PushToken_(&ctx, &(TokenToAppend_) {
				.kind = (term == '"') ? CF_TokenKind_LitUtf8String : CF_TokenKind_LitUtf8Char,
				.loc = loc,
				.range = SubstringToRange_(args->str, tok_begin, tok_end),
			});
			continue;
		}

		// Period or number
		if (head[0] == '.')
		{
			if (head+1 < end && IsNumber_(head[1], 10))
				goto lbl_parse_number;
			uint8 const* tok_begin = head;
			CF_TokenKind kind = CF_TokenKind_SymDot;
			if (head+2 < end && head[1] == '.' && head[2] == '.')
			{
				head += 3;
				kind = CF_TokenKind_SymTripleDot;
			}
			else
				head += 1;
			uint8 const* tok_end = head;
			PushToken_(&ctx, &(TokenToAppend_) {
				.kind = kind,
				.loc = loc,
				.range = SubstringToRange_(args->str, tok_begin, tok_end),
			});
			continue;
		}

		// Number
		if (IsNumber_(head[0], 10))
		{
			lbl_parse_number:;
			uint8 const* tok_begin = head;

			int32 base = 10;
			CF_TokenKind kind = CF_TokenKind_LitNumber;
			if (head+1 < end && head[0] == '0')
			{
				if (head[1] == 'x')
					base = 16;
				else if (head[1] == 'b')
					base = 2;
				else if (head[1] == 'o')
					base = 8;

				if (base != 10)
					head += 2;
				else
					base = 8; // when 0 is the prefix, it's at least octal
			}
			while (head < end && IsNumber_(head[0], base))
				++head;
			if (head < end && head[0] == '.')
			{
				++head;
				if (base != 10 && base != 16)
					kind = CF_TokenKind_MalformedToken;
				while (head < end && IsNumber_(head[0], base))
					++head;
				if (head < end && (head[0] == 'e' || head[0] == 'E' || head[0] == 'p' || head[0] == 'P'))
				{
					++head;
					if (head < end && (head[0] == '+' || head[0] == '-'))
						++head;
					while (head < end && IsNumber_(head[0], 10))
						++head;
				}
			}
			while (head < end)
			{
				if (head[0] == 'l' || head[0] == 'L')
					++head;
				else if (head[0] == 'u' || head[0] == 'U')
					++head;
				else if (head[0] == 'f' || head[0] == 'F')
					++head;
				else
					break;
			}

			uint8 const* tok_end = head;

			PushToken_(&ctx, &(TokenToAppend_) {
				.kind = kind,
				.loc = loc,
				.range = SubstringToRange_(args->str, tok_begin, tok_end),
			});
			continue;
		}

		// Identifier or keyword
		if (IsIdentifierChar_(head[0], true))
		{
			uint8 const* tok_begin = head;
			do
				++head;
			while (head < end && IsIdentifierChar_(head[0], false));
			uint8 const* tok_end = head;

			CF_TokenKind kind = CF_TokenKind_Identifier;
			if (!(ctx.flags & CF_LexingFlag_KeepKeywordsAsIdentifiers))
			{
				String as_string = StrRange(tok_begin, tok_end);
				CF_TokenKind new_kind = CF_KeywordFromString(as_string, args->standard, ctx.flags);
				if (new_kind)
					kind = new_kind;
			}
			PushToken_(&ctx, &(TokenToAppend_) {
				.kind = kind,
				.loc = loc,
				.range = SubstringToRange_(args->str, tok_begin, tok_end),
			});
			continue;
		}

		// 3-char symbols
		if (head+2 < end)
		{
			CF_TokenKind kind = 0;

			if (head[0] == '.' && head[1] == '.' && head[2] == '.')
				kind = CF_TokenKind_SymTripleDot;
			else if (head[0] == '<' && head[1] == '<' && head[2] == '=')
				kind = CF_TokenKind_SymLeftShiftEq;
			else if (head[0] == '>' && head[1] == '>' && head[2] == '=')
				kind = CF_TokenKind_SymRightShiftEq;

			if (kind)
			{
				PushToken_(&ctx, &(TokenToAppend_) {
					.kind = kind,
					.loc = loc,
					.range = SubstringToRange_(args->str, head, head+3),
				});
				head += 3;
				continue;
			}
		}

		// 2-char symbols
		if (head+1 < end)
		{
			CF_TokenKind kind = 0;

			if (head[0] == '#' && head[1] == '#')
				kind = CF_TokenKind_SymDoubleHash;
			else if (head[0] == '=' && head[1] == '=')
				kind = CF_TokenKind_SymDoubleEqual;
			else if (head[0] == '!' && head[1] == '=')
				kind = CF_TokenKind_SymBangEqual;
			else if (head[0] == '<' && head[1] == '=')
				kind = CF_TokenKind_SymLessEqual;
			else if (head[0] == '>' && head[1] == '=')
				kind = CF_TokenKind_SymGreaterEqual;
			else if (head[0] == '<' && head[1] == '<')
				kind = CF_TokenKind_SymLeftShift;
			else if (head[0] == '=' && head[1] == '=')
				kind = CF_TokenKind_SymDoubleEqual;
			else if (head[0] == '&' && head[1] == '&')
				kind = CF_TokenKind_SymDoubleAmp;
			else if (head[0] == '|' && head[1] == '|')
				kind = CF_TokenKind_SymDoublePipe;
			else if (head[0] == '+' && head[1] == '+')
				kind = CF_TokenKind_SymDoublePlus;
			else if (head[0] == '-' && head[1] == '-')
				kind = CF_TokenKind_SymDoubleMinus;
			else if (head[0] == '+' && head[1] == '=')
				kind = CF_TokenKind_SymPlusEqual;
			else if (head[0] == '-' && head[1] == '=')
				kind = CF_TokenKind_SymMinusEqual;
			else if (head[0] == '*' && head[1] == '=')
				kind = CF_TokenKind_SymAstEqual;
			else if (head[0] == '/' && head[1] == '=')
				kind = CF_TokenKind_SymSlashEqual;
			else if (head[0] == '&' && head[1] == '=')
				kind = CF_TokenKind_SymAmpEqual;
			else if (head[0] == '|' && head[1] == '=')
				kind = CF_TokenKind_SymPipeEqual;
			else if (head[0] == '^' && head[1] == '=')
				kind = CF_TokenKind_SymHatEqual;
			else if (head[0] == '%' && head[1] == '=')
				kind = CF_TokenKind_SymPercentEqual;
			else if (head[0] == '-' && head[1] == '>')
				kind = CF_TokenKind_SymArrow;

			if (kind)
			{
				PushToken_(&ctx, &(TokenToAppend_) {
					.kind = kind,
					.loc = loc,
					.range = SubstringToRange_(args->str, head, head+2),
				});
				head += 2;
				continue;
			}
		}

		// 1-char symbols
		{
			CF_TokenKind kind = 0;

			if (head[0] == '#')
				kind = CF_TokenKind_SymHash;
			else if (head[0] == '(')
				kind = CF_TokenKind_SymLeftParen;
			else if (head[0] == ')')
				kind = CF_TokenKind_SymRightParen;
			else if (head[0] == '[')
				kind = CF_TokenKind_SymLeftBrkt;
			else if (head[0] == ']')
				kind = CF_TokenKind_SymRightBrkt;
			else if (head[0] == '{')
				kind = CF_TokenKind_SymLeftCurl;
			else if (head[0] == '}')
				kind = CF_TokenKind_SymRightCurl;
			else if (head[0] == ':')
				kind = CF_TokenKind_SymColon;
			else if (head[0] == ';')
				kind = CF_TokenKind_SymSemicolon;
			else if (head[0] == ',')
				kind = CF_TokenKind_SymComma;
			else if (head[0] == '.')
				kind = CF_TokenKind_SymDot;
			else if (head[0] == '<')
				kind = CF_TokenKind_SymLess;
			else if (head[0] == '>')
				kind = CF_TokenKind_SymGreater;
			else if (head[0] == '=')
				kind = CF_TokenKind_SymEqual;
			else if (head[0] == '&')
				kind = CF_TokenKind_SymAmp;
			else if (head[0] == '|')
				kind = CF_TokenKind_SymPipe;
			else if (head[0] == '+')
				kind = CF_TokenKind_SymPlus;
			else if (head[0] == '-')
				kind = CF_TokenKind_SymMinus;
			else if (head[0] == '*')
				kind = CF_TokenKind_SymAst;
			else if (head[0] == '/')
				kind = CF_TokenKind_SymSlash;
			else if (head[0] == '~')
				kind = CF_TokenKind_SymTilde;
			else if (head[0] == '^')
				kind = CF_TokenKind_SymHat;
			else if (head[0] == '%')
				kind = CF_TokenKind_SymPercent;
			else if (head[0] == '!')
				kind = CF_TokenKind_SymBang;
			else if (head[0] == '?')
				kind = CF_TokenKind_SymQuestionMark;

			if (kind)
			{
				PushToken_(&ctx, &(TokenToAppend_) {
					.kind = kind,
					.loc = loc,
					.range = SubstringToRange_(args->str, head, head+1),
				});
				head += 1;
				continue;
			}
		}

		// Malformed token
		lbl_malformed_token:;
		CF_SourceRange malformed_range = SubstringToRange_(args->str, head, head+1);
		PushError_(&ctx, &(CF_LexError) {
			.range = malformed_range,
			.loc = loc,
			.token_index = ctx.token_count,
		});
		PushToken_(&ctx, &(TokenToAppend_) {
			.kind = CF_TokenKind_MalformedToken,
			.loc = loc,
			.range = malformed_range,
		});
		++head;
	}

	if (args->lexing_flags & CF_LexingFlag_AddNullTerminatingToken)
	{
		PushToken_(&ctx, &(TokenToAppend_) {
			.kind = CF_TokenKind_Null,
			.loc = loc,
			.range = { args->str.size, args->str.size }
		});
	}

	*out_result = (CF_TokensFromStringResult) {
		.token_count = ctx.token_count,
		.allocated_token_count = ctx.token_allocated_count,
		.token_kinds = ctx.token_kinds,
		.source_ranges = ctx.source_ranges,
		.locs = ctx.locs,
		.lex_error_count = ctx.lex_error_count,
		.lex_errors = ctx.lex_errors,
	};
	return ctx.err;
}

API CF_TokenKind
CF_KeywordFromString(String str, CF_Standard standard, uint32 lexing_flags)
{
	Trace();
	lexing_flags &= (CF_LexingFlag_AllowGnu | CF_LexingFlag_AllowMsvc);
	for (intsize i = 0; i < ArrayLength(g_keyword_spellings_table); ++i)
	{
		KeywordSpellingsEntry_ const* entry = &g_keyword_spellings_table[i];
		if (entry->lexing_flags && (entry->lexing_flags & lexing_flags) == 0)
			continue;
		if (!(entry->standard_flags & (1 << standard)))
			continue;
		if (!StringEquals(str, entry->spelling))
			continue;
		return entry->token_kind;
	}
	return 0;
}
