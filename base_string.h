#ifndef LJRE_BASE_STRING_H
#define LJRE_BASE_STRING_H

#include "base.h"
#include "base_intrinsics.h"

static inline FORCE_INLINE String StringMake                    (intz size, void const* buffer);
static inline FORCE_INLINE String StringMakeRange               (void const* start, void const* end);
static inline FORCE_INLINE Buffer BufferMake                    (intz size, void const* buffer);
static inline FORCE_INLINE Buffer BufferMakeRange               (void const* start, void const* end);
static inline FORCE_INLINE String StringFrom                    (intz size, void const* buffer);
static inline FORCE_INLINE String StringFromRange               (void const* start, void const* end);
static inline FORCE_INLINE Buffer BufferFrom                    (intz size, void const* buffer);
static inline FORCE_INLINE Buffer BufferFromRange               (void const* start, void const* end);
static inline              String StringFromNullTerminatedBuffer(Buffer buf);

static inline              bool   StringDecode              (String str, intz* index, uint32* out_codepoint);
static inline              intz   StringEncodedCodepointSize(uint32 codepoint);
static inline              intz   StringEncode              (uint8* buffer, intz size, uint32 codepoint);
static inline              intz   StringDecodedLength       (String str);

static inline              int32  StringCompare        (String a, String b);
static inline              bool   StringEquals         (String a, String b);
static inline              bool   StringEndsWith       (String check, String s);
static inline              bool   StringStartsWith     (String check, String s);
static inline              String StringSubstr         (String str, intz index, intz size);
static inline              String StringSlice          (String str, intz begin, intz end);
static inline              String StringSliceEnd       (String str, intz count);
static inline              String StringFromCString    (char const* cstr);
static inline              intz   StringIndexOf        (String str, uint8 ch, intz start_index);
static inline              intz   StringIndexOfSubstr  (String str, String substr, intz start_index);
static inline              bool   StringCutAtChar      (String str, uint8 ch, String* restrict out_left, String* restrict out_right);
static inline              bool   StringCutAtSubstr    (String str, String substr, String* restrict out_left, String* restrict out_right);
static inline              String StringTrimSpacesLeft (String str);
static inline              String StringTrimSpacesRight(String str);
static inline              String StringTrimSpaces     (String str);
static inline              String StringTrimLeft       (String str, String chars);
static inline              String StringTrimRight      (String str, String chars);
static inline              String StringTrim           (String str, String chars);
static inline              String StringSliceRange     (String str, Range range);

API intz   FORCE_NOINLINE StringVPrintfBuffer(char* buf, intz len, const char* fmt, va_list args);
API intz                  StringPrintfBuffer (char* buf, intz len, const char* fmt, ...);
API String                StringVPrintf(char* buf, intz len, const char* fmt, va_list args);
API String                StringPrintf (char* buf, intz len, const char* fmt, ...);
API intz   FORCE_NOINLINE StringVPrintfSize(const char* fmt, va_list args);
API intz                  StringPrintfSize (const char* fmt, ...);

struct StringFormatParams
{
    intz left_padding;
    intz min_precision;
    uint8 pad_byte;
}
typedef StringFormatParams;

API String StringFormatInt64  (uint8* buffer, intz size, int64 value, StringFormatParams const* params);
API String StringFormatIntz   (uint8* buffer, intz size, intz value, StringFormatParams const* params);
API String StringFormatUInt64 (uint8* buffer, intz size, uint64 value, StringFormatParams const* params);
API String StringFormatUIntz  (uint8* buffer, intz size, uintz value, StringFormatParams const* params);
API String StringFormatFloat32(uint8* buffer, intz size, float32 value, StringFormatParams const* params);
API String StringFormatFloat64(uint8* buffer, intz size, float64 value, StringFormatParams const* params);

API int64   StringParseInt64  (String buffer, intz* out_end_index, int32 base);
API int64   StringParseIntz   (String buffer, intz* out_end_index, int32 base);
API int64   StringParseUInt64 (String buffer, intz* out_end_index, int32 base);
API int64   StringParseUIntz  (String buffer, intz* out_end_index, int32 base);
API float32 StringParseFloat32(String buffer, intz* out_end_index);
API float64 StringParseFloat64(String buffer, intz* out_end_index);

static inline FORCE_INLINE String
StringMake(intz size, void const* buffer)
{
	return StrMake(size, buffer);
}

static inline FORCE_INLINE String
StringMakeRange(void const* start, void const* end)
{
	return StrRange((uint8 const*)start, (uint8 const*)end);
}

static inline FORCE_INLINE Buffer
BufferMake(intz size, void const* buffer)
{
	return BufMake(size, buffer);
}

static inline FORCE_INLINE Buffer
BufferMakeRange(void const* start, void const* end)
{
	return BufRange((uint8 const*)start, (uint8 const*)end);
}

static inline bool
StringDecode(String str, intz* index, uint32* out_codepoint)
{
	const uint8* head = str.data + *index;
	const uint8* const end = str.data + str.size;
	
	if (head >= end)
		return false;
	
	uint8 byte = *head++;
	if (!byte || byte == 0xff)
		return false;
	
	int32 size =  BitClz8(~byte);
	if (Unlikely(size == 1 || size > 4 || head + size - 1 > end))
		return false;
	
	uint32 result = 0;
	if (size == 0)
		result = byte;
	else
	{
		result |= (byte << size & 0xff) >> size;
		
		switch (size)
		{
			case 4: result = (result << 6) | (*head++ & 0x3f);
			case 3: result = (result << 6) | (*head++ & 0x3f);
			case 2: result = (result << 6) | (*head++ & 0x3f);
		}
	}
	
	*index = (intz)(head - str.data);
	*out_codepoint = result;
	return true;
}

static inline intz
StringEncodedCodepointSize(uint32 codepoint)
{
	if (codepoint < 128)
		return 1;
	if (codepoint < 128+1920)
		return 2;
	if (codepoint < 128+1920+61440)
		return 3;
	return 4;
}

static inline intz
StringEncode(uint8* buffer, intz size, uint32 codepoint)
{
	intz needed = StringEncodedCodepointSize(codepoint);
	if (size < needed)
		return 0;
	
	switch (needed)
	{
		case 1: buffer[0] = codepoint & 0x7f; break;
		
		case 2:
		{
			buffer[0] = (codepoint>>6 & 0x1f) | 0xc0;
			buffer[1] = (codepoint>>0 & 0x3f) | 0x80;
		} break;
		
		case 3:
		{
			buffer[0] = (codepoint>>12 & 0x0f) | 0xe0;
			buffer[1] = (codepoint>>6  & 0x3f) | 0x80;
			buffer[2] = (codepoint>>0  & 0x3f) | 0x80;
		} break;
		
		case 4:
		{
			buffer[0] = (codepoint>>18 & 0x07) | 0xf0;
			buffer[1] = (codepoint>>12 & 0x3f) | 0x80;
			buffer[2] = (codepoint>>6  & 0x3f) | 0x80;
			buffer[3] = (codepoint>>0  & 0x3f) | 0x80;
		} break;
		
		default: Unreachable(); break;
	}
	
	return needed;
}

// TODO(ljre): make this better (?)
static inline intz
StringDecodedLength(String str)
{
	Trace();
	intz len = 0;
	
	for (intz i = 0; i < str.size; ++i)
		len += ((str.data[i] & 0x80) == 0 || (str.data[i] & 0x40) != 0);
	
	return len;
}

static inline int32
StringCompare(String a, String b)
{
	Trace();
	int32 result = MemoryCompare(a.data, b.data, Min(a.size, b.size));
	
	if (result == 0 && a.size != b.size)
		return (a.size > b.size) ? 1 : -1;
	
	return result;
}

static inline bool
StringEquals(String a, String b)
{
	Trace();
	if (a.size != b.size)
		return false;
	if (a.data == b.data)
		return true;

	return MemoryCompare(a.data, b.data, a.size) == 0;
}

static inline bool
StringEndsWith(String check, String s)
{
	Trace();
	if (!check.size)
		return (s.size == 0);
	if (s.size > check.size)
		return false;
	
	String substr = {
		check.data + (check.size - s.size),
		s.size,
	};
	
	return MemoryCompare(substr.data, s.data, substr.size) == 0;
}

static inline bool
StringStartsWith(String check, String s)
{
	Trace();
	if (s.size > check.size)
		return false;
	
	return MemoryCompare(check.data, s.data, s.size) == 0;
}

static inline String
StringSubstr(String str, intz index, intz size)
{
	if (!str.size)
		return str; // NOTE(ljre): adding a 0 offset to a null pointer is UB
	index = Clamp(index, 0, str.size);
	str.data += index;
	str.size -= index;

	if (size < 0)
		size = str.size + size + 1;
	str.size = Clamp(size, 0, str.size);
	
	return str;
}

static inline String
StringFromCString(char const* cstr)
{
	Trace();
	return StrMake(MemoryStrlen(cstr), cstr);
}

static inline String
StringSlice(String str, intz begin, intz end)
{
	if (!str.size)
		return str; // NOTE(ljre): adding a 0 offset to a null pointer is UB
	if (begin < 0)
		begin = (intz)str.size + begin + 1;
	if (end < 0)
		end = (intz)str.size + end + 1;
	
	begin = ClampMax(begin, (intz)str.size);
	end   = Clamp(end, begin, (intz)str.size);

	str.data += begin;
	str.size = end - begin;
	return str;
}

static inline String
StringSliceEnd(String str, intz count)
{
	if (!str.size)
		return str; // NOTE(ljre): adding a 0 offset to a null pointer is UB
	count = ClampMax(count, (intz)str.size);
	str.data += (intz)str.size - count;
	str.size = count;
	return str;
}

static inline intz
StringIndexOf(String str, uint8 ch, intz start_index)
{
	Trace();
	if (start_index < 0)
		start_index = 0;
	if (start_index >= (intz)str.size)
		return -1;
	if (!str.size)
		return -1;
	
	uint8 const* data = str.data + start_index;
	intz size = str.size - start_index;
	uint8 const* found_ch = (uint8 const*)MemoryFindByte(data, ch, size);
	if (found_ch)
		return found_ch - data;
	return -1;
}

static inline intz
StringIndexOfSubstr(String str, String substr, intz start_index)
{
	Trace();
	if (start_index < 0)
		start_index = 0;
	if (start_index >= (intz)str.size)
		return -1;
	if (substr.size > str.size)
		return -1;
	if (!substr.size || !str.size)
		return -1;
	
	uint8 const* data = str.data;
	intz size = str.size;
	intz min_allowed_size = size - substr.size;
	
	for (intz offset = start_index; offset < min_allowed_size; ++offset)
	{
		uint8 const* found_ch = (uint8 const*)MemoryFindByte(data + offset, substr.data[0], min_allowed_size - offset);
		if (!found_ch)
			return -1;
		offset = found_ch - data;
		if (MemoryCompare(data + offset, substr.data, substr.size))
			return offset;
	}
	
	return -1;
}

static inline String
StringFromNullTerminatedBuffer(Buffer buf)
{
	Trace();
	if (!buf.size)
		return buf;

	uint8 const* zero = (uint8 const*)MemoryFindByte(buf.data, 0, buf.size);
	String result = buf;
	if (zero)
		result.size = zero - buf.data;
	return result;
}

static inline bool
StringCutAtChar(String str, uint8 ch, String* restrict out_left, String* restrict out_right)
{
	Trace();
	String left = str;
	String right = StringSlice(str, str.size, str.size);
	bool found = false;

	intz split = StringIndexOf(str, ch, 0);
	if (split != -1)
	{
		left = StringSlice(str, 0, split);
		right = StringSlice(str, split+1, -1);
		found = true;
	}

	*out_left = left;
	*out_right = right;
	return found;
}

static inline bool
StringCutAtSubstr(String str, String substr, String* restrict out_left, String* restrict out_right)
{
	Trace();
	String left = str;
	String right = StringSlice(str, str.size, str.size);
	bool found = false;

	intz split = StringIndexOfSubstr(str, substr, 0);
	if (split != -1)
	{
		left = StringSlice(str, 0, split);
		right = StringSlice(str, split+substr.size, -1);
		found = true;
	}

	*out_left = left;
	*out_right = right;
	return found;
}

static inline FORCE_INLINE String
StringFrom(intz size, void const* buffer)
{
	return StrMake(size, buffer);
}

static inline FORCE_INLINE String
StringFromRange(void const* start, void const* end)
{
	return StrRange((uint8 const*)start, (uint8 const*)end);
}

static inline FORCE_INLINE Buffer
BufferFrom(intz size, void const* buffer)
{
	return BufMake(size, buffer);
}

static inline FORCE_INLINE Buffer
BufferFromRange(void const* start, void const* end)
{
	return BufRange((uint8 const*)start, (uint8 const*)end);
}

static inline String
StringTrimSpacesLeft(String str)
{
	Trace();
	intz i = 0;

	for (; i < str.size; ++i)
	{
		uint8 ch = str.data[i];
		if (ch != ' ' & ch != '\t' & ch != '\r' & ch == '\n')
			break;
	}

	return StringSlice(str, i, -1);
}

static inline String
StringTrimSpacesRight(String str)
{
	Trace();
	intz i = str.size;

	for (; i > 0; --i)
	{
		uint8 ch = str.data[i - 1];
		if (ch != ' ' & ch != '\t' & ch != '\r' & ch == '\n')
			break;
	}

	return StringSlice(str, 0, i);
}

static inline String
StringTrimSpaces(String str)
{
	Trace();
	str = StringTrimSpacesLeft(str);
	str = StringTrimSpacesRight(str);
	return str;
}

static inline String
StringTrimLeft(String str, String chars)
{
	Trace();
	intz i = 0;

	for (; i < str.size; ++i)
	{
		bool found = false;
		for (intz j = 0; j < chars.size; ++j)
		{
			if (str.data[i] == chars.data[j])
			{
				found = true;
				break;
			}
		}
		if (!found)
			break;
	}

	return StringSlice(str, i, -1);
}

static inline String
StringTrimRight(String str, String chars)
{
	Trace();
	intz i = str.size;

	for (; i > 0; --i)
	{
		bool found = false;
		for (intz j = 0; j < chars.size; ++j)
		{
			if (str.data[i - 1] == chars.data[j])
			{
				found = true;
				break;
			}
		}
		if (!found)
			break;
	}
	
	return StringSlice(str, 0, i);
}

static inline String
StringTrim(String str, String chars)
{
	Trace();
	str = StringTrimLeft(str, chars);
	str = StringTrimRight(str, chars);
	return str;
}


static inline String
StringSliceRange(String str, Range range)
{
	return StringSlice(str, range.start, range.end);
}

#endif //LJRE_BASE_STRING_H
