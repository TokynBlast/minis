#!/usr/bin/env python3
"""
MASM - Minis ASM Assembler
Outputs raw bytecode for the VM
"""
import sys
import shlex
import struct

# Generate opcode: register << 5 | op
def g(reg, op):
    return (reg << 5) | op

# Opcodes matching bytecode.hpp
OPCODES = {
    # Import (0)
    'func':     g(0, 0),
    'load':     g(0, 1),
    'plugin':   g(0, 2),

    # Variable (1)
    'get':      g(1, 0),
    'set':      g(1, 1),
    'dec':      g(1, 2),
    'unset':    g(1, 3),
    'push':     g(1, 4),

    # Logic (2)
    'eq':       g(2, 0),
    'neq':      g(2, 1),
    'le':       g(2, 2),
    'lt':       g(2, 3),
    'and':      g(2, 4),
    'or':       g(2, 5),
    'jmp':      g(2, 6),
    'jmpif':    g(2, 7),
    'jmpifn':   g(2, 8),
    'not':      g(2, 9),
    'xor':      g(2, 10),

    # Function (3)
    'call':     g(3, 0),
    'tail':     g(3, 1),
    'ret':      g(3, 2),
    'builtin':  g(3, 3),
    'lambda':   g(3, 4),

    # General (4)
    'halt':     g(4, 0),
    'nop':      g(4, 1),
    'pop':      g(4, 2),
    'index':    g(4, 3),
    'yield':    g(4, 4),
    'dup':      g(4, 5),
    'swap':     g(4, 6),
    'rot':      g(4, 7),
    'over':     g(4, 8),
    'assert':   g(4, 9),

    # Math (5)
    'add':      g(5, 0),
    'sub':      g(5, 1),
    'mult':     g(5, 2),
    'div':      g(5, 3),
    'addm':     g(5, 4),
    'divm':     g(5, 5),
    'multm':    g(5, 6),
    'subm':     g(5, 7),
    'mod':      g(5, 8),
    'pow':      g(5, 9),

    # Stack (6)
    'i8':       g(6, 0),
    'i16':      g(6, 1),
    'i32':      g(6, 2),
    'i64':      g(6, 3),
    'ui8':      g(6, 4),
    'ui16':     g(6, 5),
    'ui32':     g(6, 6),
    'ui64':     g(6, 7),
    'f32':      g(6, 8),
    'f64':      g(6, 9),
    'str':      g(6, 10),
    'bool':     g(6, 11),
    'null':     g(6, 12),
    'list':     g(6, 13),
    'dict':     g(6, 14),

    # Bitwise (7)
    'band':     g(7, 0),
    'bor':      g(7, 1),
    'bxor':     g(7, 2),
    'bnot':     g(7, 3),
    'shl':      g(7, 4),
    'shr':      g(7, 5),
    'rol':      g(7, 6),
    'ror':      g(7, 7),
}


# Type sizes for explicit typed push instructions
TYPE_SIZES = {
    'i8':   1,
    'i16':  2,
    'i32':  4,
    'i64':  8,
    'ui8':  1,
    'ui16': 2,
    'ui32': 4,
    'ui64': 8,
    'f32':  4,
    'f64':  8,
}

TYPE_FORMATS = {
    'i8':   '<b',
    'i16':  '<h',
    'i32':  '<i',
    'i64':  '<q',
    'ui8':  '<B',
    'ui16': '<H',
    'ui32': '<I',
    'ui64': '<Q',
    'f32':  '<f',
    'f64':  '<d',
}


# All types for typed set/get
ALL_TYPES = set(TYPE_SIZES.keys()) | {'str', 'bool', 'null', 'list', 'dict'}


def parse_typed_value(typ, raw):
    """Parse a value with explicit type. Returns bytes."""
    raw = raw.strip()

    # Boolean
    if typ == 'bool':
        if raw.lower() == 'true':
            return bytes([1])
        elif raw.lower() == 'false':
            return bytes([0])
        else:
            return bytes([1 if int(raw) else 0])

    # Null
    if typ == 'null':
        return bytes()

    # String
    if typ == 'str':
        if raw.startswith('"') and raw.endswith('"'):
            raw = raw[1:-1]
        # Decode escape sequences
        def decode_escapes(s):
            out = []
            i = 0
            while i < len(s):
                if s[i] == '\\' and i + 1 < len(s):
                    c = s[i+1]
                    if c == 'n':
                        out.append('\n')
                        i += 2
                    elif c == 't':
                        out.append('\t')
                        i += 2
                    elif c == 'r':
                        out.append('\r')
                        i += 2
                    elif c == '"':
                        out.append('"')
                        i += 2
                    elif c == '*':
                        out.append('*')
                        i += 2
                    elif c == '\\':
                        out.append('\\')
                        i += 2
                    else:
                        out.append(c)
                        i += 2
                else:
                    out.append(s[i])
                    i += 1
            return ''.join(out)
        s = decode_escapes(raw).encode('utf-8')
        return struct.pack('<Q', len(s)) + s  # 64-bit length prefix

    # List/Dict (just marker, no data)
    if typ in ('list', 'dict'):
        return bytes()

    # Numeric types
    if typ in TYPE_FORMATS:
        fmt = TYPE_FORMATS[typ]
        if typ.startswith('f'):
            val = float(raw)
        elif raw.startswith('0x') or raw.startswith('-0x'):
            val = int(raw, 16)
        else:
            val = int(raw)
        # Let it overflow/wrap naturally - user's responsibility
        return struct.pack(fmt, val & ((1 << (TYPE_SIZES[typ] * 8)) - 1) if typ.startswith('u') else val)

    raise ValueError(f"Unknown type: {typ}")


def encode_string(s):
    """Encode a string with 64-bit length prefix"""
    data = s.encode('utf-8')
    return struct.pack('<Q', len(data)) + data  # 64-bit length prefix


def strip_comment(line):
    """Strip comment from line, respecting quoted strings."""
    in_quote = False
    for i, c in enumerate(line):
        if c == '"' and (i == 0 or line[i-1] != '\\'):
            in_quote = not in_quote
        elif c == ';' and not in_quote:
            return line[:i].strip()
    return line.strip()


def assemble(source):
    """
    Assemble MASM source into bytecode.
    Returns bytes with proper header.
    """
    output = bytearray()
    labels = {}
    fixups = []
    sections = {}
    current_section = None
    functions = []  # Track function labels

    lines = source.splitlines()

    # First pass: collect labels and sections, calculate offsets
    # Reserve 40 bytes for header (8 magic + 8 table_off + 8 fnCount + 8 entry_main + 8 reserved)
    HEADER_SIZE = 40
    byte_offset = HEADER_SIZE
    entry_point = HEADER_SIZE  # Default entry point is right after header

    for line in lines:
        line = strip_comment(line)
        if not line:
            continue
        print(line)

        # Section
        if line.startswith('.'):
            current_section = line[1:].strip()
            sections[current_section] = byte_offset
            continue

        # Label
        if line.endswith(':'):
            label = line[:-1].strip()
            labels[label] = byte_offset
            functions.append(label)  # Track as function
            if label == 'main':
                entry_point = byte_offset
            continue

        # Parse instruction
        tokens = shlex.split(line)
        if not tokens:
            continue

        op = tokens[0].lower()
        args = tokens[1:]

        # Calculate size
        # Smart set: set i32 varname 42  (type + varname + value)
        if op == 'set' and len(args) >= 2 and args[0].lower() in ALL_TYPES:
            typ = args[0].lower()
            varname = args[1]
            # size = push format + set opcode + varname
            if typ in TYPE_SIZES:
                byte_offset += 1 + 1 + 1 + TYPE_SIZES[typ]  # opcode + typeByte + meta + data
            elif typ == 'str':
                val = args[2] if len(args) > 2 else '""'
                if val.startswith('"') and val.endswith('"'):
                    val = val[1:-1]
                byte_offset += 1 + 1 + 8 + len(val.encode('utf-8'))  # opcode + typeByte + 8-byte length prefix + data
            elif typ == 'bool':
                byte_offset += 1 + 1 + 1  # opcode + typeByte + meta (bool value in lower nibble)
            elif typ == 'list':
                byte_offset += 1 + 1 + 8  # opcode + typeByte + count
            elif typ == 'dict':
                byte_offset += 1 + 1 + 8  # opcode + typeByte + count
            else:  # null
                byte_offset += 1 + 1 + 1  # opcode + typeByte + meta (VM doesn't read data for null)
            byte_offset += 1 + 8 + len(varname.encode('utf-8'))  # set opcode + 8-byte length prefix + varname

        # Smart get: get i32 varname  (type hint for casting)
        elif op == 'get' and len(args) >= 2 and args[0].lower() in ALL_TYPES:
            varname = args[1]
            byte_offset += 1 + 8 + len(varname.encode('utf-8'))  # 8-byte length prefix

        # Typed push matching VM format:
        # Format: PUSH opcode(0x24) + typeByte + data
        # Numeric/bool: 0x24 + 0x00 + meta(1) + data
        # String: 0x24 + 0x30 + string(8+len)
        # List: 0x24 + 0x40 + count(8)
        elif op in TYPE_SIZES:
            if args:
                byte_offset += 1 + 1 + 1 + TYPE_SIZES[op]  # opcode + typeByte + meta + data
            else:
                byte_offset += 1 + 1 + 1  # opcode + typeByte + meta (no data)

        elif op == 'str':
            val = args[0] if args else '""'
            if val.startswith('"') and val.endswith('"'):
                val = val[1:-1]
            byte_offset += 1 + 1 + 8 + len(val.encode('utf-8'))  # opcode + typeByte + 8-byte length prefix + data

        elif op == 'bool':
            byte_offset += 1 + 1 + 1  # opcode + typeByte + meta (bool type 9 + value in lower nibble)

        elif op in ('list', 'dict'):
            byte_offset += 1 + 1 + 8  # opcode + typeByte + count

        elif op == 'null':
            byte_offset += 1 + 1 + 1  # opcode + typeByte + meta (VM doesn't read data for null)

        elif op in ('get', 'set', 'unset', 'dec'):
            byte_offset += 1 + 8 + len(args[0].encode('utf-8'))  # 8-byte length prefix

        elif op in ('jmp', 'jmpif', 'jmpifn'):
            byte_offset += 1 + 8  # opcode + 64-bit target

        elif op == 'call':
            name = args[0]
            if name.startswith('"') and name.endswith('"'):
                name = name[1:-1]
            byte_offset += 1 + 8 + len(name.encode('utf-8')) + 8  # opcode + 8-byte length prefix + name + argc(8)

        # Handle generic push with type inference
        # Note: shlex.split strips quotes, so we check the raw line
        # Format: PUSH opcode(0x24) + typeByte + data
        elif op == 'push' and args:
            val = args[0]
            # Check if original line has a quoted string after 'push'
            raw_after_push = line.split(None, 1)[1] if len(line.split(None, 1)) > 1 else ''
            is_string = raw_after_push.strip().startswith('"')

            if is_string:
                # String: opcode + typeByte + 8-byte len + data
                byte_offset += 1 + 1 + 8 + len(val.encode('utf-8'))
            elif val.lower() in ('true', 'false'):
                # Bool: opcode + typeByte + meta (value in lower nibble)
                byte_offset += 1 + 1 + 1
            elif val == '[]':
                # Empty list: opcode + typeByte + count
                byte_offset += 1 + 1 + 8
            elif '.' in val and val.replace('.', '', 1).replace('-', '', 1).isdigit():
                # Float (f64): opcode + typeByte + meta + 8 bytes
                byte_offset += 1 + 1 + 1 + 8
            elif val.lstrip('-').replace('0x', '', 1).replace('0X', '', 1).isalnum() and (val.lstrip('-').isdigit() or val.lstrip('-').startswith('0x')):
                # Integer (i64): opcode + typeByte + meta + 8 bytes
                byte_offset += 1 + 1 + 1 + 8
            else:
                # Treat as unquoted string (convenience)
                byte_offset += 1 + 1 + 8 + len(val.encode('utf-8'))

        elif op in OPCODES:
            byte_offset += 1

        else:
            print(f"Warning: unknown opcode '{op}'")
            byte_offset += 1

    # Second pass: emit bytecode
    # Start with header placeholder - we'll fill it in at the end
    output.extend(b'\x00' * HEADER_SIZE)

    current_section = None
    for line in lines:
        line = strip_comment(line)
        if not line:
            continue

        if line.startswith('.'):
            current_section = line[1:].strip()
            continue

        if line.endswith(':'):
            continue

        try:
          tokens = shlex.split(line)
        except:
          print(line)
        if not tokens:
            continue

        op = tokens[0].lower()
        args = tokens[1:]

        # Emit
        # Smart set: set i32 varname 42
        if op == 'set' and len(args) >= 2 and args[0].lower() in ALL_TYPES:
            typ = args[0].lower()
            varname = args[1]
            val = args[2] if len(args) > 2 else ('0' if typ in TYPE_SIZES else ('[]' if typ == 'list' else 'null'))
            # Emit: push typed value using PUSH format, then set
            META_TYPES = {'i8': 0, 'i16': 1, 'i32': 2, 'i64': 3, 'ui8': 4, 'ui16': 5, 'ui32': 6, 'ui64': 7, 'f32': 8, 'f64': 8, 'bool': 9}
            if typ == 'list':
                output.append(OPCODES['push'])  # 0x24
                output.append(0x40)  # List type byte
                # Parse count from val like "[]" or a number
                if val == '[]':
                    count = 0
                else:
                    count = int(val)
                output.extend(struct.pack('<Q', count))
            elif typ == 'dict':
                output.append(OPCODES['push'])  # 0x24
                output.append(0x50)  # Dict type byte
                output.extend(struct.pack('<Q', 0))
            elif typ == 'null':
                output.append(OPCODES['push'])  # 0x24
                output.append(0x00)  # Numeric type byte
                output.append(0x0A << 4)  # null type in meta (10)
            elif typ == 'str':
                output.append(OPCODES['push'])  # 0x24
                output.append(0x30)  # String type byte
                output.extend(parse_typed_value('str', val))
            elif typ == 'bool':
                output.append(OPCODES['push'])  # 0x24
                output.append(0x00)  # Numeric type byte
                bool_val = 1 if val.lower() == 'true' else 0
                output.append((0x09 << 4) | bool_val)  # bool type (9)
            elif typ in META_TYPES:
                output.append(OPCODES['push'])  # 0x24
                output.append(0x00)  # Numeric type byte
                output.append(META_TYPES[typ] << 4)  # Type in upper 4 bits of meta
                output.extend(parse_typed_value(typ, val))
            output.append(OPCODES['set'])
            output.extend(encode_string(varname))

        # Smart get: get i32 varname (type hint ignored for now, just get)
        elif op == 'get' and len(args) >= 2 and args[0].lower() in ALL_TYPES:
            varname = args[1]
            output.append(OPCODES['get'])
            output.extend(encode_string(varname))

        # Typed push: i8 42, i32 1000, str "hello", etc.
        # Format: PUSH opcode(0x24) + typeByte + data
        elif op in TYPE_SIZES or op in ('str', 'bool', 'null', 'list', 'dict'):
            META_TYPES = {'i8': 0, 'i16': 1, 'i32': 2, 'i64': 3, 'ui8': 4, 'ui16': 5, 'ui32': 6, 'ui64': 7, 'f32': 8, 'f64': 8, 'bool': 9}
            if op == 'str':
                output.append(OPCODES['push'])  # 0x24
                output.append(0x30)  # String type byte
                output.extend(parse_typed_value('str', args[0] if args else '""'))
            elif op == 'list':
                output.append(OPCODES['push'])  # 0x24
                output.append(0x40)  # List type byte
                count = int(args[0]) if args else 0
                output.extend(struct.pack('<Q', count))
            elif op == 'dict':
                output.append(OPCODES['push'])  # 0x24
                output.append(0x50)  # Dict type byte
                output.extend(struct.pack('<Q', 0))
            elif op == 'null':
                output.append(OPCODES['push'])  # 0x24
                output.append(0x00)  # Numeric type byte
                output.append(0x0A << 4)  # null type in meta (10)
            elif op in META_TYPES:
                output.append(OPCODES['push'])  # 0x24
                output.append(0x00)  # Numeric type byte
                if op == 'bool':
                    # Compact: encode bool value in lower nibble of meta
                    bool_val = 1 if (args and args[0].lower() == 'true') else 0
                    output.append((0x09 << 4) | bool_val)  # bool type (9)
                else:
                    output.append(META_TYPES[op] << 4)  # Type in upper 4 bits of meta
                    if args:
                        output.extend(parse_typed_value(op, args[0]))

        # Generic push with type inference
        # Note: shlex.split strips quotes, so we check the raw line
        # Format: PUSH opcode(0x24) + typeByte + data
        elif op == 'push' and args:
            val = args[0]
            # Check if original line has a quoted string after 'push'
            raw_after_push = line.split(None, 1)[1] if len(line.split(None, 1)) > 1 else ''
            is_string = raw_after_push.strip().startswith('"')

            if is_string:
                # String: opcode + typeByte + string data
                output.append(OPCODES['push'])  # 0x24
                output.append(0x30)  # String type byte
                data = val.encode('utf-8')
                output.extend(struct.pack('<Q', len(data)))
                output.extend(data)
            elif val.lower() == 'true':
                output.append(OPCODES['push'])  # 0x24
                output.append(0x00)  # Numeric type byte
                output.append((0x09 << 4) | 1)  # bool type (9) + value in lower nibble
            elif val.lower() == 'false':
                output.append(OPCODES['push'])  # 0x24
                output.append(0x00)  # Numeric type byte
                output.append((0x09 << 4) | 0)  # bool type (9) + value in lower nibble
            elif val == '[]':
                output.append(OPCODES['push'])  # 0x24
                output.append(0x40)  # List type byte
                output.extend(struct.pack('<Q', 0))
            elif '.' in val and val.replace('.', '', 1).replace('-', '', 1).isdigit():
                # Float (f64)
                output.append(OPCODES['push'])  # 0x24
                output.append(0x00)  # Numeric type byte
                output.append(0x08 << 4)  # f64 type in meta (8)
                output.extend(struct.pack('<d', float(val)))
            elif val.lstrip('-').replace('0x', '', 1).replace('0X', '', 1).isalnum() and (val.lstrip('-').isdigit() or val.lstrip('-').startswith('0x')):
                # Integer (i64)
                output.append(OPCODES['push'])  # 0x24
                output.append(0x00)  # Numeric type byte
                output.append(0x03 << 4)  # i64 type in meta
                if val.startswith('0x') or val.startswith('-0x'):
                    output.extend(struct.pack('<q', int(val, 16)))
                else:
                    output.extend(struct.pack('<q', int(val)))
            else:
                # Treat as unquoted string (convenience)
                output.append(OPCODES['push'])  # 0x24
                output.append(0x30)  # String type byte
                data = val.encode('utf-8')
                output.extend(struct.pack('<Q', len(data)))
                output.extend(data)

        elif op in ('get', 'set', 'unset', 'dec'):
            output.append(OPCODES[op])
            output.extend(encode_string(args[0]))

        elif op in ('jmp', 'jmpif', 'jmpifn'):
            output.append(OPCODES[op])
            target = args[0]
            if target in labels:
                output.extend(struct.pack('<Q', labels[target]))
            elif target in sections:
                output.extend(struct.pack('<Q', sections[target]))
            else:
                fixups.append((len(output), target))
                output.extend(struct.pack('<Q', 0))

        elif op == 'call':
            output.append(OPCODES['call'])
            name = args[0]
            if name.startswith('"') and name.endswith('"'):
                name = name[1:-1]
            output.extend(encode_string(name))
            argc = int(args[1]) if len(args) >= 2 else 0
            output.extend(struct.pack('<Q', argc))  # 64-bit argc

        elif op == 'ret':
            output.append(OPCODES['ret'])

        elif op in OPCODES:
            output.append(OPCODES[op])

        else:
            print(f"Error: unknown opcode '{op}'")
            output.append(0)

    # Apply fixups
    for offset, target in fixups:
        if target in labels:
            addr = labels[target]
        elif target in sections:
            addr = sections[target]
        else:
            print(f"Error: unresolved label '{target}'")
            addr = 0
        output[offset:offset+8] = struct.pack('<Q', addr)

    # Function table offset is current end of output
    table_off = len(output)

    # Write function table
    # Format per function: name(str) + entry(u64) + ret(u8) + paramCount(u64) + params(str[])
    for fn_name in functions:
        fn_entry = labels.get(fn_name, 0)
        output.extend(encode_string(fn_name))   # name
        output.extend(struct.pack('<Q', fn_entry))  # entry point
        output.append(0)  # ret type (0 = default)
        output.extend(struct.pack('<Q', 0))  # param count = 0
        # No params to write

    # Write header
    # Magic: 8 bytes (matching what VM expects)
    magic = b'  \xc2\xbd6e\xc3\xa8'  # "  ½6eè" = 2 spaces + 6 UTF-8 bytes = 8 bytes
    fn_count = len(functions)

    # Pack header: magic(8) + table_off(8) + fnCount(8) + entry_main(8) + line_map_off(8)
    header = magic
    header += struct.pack('<Q', table_off)      # table_off
    header += struct.pack('<Q', fn_count)       # fnCount
    header += struct.pack('<Q', entry_point)    # entry_main
    header += struct.pack('<Q', 0)              # line_map_off (0 = none)

    output[0:HEADER_SIZE] = header

    return bytes(output)


def main():
    if len(sys.argv) < 2:
        print("Usage: masm.py <input.mas> [output.bin]")
        sys.exit(2)

    input_file = sys.argv[1]

    if len(sys.argv) >= 3:
        output_file = sys.argv[2]
    else:
        output_file = input_file.rsplit('.', 1)[0] + '.bin'

    with open(input_file, 'r', encoding='utf-8') as f:
        source = f.read()

    bytecode = assemble(source)

    with open(output_file, 'wb') as f:
        f.write(bytecode)

    print(f"Assembled {len(bytecode)} bytes -> {output_file}")

    if '--hex' in sys.argv:
        print("\nHex dump:")
        for i in range(0, len(bytecode), 16):
            chunk = bytecode[i:i+16]
            hex_part = ' '.join(f'{b:02x}' for b in chunk)
            print(f"  {i:04x}: {hex_part}")


if __name__ == '__main__':
    main()
    print("\n"*3)
