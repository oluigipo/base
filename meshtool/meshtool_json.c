enum JsonValueKind_
{
	JsonValueKind_Invalid = 0,
	JsonValueKind_Null,
	JsonValueKind_Undefined,
	JsonValueKind_Number,
	JsonValueKind_String,
	JsonValueKind_Boolean,
	JsonValueKind_Object,
	JsonValueKind_Array,
}
typedef JsonValueKind_;

struct JsonValue_
{
	JsonValueKind_ kind;
	uint8 const* begin;
	uint8 const* end;
}
typedef JsonValue_;

struct JsonField_
{
	JsonValue_ object;
	JsonValueKind_ value_kind;
	uint8 const* name_begin;
	uint8 const* name_end;
	uint8 const* value_begin;
	uint8 const* value_end;
}
typedef JsonField_;

struct JsonIndex_
{
	JsonValue_ array;
	intsize numeric_index;
	JsonValueKind_ value_kind;
	uint8 const* value_begin;
	uint8 const* value_end;
}
typedef JsonIndex_;

static inline JsonValue_ JsonMakeValue_    (uint8 const* data, uintsize size);
static inline bool       JsonNextField_    (JsonField_* field);
static inline String     JsonGetFieldName_ (JsonField_ field);
static inline JsonValue_ JsonGetFieldValue_(JsonField_ field);

static inline bool       JsonNextIndex_    (JsonIndex_* index);
static inline intsize    JsonGetIndexI_    (JsonIndex_ index);
static inline JsonValue_ JsonGetIndexValue_(JsonIndex_ index);

static inline int32      JsonGetValueInt32_  (JsonValue_ value);
static inline int64      JsonGetValueInt64_  (JsonValue_ value);
static inline float32    JsonGetValueFloat32_(JsonValue_ value);
static inline float64    JsonGetValueFloat64_(JsonValue_ value);
static inline String     JsonGetValueString_ (JsonValue_ value); // NOTE(ljre): No escapes parsed!
static inline bool       JsonGetValueBool_   (JsonValue_ value);

//~ Implementation
static uint8 const*
JsonTrimSpacesLeft__(uint8 const* begin, uint8 const* end)
{
	while (begin < end && (begin[0] == ' ' || begin[0] == '\t' || begin[0] == '\n' || begin[0] == '\r'))
		++begin;
	return begin;
}

static uint8 const*
JsonFindValueEnd__(uint8 const* begin, uint8 const* end, JsonValueKind_* out_kind)
{
	JsonValueKind_ kind = 0;
	uint8 const* new_end = NULL;
	
	if (begin < end)
	{
		switch (begin[0])
		{
			case '{':
			{
				int32 nesting = 1;
				++begin;
				while (nesting > 0 && begin < end)
				{
					if (begin[0] == '{')
						++nesting;
					else if (begin[0] == '}')
						--nesting;
					else if (begin[0] == '"')
					{
						++begin;
						if (begin >= end)
							goto out_of_loop1;
						while (begin[0] != '"')
						{
							if (begin+1 < end && begin[0] == '\\' && (begin[1] == '"' || begin[1] == '\\'))
								begin += 2;
							else
								++begin;
							if (begin >= end)
								goto out_of_loop1;
						}
					}
					++begin;
				}
				
				out_of_loop1:;
				if (nesting == 0)
				{
					kind = JsonValueKind_Object;
					new_end = begin;
				}
			} break;
			case '[':
			{
				int32 nesting = 1;
				++begin;
				while (nesting > 0 && begin < end)
				{
					if (begin[0] == '[')
						++nesting;
					else if (begin[0] == ']')
						--nesting;
					else if (begin[0] == '"')
					{
						++begin;
						if (begin >= end)
							goto out_of_loop2;
						while (begin[0] != '"')
						{
							if (begin+1 < end && begin[0] == '\\' && (begin[1] == '"' || begin[1] == '\\'))
								begin += 2;
							else
								++begin;
							if (begin >= end)
								goto out_of_loop2;
						}
					}
					
					++begin;
				}
				
				out_of_loop2:;
				if (nesting == 0)
				{
					kind = JsonValueKind_Array;
					new_end = begin;
				}
			} break;
			case '0': case '1': case '2': case '3': case '4':
			case '5': case '6': case '7': case '8': case '9':
			{
				while (begin < end && begin[0] >= '0' && begin[0] <= '9')
					++begin;
				if (begin < end && begin[0] == '.')
				{
					++begin;
					while (begin < end && begin[0] >= '0' && begin[0] <= '9')
						++begin;
				}
				
				kind = JsonValueKind_Number;
				new_end = begin;
			} break;
			case '"':
			{
				++begin;
				while (begin < end && begin[0] != '"')
				{
					if (begin+1 < end && begin[0] == '\\' && (begin[1] == '"' || begin[1] == '\\'))
						begin += 2;
					else
						++begin;
				}
				
				if (begin < end)
				{
					kind = JsonValueKind_String;
					new_end = begin+1;
				}
			} break;
			default:
			{
				if (begin+4 < end && StringEquals(Str("null"), StrMake(begin, 4)))
				{
					kind = JsonValueKind_Null;
					new_end = begin+4;
				}
				else if (begin+9 < end && StringEquals(Str("undefined"), StrMake(begin, 9)))
				{
					kind = JsonValueKind_Undefined;
					new_end = begin+9;
				}
				else if (begin+4 < end && StringEquals(Str("true"), StrMake(begin, 4)))
				{
					kind = JsonValueKind_Boolean;
					new_end = begin+4;
				}
				else if (begin+5 < end && StringEquals(Str("false"), StrMake(begin, 5)))
				{
					kind = JsonValueKind_Boolean;
					new_end = begin+5;
				}
			} break;
		}
	}
	
	if (out_kind)
		*out_kind = kind;
	return new_end;
}

static inline JsonValue_
JsonMakeValue_(uint8 const* data, uintsize size)
{
	JsonValueKind_ kind;
	uint8 const* end = JsonFindValueEnd__(data, data+size, &kind);
	JsonValue_ value = {
		.kind = kind,
		.begin = data,
		.end = end,
	};
	
	return value;
}

static inline bool
JsonNextField_(JsonField_* field)
{
	uint8 const* head = field->value_end;
	uint8 const* end = field->object.end;
	
	if (!head)
	{
		head = field->object.begin;
		if (head >= end || head[0] != '{')
			return false;
	}
	else
	{
		head = JsonTrimSpacesLeft__(head, end);
		if (head >= end || head[0] != ',')
			return false;
	}
	
	++head;
	head = JsonTrimSpacesLeft__(head, end);
	if (head+2 >= end || head[0] != '"')
		return false;
	
	uint8 const* name_begin = head;
	++head;
	while (head[0] != '"')
	{
		if (head[0] == '\\' && head+1 < end && (head[1] == '"' || head[1] == '\\'))
			head += 2;
		else
			++head;
		if (head >= end)
			return false;
	}
	++head;
	uint8 const* name_end = head;
	
	head = JsonTrimSpacesLeft__(head, end);
	if (head >= end || head[0] != ':')
		return false;
	
	++head;
	if (head >= end)
		return false;
	
	head = JsonTrimSpacesLeft__(head, end);
	uint8 const* value_begin = head;
	JsonValueKind_ kind;
	uint8 const* value_end = JsonFindValueEnd__(head, end, &kind);
	if (!value_end || kind == 0)
		return false;
	
	field->name_begin = name_begin;
	field->name_end = name_end;
	field->value_begin = value_begin;
	field->value_end = value_end;
	field->value_kind = kind;
	return true;
}

static inline String
JsonGetFieldName_(JsonField_ field)
{
	uint8 const* begin = field.name_begin;
	uint8 const* end = field.name_end;
	
	if (begin >= end)
		return StrNull;
	if (begin[0] != '"' || end[-1] != '"')
		return StrNull;
	++begin;
	--end;
	return StrRange(begin, end);
}

static inline JsonValue_
JsonGetFieldValue_(JsonField_ field)
{
	JsonValue_ value = {
		.kind = field.value_kind,
		.begin = field.value_begin,
		.end = field.value_end,
	};
	return value;
}

static inline bool
JsonNextIndex_(JsonIndex_* index)
{
	const uint8* head = index->value_end;
	const uint8* end = index->array.end;
	intsize numeric_index;
	
	if (!head)
	{
		numeric_index = 0;
		head = index->array.begin;
		if (head >= end || head[0] != '[')
			return false;
	}
	else
	{
		numeric_index = index->numeric_index+1;
		head = JsonTrimSpacesLeft__(head, end);
		if (head >= end || head[0] != ',')
			return false;
	}
	
	++head;
	head = JsonTrimSpacesLeft__(head, end);
	if (head >= end)
		return false;
	
	const uint8* value_begin = head;
	JsonValueKind_ kind;
	const uint8* value_end = JsonFindValueEnd__(head, end, &kind);
	if (!value_end || kind == 0)
		return false;
	
	index->numeric_index = numeric_index;
	index->value_kind = kind;
	index->value_begin = value_begin;
	index->value_end = value_end;
	return true;
}

static inline intsize
JsonGetIndexI_(JsonIndex_ index)
{
	return index.numeric_index;
}

static inline JsonValue_
JsonGetIndexValue_(JsonIndex_ index)
{
	JsonValue_ value = {
		.kind = index.value_kind,
		.begin = index.value_begin,
		.end = index.value_end,
	};
	return value;
}

static inline int32
JsonGetValueInt32_(JsonValue_ value)
{
	SafeAssert(value.kind == JsonValueKind_Number);
	char buf[128] = { 0 };
	intsize size = Min(sizeof(buf)-1, value.end-value.begin);
	MemoryCopy(buf, value.begin, size);
	
	int32 ret = (int32)strtol(buf, &(char*) { 0 }, 10);
	return ret;
}

static inline int64
JsonGetValueInt64_  (JsonValue_ value)
{
	SafeAssert(value.kind == JsonValueKind_Number);
	char buf[128] = { 0 };
	intsize size = Min(sizeof(buf)-1, value.end-value.begin);
	MemoryCopy(buf, value.begin, size);
	
	int64 ret = strtoll(buf, &(char*) { 0 }, 10);
	return ret;
}

static inline float32
JsonGetValueFloat32_(JsonValue_ value)
{
	SafeAssert(value.kind == JsonValueKind_Number);
	char buf[128] = { 0 };
	intsize size = Min(sizeof(buf)-1, value.end-value.begin);
	MemoryCopy(buf, value.begin, size);
	
	float32 ret = strtof(buf, &(char*) { 0 });
	return ret;
}

static inline float64
JsonGetValueFloat64_(JsonValue_ value)
{
	SafeAssert(value.kind == JsonValueKind_Number);
	char buf[128] = { 0 };
	intsize size = Min(sizeof(buf)-1, value.end-value.begin);
	MemoryCopy(buf, value.begin, size);
	
	float64 ret = strtod(buf, &(char*) { 0 });
	return ret;
}

static inline String
JsonGetValueString_(JsonValue_ value)
{
	SafeAssert(value.kind == JsonValueKind_String);
	return StrRange(value.begin+1, value.end-1);
}

static inline bool
JsonGetValueBool_(JsonValue_ value)
{
	SafeAssert(value.kind == JsonValueKind_Boolean);
	return value.begin[0] == 't';
}
