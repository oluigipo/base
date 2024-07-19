package common_odin

import "base:runtime"

vec2 :: [2]f32
vec3 :: [3]f32
vec4 :: [4]f32
mat2 :: matrix[2, 2]f32
mat3 :: matrix[3, 3]f32
mat4 :: matrix[4, 4]f32

Arena :: struct
{
	size: uintptr,
	offset: uintptr,
	memory: uintptr,
	reserved: uintptr,
	commit_memory_proc: proc "c"(arena: ^Arena, needed_size: uintptr) -> bool,
}

ArenaSavepoint :: struct
{
	arena: ^Arena,
	offset: uintptr,
}

ArenaSave :: proc "c"(arena: ^Arena) -> ArenaSavepoint
{
	return { arena, arena.offset }
}
ArenaRestore :: proc "c"(savepoint: ArenaSavepoint)
{
	savepoint.arena.offset = savepoint.offset
}

AlignUp :: proc "c"(x: $T, align: T) -> T
{
	return (x + align) & ~align
}

ArenaPushType :: proc "c"(arena: ^Arena, $T: typeid) -> ^T
{
	return auto_cast ArenaPushAligned(arena, size_of(T), align_of(T))
}
ArenaPushData :: proc "c"(arena: ^Arena, data: ^$T) -> ^T
{
	ptr := cast(^T) ArenaPushAligned(arena, size_of(T), align_of(T))
	if ptr != nil {
		ptr^ = data^
	}
	return ptr
}
ArenaPushArray :: proc "c"(arena: ^Arena, $T: typeid, count: int) -> []T
{
	ptr := cast([^]T) ArenaPushAligned(arena, size_of(T) * count, align_of(T))
	return ptr[:count]
}
ArenaPushArrayData :: proc "c"(arena: ^Arena, data: []$T) -> []T
{
	ptr := cast([^]T) ArenaPushAligned(arena, size_of(T) * len(data), align_of(T))
	slice := ptr[:len(data)]
	if ptr != nil {
		copy(slice, data)
	}
	return slice
}

ArenaFromMemory :: proc "c"(memory: rawptr, size: uint) -> Arena
{
	context = runtime.default_context()
	assert((cast(uintptr)memory & 15) == 0)
	return {
		size = cast(uintptr)size,
		offset = 0,
		memory = cast(uintptr)memory,
	}
}
ArenaEndAligned :: proc "c"(arena: ^Arena, alignment: uint) -> rawptr
{
	context = runtime.default_context()
	assert(alignment != 0 && ((alignment & alignment-1) == 0))
	target_offset := AlignUp(arena.memory + arena.offset, cast(uintptr)alignment-1) - arena.memory
	arena.offset = target_offset
	return auto_cast (arena.memory + arena.offset)
}
ArenaPushDirtyAligned :: proc "c"(arena: ^Arena, size: uint, alignment: uint) -> rawptr
{
	context = runtime.default_context()
	assert(alignment != 0 && ((alignment & alignment-1) == 0))
	assert(cast(int)size > 0)

	target_offset := AlignUp(arena.memory + arena.offset, cast(uintptr)alignment-1) - arena.memory
	needed := target_offset + cast(uintptr)size

	if needed > arena.size {
		if arena.commit_memory_proc == nil || !arena.commit_memory_proc(arena, needed) {
			return nil
		}
		assert(needed >= arena.size)
	}

	result := cast(rawptr)(arena.memory + arena.offset)
	arena.offset = needed

	return result
}
ArenaPushAligned :: proc "c"(arena: ^Arena, size: uint, alignment: uint) -> rawptr
{
	result := ArenaPushDirtyAligned(arena, size, alignment)
	if result != nil {
		runtime.mem_zero(result, cast(int)size)
	}
	return result
}
ArenaPushMemory :: proc "c"(arena: ^Arena, data: []u8) -> []u8
{
	return ArenaPushArrayData(arena, data)
}
ArenaPop :: proc "c"(arena: ^Arena, ptr: rawptr)
{
	context = runtime.default_context()
	p := cast(uintptr)ptr
	assert(p >= arena.memory && p < arena.memory + arena.offset)
	arena.offset = auto_cast (p - arena.memory)
}
ArenaPushString :: proc "c"(arena: ^Arena, str: string) -> string
{
	return string(ArenaPushMemory(arena, transmute([]u8)str))
}
ArenaPushCString :: proc "c"(arena: ^Arena, str: string) -> cstring
{
	ptr := ArenaPushDirtyAligned(arena, len(str) + 1, 1)
	if ptr != nil {
		runtime.mem_copy(ptr, raw_data(str), len(str))
		(cast([^]u8)ptr)[len(str)] = 0
	}
	return auto_cast ptr
}

@(private)
ArenaAllocatorProc_ :: proc(
	allocator_data: rawptr,
	mode: runtime.Allocator_Mode,
	size, alignment: int,
	old_memory: rawptr,
	old_size: int,
	location := #caller_location) -> ([]u8, runtime.Allocator_Error)
{
	result: []u8 = nil
	err := runtime.Allocator_Error.None
	arena := cast(^Arena) allocator_data
	#partial switch mode
	{
		case .Alloc, .Alloc_Non_Zeroed:
			ptr := cast([^]u8) ArenaPushAligned(arena, cast(uint) size, cast(uint) alignment)
			if ptr == nil {
				err = .Out_Of_Memory
			} else {
				result = ptr[:size]
			}
		case .Free:
			if cast(uintptr)old_memory + cast(uintptr)old_size != arena.memory + arena.offset {
				err = .Invalid_Pointer
			} else {
				ArenaPop(arena, old_memory)
			}
		case .Free_All:
			arena.offset = 0
		case:
			err = .Mode_Not_Implemented
	}
	return result, err
}

ArenaAsAllocator :: proc "c"(arena: ^Arena) -> runtime.Allocator
{
	return {
		procedure = ArenaAllocatorProc_,
		data = arena,
	}
}

@(private)
AllocatorAsCAllocatorWrapper_ :: proc "c"(
	allocator: ^CAllocator,
	mode: runtime.Allocator_Mode,
	size, alignment: int,
	old_ptr: rawptr,
	old_size: int,
	out_err: ^runtime.Allocator_Error) -> rawptr
{
	ptr, err := allocator.odin_proc(allocator.instance, mode, size, alignment, old_ptr, old_size)
	if out_err != nil {
		out_err^ = err
	}
	return raw_data(ptr)
}

@(private)
CAllocatorAsAllocatorWrapper_ :: proc(
	allocator_data: rawptr,
	mode: runtime.Allocator_Mode,
	size, alignment: int,
	old_memory: rawptr,
	old_size: int,
	location := #caller_location) -> ([]u8, runtime.Allocator_Error)
{
	callocator := cast(^CAllocator) allocator_data
	err: runtime.Allocator_Error
	ptr := callocator->procedure(mode, size, alignment, old_memory, old_size, &err)
	slice := (transmute([^]u8)ptr)[:size]
	return slice, err
}

ArenaAsCAllocator :: proc "c"(arena: ^Arena) -> CAllocator
{
	return {
		procedure = AllocatorAsCAllocatorWrapper_,
		instance = arena,
		odin_proc = ArenaAllocatorProc_,
	}
}

AllocatorAsCAllocator :: proc "c"(allocator: runtime.Allocator) -> CAllocator
{
	return {
		procedure = AllocatorAsCAllocatorWrapper_,
		instance = allocator.data,
		odin_proc = allocator.procedure,
	}
}

CAllocatorAsAllocator :: proc "c"(allocator: ^CAllocator) -> runtime.Allocator
{
	return {
		procedure = CAllocatorAsAllocatorWrapper_,
		data = allocator,
	}
}

CAllocator :: struct
{
	procedure: proc "c"(allocator: ^CAllocator, mode: runtime.Allocator_Mode, size, alignment: int, old_ptr: rawptr, old_size: int, out_err: ^runtime.Allocator_Error) -> rawptr,
	instance: rawptr,
	odin_proc: runtime.Allocator_Proc,
}
