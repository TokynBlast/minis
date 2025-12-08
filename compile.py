#!/usr/bin/env python3

import sys
import struct
import os
import inspect

# --- Bytecode opcodes (from bytecode.hpp) ---
OPCODES = {
  # importing
  'IMPORT_REGISTER': 0x0A,
  'IMPORT_FUNC': 0x01,
  'IMPORT_LOAD': 0x02,
  'IMPORT_STORE': 0x03,

  # pushing
  'PUSH_REGISTER': 0x0B,
  'PUSH_INT': 0x04,
  'PUSH_FLOAT': 0x05,
  'PUSH_BOOL': 0x06,
  'PUSH_STRING': 0x07,
  'PUSH_NULL': 0x09,
  'PUSH_LIST': 0x010,

  # variable stuff :)
  'VARIABLE_REGISTER': 0x0C,
  'GET': 0x011,
  'SET': 0x012,
  'DECLARE': 0x013,
  'UNSET': 0x14,

  // math opertions
  'MATH_REGISTER': 0x0D,
  'ADD': 0x015,
  'SUBTRACT': 0x016,
  'MULTIPLY': 0x017,
  'DIVIDE': 0x018,
  'NEGATE': 0x019,

  'LOGIC_REGISTER = 0x0E,
  'EQUAL': 0x020,
  'NOT_EQUAL': 0x21,
  'LESS_THAN_OR_EQUAL':  0x022,
  'LESS_THAN': 0x023,
  'AND': 0x024,
  'OR': 0x025,
  'JUMP': 0x026,
  'JUMP_IF': 0x027,
  'NOT_EQUAL': 0x28,


  'FUNCTION_REGISTER': 0x0F,
  'CALL': 0x029,
  'TAIL': 0x030,
  'RETURN': 0x031,
  'RETURN_VOID': 0x032,

  'GENERAL_REGISTER': 0x01A
  'HALT': 0x033,
  'SLICE': 0x034,
  'SET_INDEX': 0x035,
  'NO_OP': 0x036,
  'POP': 0x037,
  'INDEX': 0x038
}

# --- Bytecode buffer and helpers ---
bytecode = bytearray()
position = 0
line_map = []

declared_vars = set()
scope_stack = []  # Track variable scopes for functions/blocks
functions = []  # List of {name, entry, params}
function_table = {}  # name -> entry
main_lines = []
in_function = False
current_function = None
function_bodies = {}

# --- Emit helpers ---
def emit_u8(val):
    global bytecode, position
    bytecode.append(val & 0xFF)
    position += 1

def emit_u64(val):
    global bytecode, position
    bytecode.extend(struct.pack('<Q', val))
    position += 8

def emit_f64(val):
    global bytecode, position
    bytecode.extend(struct.pack('<d', float(val)))
    position += 8

def emit_str(s):
    global bytecode, position
    encoded = s.encode('utf-8')
    emit_u64(len(encoded))
    bytecode.extend(encoded)
    position += len(encoded)

def emit_op(name):
    emit_u64(OPCODES[name])

# --- Whitespace and keyword helpers ---
def get_line_col(source, pos):
    """
    Returns (line, col) for a given character position in source.
    Line and column are 1-based.
    """
    line = source.count('\n', 0, pos) + 1
    last_nl = source.rfind('\n', 0, pos)
    if last_nl == -1:
        col = pos + 1
    else:
        col = pos - last_nl
    return line, col
def skip_ws(s, i):
    while i < len(s) and s[i] in ' \t':
        i += 1
    return i

def match_kw(s, i, kw):
    return s.startswith(kw, i)

def collect_parens(start_idx, lines):
    """Collect a full parenthesized expression across lines, returns (expr, next_line_idx)"""
    expr = ''
    depth = 0
    i = start_idx
    started = False
    while i < len(lines):
        l = lines[i]
        for idx, c in enumerate(l):
            if c == '(':
                depth += 1
                started = True
            if started:
                expr += c
            if c == ')':
                depth -= 1
                if depth == 0 and started:
                    expr += l[idx+1:]
                    return expr, i+1
        if started and depth == 0:
            break
        expr += '\n'
        i += 1
    return expr, i

def collect_braces(start_idx, lines):
    """Collect a full braced block across lines, returns (block_lines, next_line_idx)"""
    block = []
    depth = 0
    i = start_idx
    started = False
    while i < len(lines):
        l = lines[i]
        if '{' in l:
            depth += l.count('{')
            started = True
        if started:
            block.append(l)
        if '}' in l:
            depth -= l.count('}')
            if depth == 0 and started:
                return block, i+1
        i += 1
    return block, i

def LogicOr(s, i):
                    i, _ = LogicAnd(s, i)
                    while True:
                        i = skip_ws(s, i)
                        if i >= len(s):
                            break
                        if s[i:i+2] == '||':
                            i += 2
                            i, _ = LogicAnd(s, i)
                            emit_op('OR')
                        else:
                            break
                    return i, None
def LogicAnd(s, i):
                    i, _ = Equality(s, i)
                    while True:
                        i = skip_ws(s, i)
                        if i >= len(s):
                            break
                        if s[i:i+2] == '&&':
                            i += 2
                            i, _ = Equality(s, i)
                            emit_op('AND')
                        else:
                            break
                    return i, None

# --- Main parse/compile loop ---
def compile_lines(lines, source=None, line_offset=0):
    BUILTIN_FUNCTIONS = {'len', 'print', 'range', 'str', 'int', 'float', 'bool', 'input'}
    def compile_expr(expr, line_hint=None):
        expr = expr.strip()
        import traceback
        stack = traceback.extract_stack(limit=5)
        # print(f"[TRACE] compile_expr: '{expr}' (line {line_hint}) | stack: {[f'{frame.name}@{frame.lineno}' for frame in stack]}")

        def error(msg, expr=expr, line_hint=line_hint):
            line_snippet = None
            if source is not None and line_hint is not None:
                try:
                    line_num = int(line_hint) - 1
                    src_lines = source.splitlines()
                    if 0 <= line_num < len(src_lines):
                        line_snippet = src_lines[line_num].strip()
                except Exception:
                    pass
            print("[ERROR]", file=sys.stderr)
            print(f"  Message: {msg}", file=sys.stderr)
            print(f"  Expression: {expr}", file=sys.stderr)
            def compile_expr(expr, line_hint=None):
                expr = expr.strip()
                import traceback
                stack = traceback.extract_stack(limit=5)
                print(f"[TRACE] compile_expr: '{expr}' (line {line_hint}) | stack: {[f'{frame.name}@{frame.lineno}' for frame in stack]}")
                # --- Recursive-descent parser logic ---
                def error(msg, expr=expr, line_hint=line_hint):
                    line_snippet = None
                    if source is not None and line_hint is not None:
                        try:
                            line_num = int(line_hint) - 1
                            src_lines = source.splitlines()
                            if 0 <= line_num < len(src_lines):
                                line_snippet = src_lines[line_num].strip()
                        except Exception:
                            pass
                    print("[ERROR]", file=sys.stderr)
                    print(f"  Message: {msg}", file=sys.stderr)
                    print(f"  Expression: {expr}", file=sys.stderr)
                    if line_hint is not None:
                        print(f"  Line: {line_hint}", file=sys.stderr)
                    if line_snippet:
                        print(f"  Source: {line_snippet}", file=sys.stderr)
                    print(f"  Stack: {[f'{frame.name}@{frame.lineno}' for frame in stack]}", file=sys.stderr)

                def skip_ws(s, i):
                    while i < len(s) and s[i] in ' \t':
                        i += 1
                    return i


                def Equality(s, i):
                    i, _ = AddSub(s, i)
                    while True:
                        i = skip_ws(s, i)
                        if i >= len(s):
                            break
                        if s[i:i+2] == '==':
                            i += 2
                            i, _ = AddSub(s, i)
                            emit_op('EQ')
                        elif s[i:i+2] == '!=':
                            i += 2
                            i, _ = AddSub(s, i)
                            emit_op('NE')
                        elif s[i:i+2] == '>=':
                            i += 2
                            i, _ = AddSub(s, i)
                            emit_op('LE')
                        elif s[i:i+2] == '<=':
                            i += 2
                            i, _ = AddSub(s, i)
                            emit_op('LE')
                        elif s[i] == '<':
                            i += 1
                            i, _ = AddSub(s, i)
                            emit_op('LT')
                        elif s[i] == '>':
                            i += 1
                            i, _ = AddSub(s, i)
                            emit_op('LT')
                        else:
                            break
                    return i, None

                def AddSub(s, i):
                    i, _ = MulDiv(s, i)
                    while True:
                        i = skip_ws(s, i)
                        if i >= len(s):
                            break
                        if s[i] == '+':
                            i += 1
                            i, _ = MulDiv(s, i)
                            emit_op('ADD')
                        elif s[i] == '-':
                            i += 1
                            i, _ = MulDiv(s, i)
                            emit_op('SUB')
                        else:
                            break
                    return i, None

                def MulDiv(s, i):
                    i, _ = Factor(s, i)
                    while True:
                        i = skip_ws(s, i)
                        if i >= len(s):
                            break
                        if s[i] == '*':
                            i += 1
                            i, _ = Factor(s, i)
                            emit_op('MUL')
                        elif s[i] == '/':
                            i += 1
                            i, _ = Factor(s, i)
                            emit_op('DIV')
                        else:
                            break
                    return i, None

                def Factor(s, i):
                    i = skip_ws(s, i)
                    # Parentheses
                    if i < len(s) and s[i] == '(':
                        i += 1
                        i, _ = LogicOr(s, i)
                        i = skip_ws(s, i)
                        if i < len(s) and s[i] == ')':
                            i += 1
                        else:
                            error("Missing closing ')'", s, line_hint)
                        return i, None
                    # String literal
                    if i < len(s) and (s[i] == '"' or s[i] == "'"):
                        quote = s[i]
                        i += 1
                        start = i
                        while i < len(s) and s[i] != quote:
                            i += 1
                        val = s[start:i]
                        emit_op('PUSH_S')
                        emit_str(val)
                        i += 1
                        return i, None
                    # Number literal
                    start = i
                    while i < len(s):
                        if s[i].isdigit() or s[i] == '.':
                            i += 1
                        else:
                            break
                    if start != i:
                        val = s[start:i]
                        try:
                            num_val = float(val)
                            emit_op('PUSH_I')
                            emit_f64(num_val)
                        except ValueError:
                            error(f"Invalid number: {val}", s, line_hint)
                        return i, None
                    # Identifier or function call
                    start = i
                    while i < len(s):
                        if s[i].isalnum() or s[i] == '_':
                            i += 1
                        else:
                            break
                    if start != i:
                        ident = s[start:i]
                        i = skip_ws(s, i)
                        if i < len(s) and s[i] == '(': # function call
                            i += 1
                            args = []
                            while i < len(s) and s[i] != ')':
                                arg_start = i
                                paren_depth = 0
                                while i < len(s):
                                    if s[i] == ',':
                                        if paren_depth == 0:
                                            break
                                    elif s[i] == '(':
                                        paren_depth += 1
                                    elif s[i] == ')':
                                        if paren_depth == 0:
                                            break
                                        else:
                                            paren_depth -= 1
                                    i += 1
                                arg = s[arg_start:i].strip()
                                if arg:
                                    compile_expr(arg, line_hint)
                                i = skip_ws(s, i)
                                if i < len(s) and s[i] == ',':
                                    i += 1
                            i = skip_ws(s, i)
                            if i < len(s) and s[i] == ')':
                                i += 1
                            emit_op('CALL')
                            emit_str(ident)
                            emit_u64(len(args))
                            return i, None
                        else:
                            emit_op('GET')
                            emit_str(ident)
                            return i, None
                    # Fallback
                    error(f"Expression parse error", s, line_hint)
                    return i, None

                # Always parse the expression
                LogicOr(expr, 0)

            # --- Character-by-character state machine ---
            src = source if source is not None else '\n'.join(lines)
            i = 0
            length = len(src)
            while i < length:
                c = src[i]
                # Skip whitespace
                if c in ' \t':
                    i += 1
                    continue
                # Skip single-line comments (// ...)
                if c == '/' and i+1 < length and src[i+1] == '/':
                    while i < length and src[i] != '\n':
                        i += 1
                    i += 1
                    continue
                # Skip hash comments (# ...)
                if c == '#':
                    while i < length and src[i] != '\n':
                        i += 1
                    i += 1
                    continue
                # Multi-line comment (/* ... */)
                if c == '/' and i+1 < length and src[i+1] == '*':
                    i += 2
                    while i+1 < length and not (src[i] == '*' and src[i+1] == '/'):
                        i += 1
                    i += 2
                    continue
                # Statement/Expression boundaries
                start = i
                # Find end of statement (semicolon, newline, or block open/close)
                while i < length and src[i] not in ';\n{}':
                    i += 1
                expr = src[start:i].strip()
                # Handle block open/close as separate tokens
                if expr:
                    compile_expr(expr)
                # Emit block open/close as needed
                if i < length and src[i] in '{}':
                    # Could emit block open/close opcodes here if needed
                    i += 1
                    continue
                # Move past semicolon or newline
                if i < length and src[i] in ';\n':
                    i += 1
                    continue
                if i >= len(s):
                    break
                if s[i] == '+':
                    i += 1
                    i, _ = MulDiv(s, i)
                    emit_op('ADD')
                elif s[i] == '-':
                    i += 1
                    i, _ = MulDiv(s, i)
                    emit_op('SUB')
                else:
                    break
            return i, None

        def MulDiv(s, i):
            i, _ = Factor(s, i)
            while True:
                i = skip_ws(s, i)
                if i >= len(s):
                    break
                if s[i] == '*':
                    i += 1
                    i, _ = Factor(s, i)
                    emit_op('MUL')
                elif s[i] == '/':
                    i += 1
                    i, _ = Factor(s, i)
                    emit_op('DIV')
                else:
                    break
            return i, None

        def Factor(s, i):
            i = skip_ws(s, i)
            # Parentheses
            if i < len(s) and s[i] == '(':
                i += 1
                i, _ = LogicOr(s, i)
                i = skip_ws(s, i)
                if i < len(s) and s[i] == ')':
                    i += 1
                else:
                    error("Missing closing ')'", s, line_hint)
                return i, None
            # String literal
            if i < len(s) and (s[i] == '"' or s[i] == "'"):
                quote = s[i]
                i += 1
                start = i
                while i < len(s) and s[i] != quote:
                    i += 1
                val = s[start:i]
                emit_op('PUSH_S')
                emit_str(val)
                i += 1
                return i, None
            # Number literal
            start = i
            while i < len(s):
                if s[i].isdigit() or s[i] == '.':
                    i += 1
                else:
                    break
            if start != i:
                val = s[start:i]
                try:
                    num_val = float(val)
                    emit_op('PUSH_I')
                    emit_f64(num_val)
                except ValueError:
                    error(f"Invalid number: {val}", s, line_hint)
                return i, None
            # Identifier or function call
            start = i
            while i < len(s):
                if s[i].isalnum() or s[i] == '_':
                    i += 1
                else:
                    break
            if start != i:
                ident = s[start:i]
                i = skip_ws(s, i)
                if i < len(s) and s[i] == '(': # function call
                    i += 1
                    args = []
                    while i < len(s) and s[i] != ')':
                        arg_start = i
                        paren_depth = 0
                        while i < len(s):
                            if s[i] == ',':
                                if paren_depth == 0:
                                    break
                            elif s[i] == '(':
                                paren_depth += 1
                            elif s[i] == ')':
                                if paren_depth == 0:
                                    break
                                else:
                                    paren_depth -= 1
                            i += 1
                        arg = s[arg_start:i].strip()
                        if arg:
                            compile_expr(arg, line_hint)
                        i = skip_ws(s, i)
                        if i < len(s) and s[i] == ',':
                            i += 1
                    i = skip_ws(s, i)
                    if i < len(s) and s[i] == ')':
                        i += 1
                    emit_op('CALL')
                    emit_str(ident)
                    emit_u64(len(args))
                    return i, None
                else:
                    emit_op('GET')
                    emit_str(ident)
                    return i, None
            # Fallback
            error(f"Expression parse error", s, line_hint)
            return i, None

        # Always parse the expression
        LogicOr(expr, 0)

    # --- Main loop: process each line ---
    i = 0
    while i < len(lines):
        line = lines[i].strip()
        current_expr_line = line_offset + i + 1
        # Skip empty lines and comments
        if not line or line.startswith('#'):
            i += 1
            continue
        # Handle let declarations
        if line.startswith('let '):
            let_expr = line[len('let '):].strip()
            if '=' in let_expr:
                var, rhs = let_expr.split('=', 1)
                var = var.strip()
                rhs = rhs.strip()
                compile_expr(rhs, current_expr_line)
                emit_op('DECL')
                emit_str(var)
                emit_op('SET')
                emit_str(var)
            else:
                var = let_expr.strip()
                emit_op('DECL')
                emit_str(var)
            i += 1
            continue
        # Handle return
        if line.startswith('return '):
            ret_expr = line[len('return '):].strip()
            if ret_expr:
                compile_expr(ret_expr, current_expr_line)
                emit_op('RET')
            else:
                emit_op('RET_VOID')
            i += 1
            continue
        if line == 'return':
            emit_op('RET_VOID')
            i += 1
            continue
        # Handle yield
        if line.startswith('yield '):
            yield_expr = line[len('yield '):].strip()
            compile_expr(yield_expr, current_expr_line)
            emit_op('YIELD')
            i += 1
            continue
        if line == 'yield':
            emit_op('YIELD')
            i += 1
            continue
        # Handle import
        if line.startswith('import '):
            import_name = line[len('import '):].strip()
            emit_op('IMPORTED_LOAD')
            emit_str(import_name)
            i += 1
            continue
        # Handle plugin
        if line.startswith('plugin '):
            plugin_name = line[len('plugin '):].strip()
            emit_op('IMPORTED_FUNC')
            emit_str(plugin_name)
            i += 1
            continue
        # Handle if/elif/else/while blocks (not implemented here, but could be added)
        # Otherwise, treat as expression or statement
        compile_expr(line, current_expr_line)
        i += 1

# --- Main entry ---
def main():
    if len(sys.argv) < 2:
        print("Usage: python compile2.py <input.mi> [-o output.mic]")
        sys.exit(1)
    input_file = sys.argv[1]
    output_file = sys.argv[2] if len(sys.argv) > 2 else "compiler.mic"
    with open(input_file) as f:
        source = f.read()
        lines = source.splitlines()
    # First pass: gather functions and main
    compile_lines(lines, source)
    # Second pass: emit functions
    for fn in functions:
        # Emit JMP to skip function body
        emit_op('JMP')
        skip_offset = len(bytecode)
        emit_u64(0)
        entry_point = len(bytecode)
        # Parameters as declared vars
        for p in fn['params']:
            declared_vars.add(p)
        # Emit function body
        compile_lines(fn['body'])
        # Emit RET_VOID at end
        emit_op('RET_VOID')
        # Patch JMP
        after_fn = len(bytecode)
        bytecode[skip_offset:skip_offset+8] = struct.pack('<Q', after_fn)
        function_table[fn['name']] = entry_point
    # Emit main
    # (Assume main_lines is filled if needed)
    # Write output (header omitted for brevity)
    with open(output_file, 'wb') as f:
        f.write(bytecode)
    print(f"✓ Compiled to {output_file} ({len(bytecode)} bytes)")

if __name__ == '__main__':
    main()
