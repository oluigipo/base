#include "common.h"
#include "api_os.h"

//~ Parser
uint16 typedef Object_;
enum Object_
{
	Button_A = 1,
	Button_B = 2,
	Button_X = 3,
	Button_Y = 4,
	Button_Left = 5,
	Button_Right = 6,
	Button_Up = 7,
	Button_Down = 8,
	Button_LeftShoulder = 9, // LB
	Button_RightShoulder = 10, // RB
	Button_LeftStick = 11, // LS
	Button_RightStick = 12, // RS
	Button_Start = 13,
	Button_Back = 14,
	
	Pressure_LeftTrigger = 15, // LT
	Pressure_RightTrigger = 16, // RT
	
	Axis_LeftX = 17,
	Axis_LeftY = 18,
	Axis_RightX = 19,
	Axis_RightY = 20,
	
	// The lower bits of this enum is one of these entries specified above.
	// The higher bits are used to store extra information about the object.
	//
	// Higher Bits: 0b7654_3210
	// 7 = If the axis should be inverted
	// 6 = If the input axis should be limited to 0..MAX
	// 5 = If the input axis should be limited to MIN..0
	// 4 = If the output axis should be limited to 0..MAX
	// 3 = If the output axis should be limited to MIN..0
	//
	// Bits 6 & 5 should never be set at the same time
	// Bits 4 & 3 should never be set at the same time
	
	Count = 21,
};

struct Controller_
{
	uint8 guid[32];
	String name;
	
	Object_ buttons[32];
	Object_ axes[16];
	Object_ povs[8][4];
}
typedef Controller_;

static bool ParseEntry_(String line, Controller_* out_controller, String* out_platform);

//- Implementation
static Object_
FindObjectFromName_(String name)
{
	struct
	{
		String name;
		Object_ object;
	}
	static const table[] = {
		{ StrInit("a"), Button_A },
		{ StrInit("b"), Button_B },
		{ StrInit("back"), Button_Back },
		
		{ StrInit("dpdown"), Button_Down },
		{ StrInit("dpleft"), Button_Left },
		{ StrInit("dpright"), Button_Right },
		{ StrInit("dpup"), Button_Up },
		
		{ StrInit("leftshoulder"), Button_LeftShoulder },
		{ StrInit("leftstick"), Button_LeftStick },
		{ StrInit("lefttrigger"), Pressure_LeftTrigger },
		{ StrInit("leftx"), Axis_LeftX },
		{ StrInit("lefty"), Axis_LeftY },
		
		{ StrInit("rightshoulder"), Button_RightShoulder },
		{ StrInit("rightstick"), Button_RightStick },
		{ StrInit("righttrigger"), Pressure_RightTrigger },
		{ StrInit("rightx"), Axis_RightX },
		{ StrInit("righty"), Axis_RightY },
		
		{ StrInit("start"), Button_Start },
		
		{ StrInit("x"), Button_X },
		{ StrInit("y"), Button_Y },
	};
	
	int32 left = 0;
	int32 right = ArrayLength(table);
	
	while (left < right)
	{
		int32 index = (left + right) / 2;
		int32 cmp = StringCompare(name, table[index].name);
		
		if (cmp < 0)
			right = index;
		else if (cmp > 0)
			left = index + 1;
		else
			return table[index].object;
	}
	
	return 0;
}

static bool
ParseEntry_(String line, Controller_* out_controller, String* out_platform)
{
	Controller_ con = { 0 };
	String platform = StrNull;
	
	uint8 const* const begin = line.data;
	uint8 const* const end = line.data + line.size;
	uint8 const* head = begin;
	
	if (line.size < 34 || begin[0] == '#')
		return false;
	
	//- GUID
	for (int32 i = 0; i < 32; ++i)
	{
		if (!(head[0] >= '0' && head[0] <= '9') && !(head[0] >= 'a' && head[0] <= 'f'))
			return false;
		
		con.guid[i] = *head++;
	}
	
	if (head >= end || *head++ != ',')
		return false;
	
	//- Name
	{
		uint8 const* name_begin = head;
		uint8 const* name_end = MemoryFindByte(head, ',', end - head);
		
		if (!name_end)
			return false;
		
		head = name_end;
		con.name = StrMake(name_end - name_begin, name_begin);
	}
	
	if (head >= end || *head++ != ',')
		return false;
	
	//- Mapping
	while (head < end)
	{
		// Left arg
		uint8 left_prefix = 0;
		if (head[0] == '+' || head[0] == '-')
			left_prefix = *head++;
		
		uint8 const* left_arg_begin = head;
		while (head < end && head[0] != ':')
			++head;
		uint8 const* left_arg_end = head;
		
		String left_arg = {
			.size = left_arg_end - left_arg_begin,
			.data = left_arg_begin
		};
		
		if (StringEquals(left_arg, Str("platform")))
		{
			if (head + 2 >= end || head[0] != ':')
				return false;
			
			uint8 const* platform_begin = ++head;
			while (head < end && (head[0] != '\n' || head[0] != ','))
				++head;
			
			platform = StrRange(platform_begin, head - 1);
			
			if (head >= end || *head++ != ',')
				break;
			
			continue;
		}
		
		Object_ object = FindObjectFromName_(left_arg);
		if (object == 0)
			return false;
		
		if (head + 2 >= end || head[0] != ':')
			return false;
		++head; // eat ':'
		
		// Right arg
		uint8 right_prefix = 0;
		if (head[0] == '+' || head[0] == '-')
			right_prefix = *head++;
		
		uint8 kind = *head++;
		
		switch (kind)
		{
			case 'b':
			{
				int32 num = 0;
				while (head < end && head[0] >= '0' && head[0] <= '9')
				{
					num *= 10;
					num += *head++ - '0';
				}
				
				if (num >= ArrayLength(con.buttons) || con.buttons[num] != 0)
					return false;
				
				con.buttons[num] = object;
			} break;
			
			case 'h':
			{
				if (head + 3 >= end)
					return false;
				
				int32 left = *head++ - '0';
				if (*head++ != '.')
					return false;
				uint32 right = *head++ - '0';
				
				int32 bit = BitCtz32(right);
				
				if (left >= ArrayLength(con.povs) || bit >= 4 || con.povs[left][bit] != 0)
					return false;
				
				con.povs[left][bit] = object;
			} break;
			
			case 'a':
			{
				uint8 higher_bits = 0;
				
				if (left_prefix == '+')
					higher_bits |= 1 << 4;
				else if (left_prefix == '-')
					higher_bits |= 1 << 3;
				
				if (right_prefix == '+')
					higher_bits |= 1 << 6;
				else if (right_prefix == '-')
					higher_bits |= 1 << 5;
				
				int32 num = 0;
				while (head < end && head[0] >= '0' && head[0] <= '9')
				{
					num *= 10;
					num += *head++ - '0';
				}
				
				if (head < end && head[0] == '~')
					++head, higher_bits |= 1 << 7;
				
				if (num > ArrayLength(con.axes) || con.axes[num] != 0)
					return false;
				
				con.axes[num] = (uint16)(object | (higher_bits << 8));
			} break;
			
			default: return false;
		}
		
		if (head >= end || *head++ != ',')
			break;
	}
	
	//- Done
	*out_controller = con;
	*out_platform = platform;
	return true;
}

//~ CLI
static int32
AsHexChar_(uint8 c)
{
	if (c >= 'a')
		return c - 'a' + 10;
	if (c >= 'A')
		return c - 'A' + 10;
	return c - '0';
}

struct Writing_
{
	char* buf;
	uintsize len;
	Arena* arena;
}
typedef Writing_;

static Writing_
BeginWriting_(Arena* arena)
{
	return (Writing_) {
		.buf = ArenaEnd(arena),
		.arena = arena,
		.len = 0,
	};
}

static void
Write_(Writing_* w, char const* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	
	String str = ArenaVPrintf(w->arena, fmt, args);
	w->len += str.size;
	
	va_end(args);
}

API int32
EntryPoint(int argc, char const* const* argv)
{
	OS_Init(0);
	if (argc < 3)
	{
		OS_LogOut("usage: %s path/to/gamecontrollerdb.txt path/to/output.h\n", argv[0]);
		return 1;
	}
	
	const char* input_fname = argv[1];
	const char* output_fname = argv[2];
	String input_name = StrMake(MemoryStrlen(input_fname), input_fname);
	String output_name = StrMake(MemoryStrlen(output_fname), output_fname);
	
	Arena arena = OS_VirtualAllocArena(32 << 20);
	String input_data;
	OS_Error err;
	if (!OS_ReadEntireFile(input_name, &arena, (void**)&input_data.data, &input_data.size, &err))
	{
		OS_LogErr("could not open input file (%u): %S\n", err.code, err.what);
		return 1;
	}
	
	static String const objects_table[] = {
		[Button_A] = StrInit("OS_GamepadMap_Button_A"),
		[Button_B] = StrInit("OS_GamepadMap_Button_B"),
		[Button_X] = StrInit("OS_GamepadMap_Button_X"),
		[Button_Y] = StrInit("OS_GamepadMap_Button_Y"),
		[Button_Left] = StrInit("OS_GamepadMap_Button_Left"),
		[Button_Right] = StrInit("OS_GamepadMap_Button_Right"),
		[Button_Up] = StrInit("OS_GamepadMap_Button_Up"),
		[Button_Down] = StrInit("OS_GamepadMap_Button_Down"),
		[Button_LeftShoulder] = StrInit("OS_GamepadMap_Button_LeftShoulder"),
		[Button_RightShoulder] = StrInit("OS_GamepadMap_Button_RightShoulder"),
		[Button_LeftStick] = StrInit("OS_GamepadMap_Button_LeftStick"),
		[Button_RightStick] = StrInit("OS_GamepadMap_Button_RightStick"),
		[Button_Start] = StrInit("OS_GamepadMap_Button_Start"),
		[Button_Back] = StrInit("OS_GamepadMap_Button_Back"),
		
		[Pressure_LeftTrigger] = StrInit("OS_GamepadMap_Pressure_LeftTrigger"),
		[Pressure_RightTrigger] = StrInit("OS_GamepadMap_Pressure_RightTrigger"),
		
		[Axis_LeftX] = StrInit("OS_GamepadMap_Axis_LeftX"),
		[Axis_LeftY] = StrInit("OS_GamepadMap_Axis_LeftY"),
		[Axis_RightX] = StrInit("OS_GamepadMap_Axis_RightX"),
		[Axis_RightY] = StrInit("OS_GamepadMap_Axis_RightY"),
	};
	
	Writing_* w = &(Writing_) { 0 };
	*w = BeginWriting_(&arena);
	
	Write_(w, "// This file was generated from \"%S\"\n\n", input_name);
	Write_(w, "static OS_GamepadMapping const g_gamepadmap_database[] = {\n");
	
	uint8 const* head = input_data.data;
	uint8 const* const end = input_data.data + input_data.size;
	
	while (head < end)
	{
		while (*head == '\n')
			++head;
		if (!*head)
			break;
		if (*head == '#')
		{
			uint8 const* found = MemoryFindByte(head, '\n', end - head);
			if (!found)
				break;
			
			head = found + 1;
			continue;
		}
		
		uint8 const* eol = MemoryFindByte(head, '\n', end - head);
		if (!eol)
			eol = end - 1;
		
		String line = {
			.size = eol - head,
			.data = head,
		};
		
		Controller_ con;
		String platform;
		if (ParseEntry_(line, &con, &platform) && StringEquals(platform, Str("Windows")))
		{
			Write_(w, "\t//%S\n\t{", con.name);
			
			// Print GUID
			Write_(w, ".guid={");
			for (int32 i = 0; i < ArrayLength(con.guid); ++i)
				Write_(w, "'%c',", con.guid[i]);
			Write_(w, "},");
			
			// Print buttons
			for (int32 i = 0; i < ArrayLength(con.buttons); ++i)
			{
				Object_ obj = con.buttons[i];
				
				if (obj != 0)
					Write_(w, ".buttons[%i]=%S,", i, objects_table[obj]);
			}
			
			// Print axes
			for (int32 i = 0; i < ArrayLength(con.axes); ++i)
			{
				Object_ obj = con.axes[i];
				
				if (obj != 0)
					Write_(w, ".axes[%i]=%S|%i,", i, objects_table[obj & 0xff], obj & 0xff00);
			}
			
			// Print povs
			for (int32 i = 0; i < ArrayLength(con.povs); ++i)
			{
				for (int32 k = 0; k < ArrayLength(con.povs[i]); ++k)
				{
					Object_ obj = con.povs[i][k];
					
					if (obj != 0)
						Write_(w, ".povs[%i][%i]=%S,", i, k, objects_table[obj]);
				}
			}
			
			Write_(w, "},\n");
		}
		
		head = eol + 1;
	}
	
	Write_(w, "};\n");
	
	if (!OS_WriteEntireFile(output_name, w->buf, w->len, &err))
	{
		OS_LogErr("could not write output file (%u): %S\n", err.code, err.what);
		return 1;
	}
	
	return 0;
}
