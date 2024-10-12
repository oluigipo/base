#include "common.h"
#include "api.h"

#include <stdlib.h> // for strtol, strtoll, strtof, strtod

static uint8 const*
TrimSpacesLeft_(uint8 const* head, uint8 const* end)
{
	while (head < end && (head[0] == ' ' || head[0] == '\t' || head[0] == '\n' || head[0] == '\r'))
		++head;
	return head;
}

static bool
FindEndOfString_(uint8 const** phead, uint8 const* end)
{
	Trace();
	uint8 const* head = *phead;
	bool result = false;

	while (head < end)
	{
		if (head[0] == '\\')
		{
			head += 2;
			if (head >= end)
			{
				head = end;
				break;
			}
		}
		else if (head[0] == '"')
		{
			result = true;
			++head;
			break;
		}
		++head;
	}

	*phead = head;
	return result;
}

static JSON_Value
ParseValue_(uint8 const* start, uint8 const* end)
{
	Trace();
	start = TrimSpacesLeft_(start, end);
	uint8 const* head = start;
	JSON_Type type = 0;

	if (head < end)
	{
		if (head[0] == '{' || head[0] == '[')
		{
			uint8 open = head[0];
			uint8 close = (head[0] == '{') ? '}' : ']';
			intz nesting = 1;
			++head;
			while (nesting > 0 && head < end)
			{
				uint8 ch = *head++;
				if (ch == open)
					++nesting;
				else if (ch == close)
					--nesting;
				else if (ch == '"' && !FindEndOfString_(&head, end))
					break;
			}
			if (nesting == 0)
				type = JSON_Type_Object;
		}
		else if (head[0] >= '0' && head[0] <= '9' || head[0] == '-')
		{
			do
				++head;
			while (head < end && head[0] >= '0' && head[0] <= '9');
			if (head < end && head[0] == '.')
			{
				do
					++head;
				while (head < end && head[0] >= '0' && head[0] <= '9');
			}
			if (head < end && (head[0] == 'e' || head[0] == 'E'))
			{
				++head;
				if (head < end && (head[0] == '+' || head[0] == '-'))
					++head;
				while (head < end && head[0] >= '0' && head[0] <= '9')
					++head;
			}
			type = JSON_Type_Number;
		}
		else if (head[0] == '"')
		{
			++head;
			if (FindEndOfString_(&head, end))
				type = JSON_Type_String;
		}
		else
		{
			String as_str = StringMakeRange(head, end);
			if (StringStartsWith(as_str, Str("null")))
			{
				head += 4;
				type = JSON_Type_Null;
			}
			else if (StringStartsWith(as_str, Str("true")))
			{
				head += 4;
				type = JSON_Type_Boolean;
			}
			else if (StringStartsWith(as_str, Str("false")))
			{
				head += 5;
				type = JSON_Type_Boolean;
			}

			if (head < end && type != 0)
			{
				// NOTE(ljre): Check if identifier has not ended
				if (head[0] != ' '  && head[0] != '\t' &&
					head[0] != '\n' && head[0] != '\r' &&
					head[0] != ','  && head[0] != '}' &&
					head[0] != ']')
				{
					type = 0;
				}
			}
		}
	}

	return (JSON_Value) {
		.type = type,
		.start = start,
		.end = head,
	};
}

API JSON_Value
JSON_ValueFromString(String str)
{
	Trace();
	return ParseValue_(str.data, str.data + str.size);
}

API bool
JSON_IterateObject(JSON_ObjectIt* it)
{
	Trace();
	uint8 const* head;
	uint8 const* end = it->object.end;
	if (it->field_value.end)
	{
		head = it->field_value.end;
		head = TrimSpacesLeft_(head, end);
		if (head >= end || head[0] != ',')
			return false;
	}
	else
	{
		head = it->object.start;
		if (head >= end || head[0] != '{')
			return false;
	}

	++head; // eat ',' or '{'
	head = TrimSpacesLeft_(head, end);
	if (head+2 >= end || head[0] != '"')
		return false;
	++head; // eat '"'

	uint8 const* name_start = head;
	if (!FindEndOfString_(&head, end))
		return false;
	uint8 const* name_end = head - 1;

	head = TrimSpacesLeft_(head, end);
	if (head >= end || head[0] != ':')
		return false;

	++head; // eat ':'
	head = TrimSpacesLeft_(head, end);
	if (head >= end)
		return false;

	JSON_Value field_value = ParseValue_(head, end);
	if (!field_value.type)
		return false;

	it->field_name = StringMakeRange(name_start, name_end);
	it->field_value = field_value;
	return true;
}

API bool
JSON_IterateArray(JSON_ArrayIt* it)
{
	Trace();
	uint8 const* head;
	uint8 const* end = it->array.end;
	if (it->elem_value.end)
	{
		head = it->elem_value.end;
		head = TrimSpacesLeft_(head, end);
		if (head >= end || head[0] != ',')
			return false;
	}
	else
	{
		head = it->array.start;
		if (head >= end || head[0] != '[')
			return false;
	}

	++head; // eat ',' or '['
	head = TrimSpacesLeft_(head, end);
	if (head >= end)
		return false;

	JSON_Value elem_value = ParseValue_(head, end);
	if (!elem_value.type)
		return false;

	++it->elem_index;
	it->elem_value = elem_value;
	return true;
}

API int32
JSON_Int32FromValue(JSON_Value value, int32 default_value, bool truncate_if_needed)
{
	int32 result = default_value;
	if (value.type == JSON_Type_Number)
	{
		String as_str = StringMakeRange(value.start, value.end);
		if (as_str.size >= 256)
			return default_value;
		if (!truncate_if_needed && (
				MemoryFindByte(as_str.data, '.', as_str.size) ||
				MemoryFindByte(as_str.data, 'e', as_str.size) ||
				MemoryFindByte(as_str.data, 'E', as_str.size)))
			return default_value;

		char tmpbuf[256] = {};
		MemoryCopy(tmpbuf, as_str.data, as_str.size);
		result = strtol(tmpbuf, &(char*) {}, 10);
	}
	return result;
}

API int64
JSON_Int64FromValue(JSON_Value value, int64 default_value, bool truncate_if_needed)
{
	int64 result = default_value;
	if (value.type == JSON_Type_Number)
	{
		String as_str = StringMakeRange(value.start, value.end);
		if (as_str.size >= 256)
			return default_value;
		if (!truncate_if_needed && (
				MemoryFindByte(as_str.data, '.', as_str.size) ||
				MemoryFindByte(as_str.data, 'e', as_str.size) ||
				MemoryFindByte(as_str.data, 'E', as_str.size)))
			return default_value;

		char tmpbuf[256] = {};
		MemoryCopy(tmpbuf, as_str.data, as_str.size);
		result = strtoll(tmpbuf, &(char*) {}, 10);
	}
	return result;
}

API float32
JSON_Float32FromValue(JSON_Value value, float32 default_value)
{
	float32 result = default_value;
	if (value.type == JSON_Type_Number)
	{
		String as_str = StringMakeRange(value.start, value.end);
		if (as_str.size >= 256)
			return default_value;
		char tmpbuf[256] = {};
		MemoryCopy(tmpbuf, as_str.data, as_str.size);
		result = strtof(tmpbuf, &(char*) {});
	}
	return result;
}

API float64
JSON_Float64FromValue(JSON_Value value, float64 default_value)
{
	float64 result = default_value;
	if (value.type == JSON_Type_Number)
	{
		String as_str = StringMakeRange(value.start, value.end);
		if (as_str.size >= 256)
			return default_value;
		char tmpbuf[256] = {};
		MemoryCopy(tmpbuf, as_str.data, as_str.size);
		result = strtod(tmpbuf, &(char*) {});
	}
	return result;
}

API bool
JSON_BoolFromValue(JSON_Value value, bool default_value)
{
	bool result = default_value;
	if (value.type == JSON_Type_Boolean)
		result = (value.start[0] == 't');
	return result;
}

API String
JSON_StringFromValue(JSON_Value value, String default_value)
{
	String result = default_value;
	if (value.type == JSON_Type_String)
		result = StringSlice(StringMakeRange(value.start, value.end), 1, -2);
	return result;
}

API String
JSON_JsonFromValue(JSON_Value value)
{
	return StringMakeRange(value.start, value.end);
}

API bool
JSON_StringNeedsEscaping(String str)
{
	return MemoryFindByte(str.data, '\\', str.size) != NULL;
}

API String
JSON_EscapeStringToAllocator(String str, SingleAllocator allocator, AllocatorError* out_err)
{
	// TODO
	return AllocatorCloneString(&allocator, str, out_err);
}

API String
JSON_EscapeStringToArena(String str, Arena* arena)
{
	// TODO
	return ArenaPushString(arena, str);
}
