#ifndef JSON_API_H
#define JSON_API_H

#include "common_basic.h"

enum JSON_Type
{
	JSON_Type_Invalid = 0,
	JSON_Type_Null,
	JSON_Type_Boolean,
	JSON_Type_String,
	JSON_Type_Number,
	JSON_Type_Array,
	JSON_Type_Object,
}
typedef JSON_Type;

struct JSON_Value
{
	JSON_Type type;
	uint8 const* start;
	uint8 const* end;
}
typedef JSON_Value;

struct JSON_ObjectIt
{
	JSON_Value object;
	String field_name;
	JSON_Value field_value;
}
typedef JSON_ObjectIt;

struct JSON_ArrayIt
{
	JSON_Value array;
	intz elem_index;
	JSON_Value elem_value;
}
typedef JSON_ArrayIt;

// Value initialization and iteration
API JSON_Value JSON_ValueFromString (String str);
API bool       JSON_IterateObject   (JSON_ObjectIt* it);
API bool       JSON_IterateArray    (JSON_ArrayIt* it);

// Specific checks on values
API bool       JSON_IsValueInteger  (JSON_Value value);
API bool       JSON_IsValueBigInt   (JSON_Value value);

// Parse value as specific type
API int32      JSON_Int32FromValue  (JSON_Value value, int32 default_value, bool truncate_if_needed);
API int64      JSON_Int64FromValue  (JSON_Value value, int64 default_value, bool truncate_if_needed);
API float32    JSON_Float32FromValue(JSON_Value value, float32 default_value);
API float64    JSON_Float64FromValue(JSON_Value value, float64 default_value);
API bool       JSON_BoolFromValue   (JSON_Value value, bool default_value);
API String     JSON_StringFromValue (JSON_Value value, String default_value);
API String     JSON_JsonFromValue   (JSON_Value value); // returns the underlying json representation

// String escaping
API bool       JSON_StringNeedsEscaping(String str);
API String     JSON_EscapeStringToAllocator(String str, SingleAllocator allocator, AllocatorError* out_err);
API String     JSON_EscapeStringToArena(String str, Arena* arena);

#endif