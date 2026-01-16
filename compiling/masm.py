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
        s = raw.encode('utf-8')
        return struct.pack('<I', len(s)) + s

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
    """Encode a string with length prefix"""
    data = s.encode('utf-8')
    return struct.pack('<I', len(data)) + data


def assemble(source):
    """
    Assemble MASM source into bytecode.
    Returns bytes.
    """
    output = bytearray()
    labels = {}
    fixups = []
    sections = {}
    current_section = None

    lines = source.splitlines()

    # First pass: collect labels and sections, calculate offsets
    byte_offset = 0
    for line in lines:
        line = line.split(';', 1)[0].strip()
        if not line:
            continue

        # Section
        if line.startswith('.'):
            current_section = line[1:].strip()
            sections[current_section] = byte_offset
            continue

        # Label
        if line.endswith(':'):
            label = line[:-1].strip()
            labels[label] = byte_offset
            continue

        # Parse instruction
        tokens = shlex.split(line)
        if not tokens:
            continue

        op = tokens[0].lower()
        args = tokens[1:]

        # Calculate size
        # Typed push: i8, i16, i32, i64, ui8, etc.
        if op in TYPE_SIZES:
            byte_offset += 1 + TYPE_SIZES[op]

        elif op == 'str':
            val = args[0] if args else '""'
            if val.startswith('"') and val.endswith('"'):
                val = val[1:-1]
            byte_offset += 1 + 4 + len(val.encode('utf-8'))

        elif op in ('bool', 'null', 'list', 'dict'):
            byte_offset += 1 + (1 if op == 'bool' else 0)

        elif op in ('get', 'set', 'unset', 'dec'):
            byte_offset += 1 + 4 + len(args[0].encode('utf-8'))

        elif op in ('jmp', 'jmpif', 'jmpifn'):
            byte_offset += 1 + 4

        elif op == 'call':
            name = args[0]
            if name.startswith('"') and name.endswith('"'):
                name = name[1:-1]
            byte_offset += 1 + 4 + len(name.encode('utf-8')) + 1

        elif op in OPCODES:
            byte_offset += 1

        else:
            print(f"Warning: unknown opcode '{op}'")
            byte_offset += 1

    # Second pass: emit bytecode
    current_section = None
    for line in lines:
        line = line.split(';', 1)[0].strip()
        if not line:
            continue

        if line.startswith('.'):
            current_section = line[1:].strip()
            continue

        if line.endswith(':'):
            continue

        tokens = shlex.split(line)
        if not tokens:
            continue

        op = tokens[0].lower()
        args = tokens[1:]

        # Emit
        # Typed push: i8 42, i32 1000, str "hello", etc.
        if op in TYPE_SIZES or op in ('str', 'bool', 'null', 'list', 'dict'):
            output.append(OPCODES[op])
            if args:
                output.extend(parse_typed_value(op, args[0]))
            elif op in ('null', 'list', 'dict'):
                pass  # no data needed

        elif op in ('get', 'set', 'unset', 'dec'):
            output.append(OPCODES[op])
            output.extend(encode_string(args[0]))

        elif op in ('jmp', 'jmpif', 'jmpifn'):
            output.append(OPCODES[op])
            target = args[0]
            if target in labels:
                output.extend(struct.pack('<I', labels[target]))
            elif target in sections:
                output.extend(struct.pack('<I', sections[target]))
            else:
                fixups.append((len(output), target))
                output.extend(struct.pack('<I', 0))

        elif op == 'call':
            output.append(OPCODES['call'])
            name = args[0]
            if name.startswith('"') and name.endswith('"'):
                name = name[1:-1]
            output.extend(encode_string(name))
            argc = int(args[1]) if len(args) > 1 else 0
            output.append(argc)

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
        output[offset:offset+4] = struct.pack('<I', addr)

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
