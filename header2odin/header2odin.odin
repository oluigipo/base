// NOTE(ljre): does not depend on api_os.h (chicken egg problem)
package header2odin

import "base:runtime"
import fmt "core:fmt"
import os "core:os"
import c "core:c/frontend/tokenizer"
import strings "core:strings"
import mem "core:mem/virtual"

Decl :: struct
{
	name: string,
	initializer: string,
	bitfield_bit_count: string,
	is_api_decl: bool,
	is_typedef: bool,
	type: Type,
	next: ^Decl,
}

Type :: union
{
	PointerType,
	StructType,
	ProcType,
	PrimitiveType,
	ArrayType,
	EnumType,
	CustomType,
}

StructType :: struct
{
	name: string,
	is_union: bool,
	is_bitfield: bool,
	fields: []^Decl,
}

ProcType :: struct
{
	return_type: ^Type,
	parameters: []^Decl,
	is_variadic: bool,
}

PointerType :: struct
{
	is_const: bool,
	to: ^Type,
}

ArrayType :: struct
{
	length: string,
	of: ^Type,
}

EnumConst :: struct
{
	name: string,
	value: string,
}

EnumType :: struct
{
	name: string,
	underlying_type: PrimitiveType,
	constants: []EnumConst,
}

CustomType :: struct
{
	name: string,
}

PrimitiveType :: enum
{
	Void,
	Int8,
	Int16,
	Int32,
	Int64,
	UInt8,
	UInt16,
	UInt32,
	UInt64,
	Float32,
	Float64,
	Intsize,
	UIntsize,
	Intptr,
	UIntptr,
	Bool,
	String,
	Char,
}

Parser :: struct
{
	head: ^c.Token,
}

main :: proc()
{
	argv := os.args
	if len(argv) < 2 {
		fmt.printfln("usage: %v src/api_example.h", argv[0])
		return
	}
	
	arena := &mem.Arena {}
	if err := mem.arena_init_static(arena, 32<<30, 32<<20); err != .None {
		return
	}
	context.allocator = mem.arena_allocator(arena)

	tokenizer := &c.Tokenizer {}
	c.init_defaults(tokenizer)

	tokens := c.tokenize_file(tokenizer, argv[1], 0)
	parser := &Parser { tokens }
	decls: ^Decl = nil
	pdecl := &decls
	for parser.head != nil && parser.head.kind != .EOF {
		if parser.head.lit[0] == '#' {
			skip_until_next_line(&parser.head)
			continue
		}

		// for i in prev_head.pos.line..<head.pos.line {
		// 	fmt.println()
		// }
		// fmt.printf("%v%v", head.prefix, head.lit)

		if runtime.string_eq(parser.head.lit, "static_assert") {
			advance(parser)
			eat(parser, "(")
			nesting := 1
			for nesting > 0 && parser.head != nil {
				if runtime.string_eq(parser.head.lit, "(") {
					nesting += 1
				} else if runtime.string_eq(parser.head.lit, ")") {
					nesting -= 1
				}
				advance(parser)
			}
			eat(parser, ";")
		} else {
			decl := parse_decl(parser, true)
			eat(parser, ";")

			pdecl^ = decl
			it := decl
			for ; it.next != nil; it = it.next {}
			pdecl = &it.next
		}
	}

	builder, err := strings.builder_make()
	if err != .None {
		return
	}
	fmt.sbprint(&builder, "package capi\n\n")
	write_decls(decls, &builder)

	output := strings.to_string(builder)
	if len(argv) >= 3 {
		os.write_entire_file(argv[2], transmute([]u8)output)
	} else {
		fmt.println(output)
	}
	mem.arena_destroy(arena)
}

skip_until_next_line :: proc(head: ^^c.Token)
{
	if head^ != nil {
		start_pos := head^.pos
		head^ = head^.next
		for head^ != nil && start_pos.line == head^.pos.line {
			head^ = head^.next
		}
	}
}

parse_decl :: proc(parser: ^Parser, allow_multiple: bool) -> ^Decl
{
	decl := new(Decl)

	if runtime.string_eq(parser.head.lit, "API") {
		decl.is_api_decl = true
		advance(parser)
	}

	// parse declarator
	base_has_const := false
	declarator_loop: for {
		should_eat_token := true
		switch parser.head.lit {
			case "void": decl.type = PrimitiveType.Void
			case "char": decl.type = PrimitiveType.Char
			case "bool": decl.type = PrimitiveType.Bool
			case "String": decl.type = PrimitiveType.String
			case "intsize": decl.type = PrimitiveType.Intsize
			case "uintsize": decl.type = PrimitiveType.UIntsize
			case "intptr": decl.type = PrimitiveType.Intptr
			case "uintptr": decl.type = PrimitiveType.UIntptr
			case "int8": decl.type = PrimitiveType.Int8
			case "int16": decl.type = PrimitiveType.Int16
			case "int32", "int": decl.type = PrimitiveType.Int32
			case "int64": decl.type = PrimitiveType.Int64
			case "uint8": decl.type = PrimitiveType.UInt8
			case "uint16": decl.type = PrimitiveType.UInt16
			case "uint32": decl.type = PrimitiveType.UInt32
			case "uint64": decl.type = PrimitiveType.UInt64
			case "float32", "float": decl.type = PrimitiveType.Float32
			case "float64", "double": decl.type = PrimitiveType.Float64
			case "struct", "union":
				is_union := runtime.string_eq(parser.head.lit, "union")
				advance(parser)
				name := ""
				if parser.head.kind == .Ident {
					name = parser.head.lit
					advance(parser)
				}
				fields: []^Decl = {}
				is_bitfield := false
				if runtime.string_eq(parser.head.lit, "{") {
					eat(parser, "{")
					fields_list := make([dynamic]^Decl)
					for !runtime.string_eq(parser.head.lit, "}") {
						field_decl := parse_decl(parser, true)
						if tryeat(parser, ":") {
							is_bitfield = true
							field_decl.bitfield_bit_count = parser.head.lit
							advance(parser)
						}
						eat(parser, ";")
						append_elem(&fields_list, field_decl)
					}
					eat(parser, "}")
					fields = fields_list[:]
				}
				decl.type = StructType {
					name = name,
					fields = fields,
					is_union = is_union,
					is_bitfield = is_bitfield,
				}
				should_eat_token = false
			case "enum":
				advance(parser)
				name := ""
				if parser.head.kind == .Ident {
					name = parser.head.lit
					advance(parser)
				}
				underlying_type := PrimitiveType.Int32
				if runtime.string_eq(parser.head.lit, ":") {
					advance(parser)
					// TODO
					advance(parser)
				}
				members := make([dynamic]EnumConst)
				if runtime.string_eq(parser.head.lit, "{") {
					advance(parser)
					for !runtime.string_eq(parser.head.lit, "}") {
						name := parser.head.lit
						advance(parser)
						value := ""
						if runtime.string_eq(parser.head.lit, "=") {
							advance(parser)
							value = parser.head.lit
							advance(parser)
						}
						append(&members, EnumConst {name = name, value = value})
						if !runtime.string_eq(parser.head.lit, ",") {
							break
						}
						eat(parser, ",")
					}
					eat(parser, "}")
				}
				decl.type = EnumType {
					name = name,
					underlying_type = underlying_type,
					constants = members[:],
				}
				should_eat_token = false
			case "typedef":
				decl.is_typedef = true
			case "const":
				base_has_const = true
			case:
				if parser.head.kind != .Ident || decl.type != nil {
					break declarator_loop
				}
				decl.type = CustomType {
					name = parser.head.lit
				}
		}
		if should_eat_token {
			advance(parser)
		}
	}

	parse_operand :: proc(parser: ^Parser, decl: ^Decl, out_type: ^Type, has_const: ^bool)
	{
		out_type := out_type
		// parse prefix
		for {
			if runtime.string_eq(parser.head.lit, "*") {
				to_type := new(Type)
				to_type^ = out_type^
				out_type^ = PointerType {
					is_const = has_const^,
					to = to_type
				}
				has_const^ = false
			} else if runtime.string_eq(parser.head.lit, "const") {
				has_const^ = true
			} else if runtime.string_eq(parser.head.lit, "restrict") {
			} else {
				break
			}
			advance(parser)
		}

		// parse name or parens
		if parser.head.kind == .Ident {
			decl.name = parser.head.lit
			advance(parser)
		} else if runtime.string_eq(parser.head.lit, "(") {
			advance(parser)
			parse_operand(parser, decl, out_type, has_const)
			eat(parser, ")")
		}

		// parse suffix
		for {
			if runtime.string_eq(parser.head.lit, "(") {
				advance(parser)
				params := make([dynamic]^Decl)
				is_variadic := false
				for !runtime.string_eq(parser.head.lit, ")") {
					if runtime.string_eq(parser.head.lit, "...") {
						is_variadic = true
						advance(parser)
					} else {
						param_decl := parse_decl(parser, false)
						if len(param_decl.name) == 0 {
							prim, ok := param_decl.type.(PrimitiveType)
							if ok && prim == .Void {
								break // found 'Proc(void)' pattern'
							}
						}
						append(&params, param_decl)
					}
					if !runtime.string_eq(parser.head.lit, ",") {
						break
					}
					eat(parser, ",")
				}
				eat(parser, ")")

				ret_type := new(Type)
				ret_type^ = out_type^
				out_type^ = ProcType {
					return_type = ret_type,
					parameters = params[:],
					is_variadic = is_variadic,
				}
				out_type = ret_type
			} else if runtime.string_eq(parser.head.lit, "[") {
				advance(parser)
				length_expr := ""
				if !runtime.string_eq(parser.head.lit, "]") {
					length_expr = parser.head.lit
					advance(parser)
				}
				eat(parser, "]")

				of := new(Type)
				of^ = out_type^
				out_type^ = ArrayType {
					length = length_expr,
					of = of,
				}
				out_type = of
			} else {
				break
			}
		}
	}
		
	has_const := base_has_const
	parse_operand(parser, decl, &decl.type, &has_const)
	
	if allow_multiple {
		tail := &decl.next
		for {
			if !runtime.string_eq(parser.head.lit, ",") {
				break
			}
			advance(parser)
			new_decl := new_clone(decl^)
			tail^ = new_decl
			tail = &new_decl.next
			has_const := base_has_const
			parse_operand(parser, new_decl, &new_decl.type, &has_const)
		}
	}

	return decl
}

advance :: proc(parser: ^Parser)
{
	if parser.head != nil && parser.head.kind != .EOF {
		parser.head = parser.head.next
	}
}

eat :: proc(parser: ^Parser, lit: string, loc := #caller_location)
{
	assert(runtime.string_eq(parser.head.lit, lit), fmt.tprintf("\nexpected '%v', but got '%v' at %v\nloc: %v", lit, parser.head.lit, parser.head.pos, loc))
	advance(parser)
}

tryeat :: proc(parser: ^Parser, lit: string) -> bool
{
	if runtime.string_eq(parser.head.lit, lit) {
		advance(parser)
		return true
	}
	return false
}

write_decls :: proc(decls: ^Decl, out: ^strings.Builder)
{
	has_api_decls := false
	api_prefix := ""

	for decl := decls; decl != nil; decl = decl.next {
		if !decl.is_api_decl {
			name := decl.name
			switch type in decl.type {
				case EnumType:
					if len(type.name) == 0 {
						// this enum defines constants
						for member in type.constants {
							fmt.sbprintf(out, "%v :: %v\n", member.name, member.value)
						}
						continue
					} else {
						fmt.sbprintf(out, "%v :: ", sanitize_ident(name))
						write_type(out, decl.type, 0)
						fmt.sbprint(out, "\n")
					}
				case ProcType:
					fmt.sbprintf(out, "%v :: ", sanitize_ident(name))
					if decl.is_typedef {
						fmt.sbprint(out, "#type ")
					}
					write_type(out, decl.type, 0)
					fmt.sbprint(out, "\n")
				case PointerType, ArrayType, PrimitiveType, StructType, CustomType:
					fmt.sbprintf(out, "%v :: ", sanitize_ident(name))
					write_type(out, decl.type, 0)
					fmt.sbprint(out, "\n")
			}
		} else if !has_api_decls {
			prefix_start := strings.index_rune(decl.name, '_')
			if prefix_start != -1 {
				api_prefix = decl.name[:prefix_start]
				has_api_decls = true
			}
		}
	}

	if has_api_decls {
		fmt.sbprintf(out, "@(default_calling_convention=\"c\")\nforeign {{\n")
		for decl := decls; decl != nil; decl = decl.next {
			if !decl.is_api_decl || !strings.has_prefix(decl.name, api_prefix) {
				continue
			}
			#partial switch v in decl.type {
				case ProcType:
					fmt.sbprintf(out, "\t%v :: ", sanitize_ident(decl.name))
					write_proc_type(out, v, 1)
					fmt.sbprintf(out, " ---;\n")
			}
		}
		fmt.sbprintf(out, "}\n")
	}

	write_proc_type :: proc(out: ^strings.Builder, type: ProcType, indent_level: int)
	{
		fmt.sbprintf(out, "proc \"c\"(")
		first := true
		for param in type.parameters {
			if !first {
				fmt.sbprintf(out, ", ")
			}
			first = false
			param_type := param.type
			if is_char_pointer(param_type) {
				param_type = CustomType {name = "cstring"}
			} else if is_const_pointer(param_type) {
				fmt.sbprintf(out, "#by_ptr ")
				param_type = param_type.(PointerType).to^
			}
			fmt.sbprintf(out, "%v: ", sanitize_ident(param.name))
			write_type(out, param_type, indent_level, true)
		}
		if (type.is_variadic) {
			if len(type.parameters) > 0 {
				fmt.sbprint(out, ", ")
			}
			fmt.sbprintf(out, "#c_vararg varargs: ..any")
		}
		fmt.sbprintf(out, ")")
		prim, ok := type.return_type.(PrimitiveType)
		if !ok || prim != .Void {
			fmt.sbprintf(out, " -> ")
			write_type(out, type.return_type^, indent_level)
		}
	}

	is_const_pointer :: proc(type: Type) -> bool
	{
		#partial switch t in type {
			case PointerType:
				if t.is_const {
					#partial switch t2 in t.to {
						case PrimitiveType:
							return false
					}
					return true
				}
		}
		return false
	}

	is_void_pointer :: proc(type: Type) -> bool
	{
		#partial switch t in type {
			case PointerType:
				#partial switch t2 in t.to {
					case PrimitiveType:
						if t2 == .Void {
							return true
						}
				}
		}
		return false
	}

	is_char_pointer :: proc(type: Type) -> bool
	{
		#partial switch t in type {
			case PointerType:
				#partial switch t2 in t.to {
					case PrimitiveType:
						if t2 == .Char {
							return true
						}
				}
		}
		return false
	}

	write_type :: proc(out: ^strings.Builder, type: Type, indent_level: int, in_proc_param := false)
	{
		switch t in type {
			case PrimitiveType:
				name := ""
				switch t {
					case .Void: name = "void"
					case .Char: name = "char"
					case .Int8: name = "i8"
					case .Int16: name = "i16"
					case .Int32: name = "i32"
					case .Int64: name = "i64"
					case .UInt8: name = "u8"
					case .UInt16: name = "u16"
					case .UInt32: name = "u32"
					case .UInt64: name = "u64"
					case .Float32: name = "f32"
					case .Float64: name = "f64"
					case .Intsize: name = "int"
					case .UIntsize: name = "uint"
					case .Intptr: name = "intptr"
					case .UIntptr: name = "uintptr"
					case .Bool: name = "bool"
					case .String: name = "string"
					case: name = "(unknown)"
				}
				fmt.sbprintf(out, "%v", name)
			case PointerType:
				if (is_void_pointer(t)) {
					fmt.sbprintf(out, "rawptr")
				} else {
					fmt.sbprintf(out, "^")
					write_type(out, t.to^, indent_level)
				}
			case ArrayType:
				if in_proc_param {
					fmt.sbprintf(out, "[^]")
				} else {
					fmt.sbprintf(out, "[%v]", t.length)
				}
				write_type(out, t.of^, indent_level)
			case ProcType:
				write_proc_type(out, t, indent_level)
			case StructType:
				bitfield_type := "u8" // TODO: PROPER BITFIELD TYPE
				if t.is_bitfield {
					fmt.sbprintf(out, "bit_field %v\n", bitfield_type)
				} else if t.is_union {
					fmt.sbprintf(out, "struct #raw_union\n");
				} else {
					fmt.sbprintf(out, "struct\n");
				}
				write_indentation(out, indent_level)
				indent_level := indent_level+1
				fmt.sbprint(out, "{\n")
				for decl in t.fields {
					for it := decl; it != nil; it = it.next {
						write_indentation(out, indent_level)
						if len(it.name) > 0 {
							fmt.sbprintf(out, "%v: ", sanitize_ident(it.name))
						} else {
							fmt.sbprint(out, "using _: ")
						}
						write_type(out, it.type, indent_level)
						if t.is_bitfield {
							fmt.sbprintf(out, " | %v", it.bitfield_bit_count)
						}
						fmt.sbprintf(out, ",\n")
					}
				}
				indent_level -= 1
				write_indentation(out, indent_level)
				fmt.sbprintf(out, "}")
			case EnumType:
				fmt.sbprintf(out, "enum ")
				write_type(out, t.underlying_type, indent_level)
				write_indentation(out, indent_level)
				indent_level := indent_level+1
				fmt.sbprint(out, "{\n")
				for member in t.constants {
					write_indentation(out, indent_level)
					name := member.name
					if strings.has_prefix(name, t.name) {
						name = name[len(t.name):]
						if !(name[1] >= '0' && name[1] <= '9') {
							name = name[1:]
						}
					}
					if len(member.value) > 0 {
						fmt.sbprintf(out, "%v = %v,\n", sanitize_ident(name), member.value)
					} else {
						fmt.sbprintf(out, "%v,\n", sanitize_ident(name))
					}
				}
				indent_level -= 1
				write_indentation(out, indent_level)
				fmt.sbprintf(out, "}")
			case CustomType:
				fmt.sbprint(out, t.name)
		}
	}

	write_indentation :: proc(out: ^strings.Builder, indent_level: int)
	{
		for i in 0..<indent_level {
			fmt.sbprint(out, "\t")
		}
	}

	sanitize_ident :: proc(ident: string) -> string
	{
		switch ident {
			case "proc": return "proc_"
			case: return ident
		}
	}
}
