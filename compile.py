#!/usr/bin/env python3
"""
Simple Minis Compiler - String-based compilation to AVOCADO1 bytecode
No tokenizing, no uglifying - just simple string literal matching!
"""

import sys
import struct
import os

class MinisCompiler:
    def __init__(self):
        self.bytecode = bytearray()
        self.position = 0
        self.functions = []  # List of {name, entry, params, isVoid, typed, ret}
        self.strings = []
        self.imports = []
        self.in_function = False
        self.current_function_lines = []
        
        # Built-in functions from vm.cpp
        self.builtins = {
            'print', 'abs', 'neg', 'range', 'max', 'min', 'sort', 'reverse',
            'sum', 'input', 'len', 'split', 'upper', 'lower', 'round', 'random',
            'open', 'close', 'write', 'read', 'flush'
        }
        
    def write_u8(self, value):
        """Write unsigned 8-bit integer"""
        self.bytecode.append(value & 0xFF)
        self.position += 1
        
    def write_u64(self, value):
        """Write unsigned 64-bit integer"""
        self.bytecode.extend(struct.pack('<Q', value))
        self.position += 8
        
    def write_s64(self, value):
        """Write signed 64-bit integer"""
        self.bytecode.extend(struct.pack('<q', value))
        self.position += 8
        
    def write_f64(self, value):
        """Write 64-bit float"""
        self.bytecode.extend(struct.pack('<d', value))
        self.position += 8
        
    def write_str(self, text):
        """Write string with length prefix - VM expects 64-bit length!"""
        encoded = text.encode('utf-8')
        self.write_u64(len(encoded))  # Use 64-bit length
        self.bytecode.extend(encoded)
        self.position += len(encoded)
        
    def write_u32(self, value):
        """Write unsigned 32-bit integer"""
        self.bytecode.extend(struct.pack('<I', value))
        self.position += 4
    
    def emit_op(self, opcode):
        """Emit a bytecode operation - VM expects 64-bit opcodes!"""
        self.write_u64(opcode)
    
    # Opcode constants (from bytecode.hpp - CORRECT enumeration order!)
    # Starting from 0x01:
    IMPORTED_FUNC = 0x01
    IMPORTED_LOAD = 0x02
    IMPORTED_STORE = 0x03
    NOP = 0x04
    PUSH_I = 0x05
    PUSH_F = 0x06
    PUSH_B = 0x07
    PUSH_S = 0x08
    PUSH_C = 0x09
    PUSH_N = 0x0A
    MAKE_LIST = 0x0B
    GET = 0x0C
    SET = 0x0D
    DECL = 0x0E
    POP = 0x0F
    ADD = 0x10
    SUB = 0x11
    MUL = 0x12
    DIV = 0x13
    NEG = 0x14
    EQ = 0x15
    NE = 0x16
    LT = 0x17
    LE = 0x18
    AND = 0x19
    OR = 0x1A
    JMP = 0x1B
    JF = 0x1C
    CALL = 0x1D
    RET = 0x1E
    RET_VOID = 0x1F
    HALT = 0x20
    UNSET = 0x21
    SLICE = 0x22
    INDEX = 0x23
    SET_INDEX = 0x24
    TAIL = 0x25
    YIELD = 0x26
    NOT = 0x27
    
    def is_builtin(self, name):
        """Check if a function is a built-in"""
        return name in self.builtins
        
    def compile_line(self, line):
        """Compile a single line using string literal comparison"""
        line = line.strip()
        
        # Skip empty lines and comments
        if not line or line.startswith('#') or line.startswith('//'):
            return
        
        # Import statements: import dev
        if line.startswith('import '):
            module_name = line[7:].strip()
            self.imports.append(module_name)
            print(f"  Imported: {module_name}")
            # Imports don't generate bytecode, just track them
            return
        
        # Function definitions: fn name() { ... }
        # For now, just skip function definitions as they're compiled separately
        if line.startswith('fn '):
            # Skip until we hit the closing brace (handled by caller)
            return
            
        # Opening/closing braces
        if line == '{' or line == '}':
            return
        
        # Control flow - if, elif, else, while - handled specially by compile()
        if line.startswith('if ') or line.startswith('elif ') or line.startswith('else') or line.startswith('while '):
            # These are handled in compile() method
            return
        
        # Print with module.function() call: print(module.function(args))
        if line.startswith('print(') and '.' in line and line.count('(') >= 2:
            # Extract the inner part: module.function(args)
            inner = line[6:-1]  # Remove "print(" and ")"
            
            # Parse module.function(args)
            if '.' in inner:
                parts = inner.split('.')
                module_name = parts[0].strip()
                func_part = '.'.join(parts[1:])
                
                # Extract function name and arguments
                paren_idx = func_part.find('(')
                if paren_idx != -1:
                    func_name = func_part[:paren_idx].strip()
                    # Find matching closing paren
                    args_str = func_part[paren_idx+1:-1].strip()  # Remove ( and )
                    
                    # Parse arguments
                    args = []
                    if args_str:
                        args = [arg.strip() for arg in args_str.split(',')]
                    
                    # Push arguments onto stack
                    for arg in args:
                        if arg.startswith('"') and arg.endswith('"'):
                            self.emit_op(self.PUSH_S)
                            self.write_str(arg[1:-1])
                        elif arg.replace('-', '').replace('.', '').isdigit():
                            self.emit_op(self.PUSH_I)
                            self.write_f64(float(arg))
                        else:
                            self.emit_op(self.GET)
                            self.write_str(arg)
                    
                    # Emit CALL instruction with module_function_ naming
                    self.emit_op(self.CALL)
                    self.write_str(f"{module_name}_{func_name}_")
                    self.write_u64(len(args))
                    
                    # Now CALL print with the result
                    self.emit_op(self.CALL)
                    self.write_str("print")
                    self.write_u64(1)
                    return
        
        # Module-qualified function calls: module.function(args)
        if '.' in line and '(' in line and not line.startswith('print('):
            # Parse module.function(arg1, arg2, ...)
            parts = line.split('.')
            module_name = parts[0].strip()
            func_part = '.'.join(parts[1:])
            
            # Extract function name and arguments
            paren_idx = func_part.find('(')
            if paren_idx != -1:
                func_name = func_part[:paren_idx].strip()
                # Find the closing paren
                close_paren = func_part.rfind(')')
                if close_paren != -1:
                    args_str = func_part[paren_idx+1:close_paren].strip()
                    
                    # Parse arguments
                    args = []
                    if args_str:
                        # Simple argument parsing - split by comma
                        args = [arg.strip() for arg in args_str.split(',')]
                    
                    # Push arguments onto stack
                    for arg in args:
                        if arg.startswith('"') and arg.endswith('"'):
                            # String literal
                            self.emit_op(self.PUSH_S)
                            self.write_str(arg[1:-1])
                        elif arg.replace('-', '').replace('.', '').isdigit():
                            # Numeric literal
                            self.emit_op(self.PUSH_I)
                            self.write_f64(float(arg))
                        elif arg == 'true' or arg == 'false':
                            # Boolean literal
                            self.emit_op(self.PUSH_B)
                            self.write_u8(1 if arg == 'true' else 0)
                        else:
                            # Variable reference
                            self.emit_op(self.GET)
                            self.write_str(arg)
                    
                    # Emit CALL instruction with module_function_ naming
                    self.emit_op(self.CALL)
                    self.write_str(f"{module_name}_{func_name}_")
                    self.write_u64(len(args))
                    
                    # If not assigned to a variable, pop the result
                    if '=' not in line:
                        self.emit_op(self.POP)
                    
                    return
        
        # Return statements: return value;
        if line.startswith('return '):
            value_part = line[7:].strip().rstrip(';')
            
            # Parse the return value
            if value_part == 'true' or value_part == 'false':
                self.emit_op(self.PUSH_B)
                self.write_u8(1 if value_part == 'true' else 0)
            elif value_part.isdigit():
                self.emit_op(self.PUSH_I)
                self.write_f64(float(value_part))
            elif value_part.startswith('"') and value_part.endswith('"'):
                self.emit_op(self.PUSH_S)
                self.write_str(value_part[1:-1])
            elif value_part:  # Variable or expression
                self.emit_op(self.GET)
                self.write_str(value_part)
            
            self.emit_op(self.RET)
            return
            
        # String literals - print statements
        if 'print(' in line and '"' in line:
            # Extract string between quotes
            start = line.find('"')
            end = line.find('"', start + 1)
            if start != -1 and end != -1:
                text = line[start + 1:end]
                # PUSH_S opcode
                self.emit_op(self.PUSH_S)
                self.write_str(text)
                # CALL opcode for print
                self.emit_op(self.CALL)
                self.write_str("print")
                self.write_u64(1)  # 1 argument
                return
        
        # Boolean literals - print statements
        if 'print(' in line:
            val = line.replace('print(', '').replace(')', '').strip()
            if val == 'true' or val == 'false':
                self.emit_op(self.PUSH_B)
                self.write_u8(1 if val == 'true' else 0)
                self.emit_op(self.CALL)
                self.write_str("print")
                self.write_u64(1)
                return
        
        # Integer literals - print statements
        if 'print(' in line and line.replace('print(', '').replace(')', '').strip().isdigit():
            num = int(line.replace('print(', '').replace(')', '').strip())
            # PUSH_I opcode (VM reads as float!)
            self.emit_op(self.PUSH_I)
            self.write_f64(float(num))
            # CALL for print
            self.emit_op(self.CALL)
            self.write_str("print")
            self.write_u64(1)
            return
            
        # Variable declarations: let x = 42
        if line.startswith('let ') and '=' in line:
            parts = line[4:].split('=')
            var_name = parts[0].strip()
            value = parts[1].strip().rstrip(';')
            
            # Check for function calls
            if '()' in value:
                func_name = value.replace('()', '').strip()
                # Emit CALL instruction
                self.emit_op(self.CALL)
                # Check if it's a user-defined function (add _ suffix)
                if func_name in [f['name'] for f in self.functions]:
                    self.write_str(func_name + '_')
                else:
                    self.write_str(func_name)
                self.write_u64(0)  # 0 arguments
            # Push the value
            elif value == 'true' or value == 'false':
                self.emit_op(self.PUSH_B)
                self.write_u8(1 if value == 'true' else 0)
            elif value.isdigit():
                self.emit_op(self.PUSH_I)
                self.write_f64(float(value))
            elif value.replace('-', '').isdigit():  # Negative numbers
                self.emit_op(self.PUSH_I)
                self.write_f64(float(value))
            elif value.startswith('"') and value.endswith('"'):
                self.emit_op(self.PUSH_S)
                self.write_str(value[1:-1])
            elif value == '[]':  # Empty list
                self.emit_op(self.MAKE_LIST)
                self.write_u64(0)
            else:  # Variable reference
                self.emit_op(self.GET)
                self.write_str(value)
            
            # DECL opcode - declare and store
            # DECL format: string(varname) + u64(type) where 0xECull means infer type
            self.emit_op(self.DECL)
            self.write_str(var_name)
            self.write_u64(0xEC)  # Type inference marker
            return
            
        # Load and use variable
        if 'print(' in line and not '"' in line and not line.replace('print(', '').replace(')', '').strip().isdigit():
            var_name = line.replace('print(', '').replace(')', '').strip()
            if var_name != 'true' and var_name != 'false':
                # GET opcode - load variable
                self.emit_op(self.GET)
                self.write_str(var_name)
                # CALL for print
                self.emit_op(self.CALL)
                self.write_str("print")
                self.write_u64(1)
                return
        
        # Standalone function calls: functionName()
        if '()' in line and not line.startswith('fn '):
            func_name = line.replace('()', '').strip()
            # Emit CALL instruction
            self.emit_op(self.CALL)
            # Check if it's a user-defined function (add _ suffix)
            if func_name in [f['name'] for f in self.functions]:
                self.write_str(func_name + '_')
            else:
                self.write_str(func_name)
            self.write_u64(0)  # 0 arguments
            # Pop the return value since it's not used
            self.emit_op(self.POP)
            return
    
    def compile(self, source_code):
        """Compile source code to AVOCADO1 bytecode"""
        # Split into lines
        lines = source_code.split('\n')
        
        # First pass: gather imports
        for line in lines:
            line_stripped = line.strip()
            if line_stripped.startswith('import '):
                module_name = line_stripped[7:].strip()
                if module_name not in self.imports:
                    self.imports.append(module_name)
                    print(f"  Imported: {module_name}")
        
        # Reserve space for header
        header_size = 32
        code_start = header_size
        
        # Reset and skip header
        self.bytecode = bytearray([0] * header_size)
        self.position = header_size
        
        # Load and link imported modules BEFORE compiling our code
        imported_functions = []
        for module_name in self.imports:
            bytecode_file = f"{module_name}.mic"
            if os.path.exists(bytecode_file):
                print(f"  Linking {bytecode_file}...")
                with open(bytecode_file, 'rb') as f:
                    # Read magic
                    magic = f.read(8).decode('ascii')
                    if magic != 'AVOCADO1':
                        print(f"    Warning: Invalid magic in {bytecode_file}")
                        continue
                    
                    # Read header
                    mod_table_offset = struct.unpack('<Q', f.read(8))[0]
                    mod_fn_count = struct.unpack('<Q', f.read(8))[0]
                    mod_entry_main = struct.unpack('<Q', f.read(8))[0]
                    
                    # Read code section (from header end to table start)
                    f.seek(32)  # Skip header
                    code_size = mod_table_offset - 32
                    module_code = f.read(code_size)
                    
                    # Calculate offset for this module's code in our bytecode
                    # This is where module code will be placed
                    module_code_offset = len(self.bytecode)
                    
                    # Append module code to our bytecode
                    self.bytecode.extend(module_code)
                    self.position = len(self.bytecode)
                    
                    # Read function table
                    f.seek(mod_table_offset)
                    for _ in range(mod_fn_count):
                        # Read function name (length-prefixed)
                        name_len = struct.unpack('<Q', f.read(8))[0]
                        fn_name = f.read(name_len).decode('utf-8')
                        
                        # Read function metadata
                        entry_point = struct.unpack('<Q', f.read(8))[0]
                        is_void = struct.unpack('<B', f.read(1))[0]
                        typed = struct.unpack('<B', f.read(1))[0]
                        ret_type = struct.unpack('<B', f.read(1))[0]  # u8 not u64!
                        param_count = struct.unpack('<Q', f.read(8))[0]
                        
                        # Adjust entry point to account for where we placed the code
                        # Entry points in module are relative to header (32 bytes)
                        # We need to make them relative to our placement
                        adjusted_entry = module_code_offset + (entry_point - 32)
                        
                        # Skip __main__ from imported modules
                        if fn_name == '__main__':
                            continue
                        
                        # Add to imported functions with module prefix
                        prefixed_name = f"{module_name}_{fn_name}"
                        imported_functions.append({
                            'name': prefixed_name,
                            'entry': adjusted_entry,
                            'isVoid': is_void,
                            'typed': typed,
                            'ret': ret_type,
                            'params': param_count
                        })
                        print(f"    Linked: {prefixed_name} @ {adjusted_entry}")
            else:
                print(f"  Warning: {bytecode_file} not found")
        
        # Now compile this module's code
        # Parse and handle control flow structures
        main_lines = []
        i = 0
        while i < len(lines):
            line = lines[i].strip()
            
            # Function definition
            if line.startswith('fn '):
                func_name = line[3:].split('(')[0].strip()
                # Collect function body
                func_lines = [line]
                i += 1
                brace_count = 0
                started = False
                while i < len(lines):
                    func_lines.append(lines[i])
                    if '{' in lines[i]:
                        brace_count += lines[i].count('{')
                        started = True
                    if '}' in lines[i]:
                        brace_count -= lines[i].count('}')
                    if started and brace_count == 0:
                        break
                    i += 1
                
                # Compile function
                self.compile_function(func_name, func_lines)
            else:
                main_lines.append(lines[i])
            i += 1
        
        # Remember where __main__ starts
        main_start = len(self.bytecode)
        
        # Compile main code with control flow handling
        self.compile_block(main_lines)
        
        # Add HALT opcode at the end
        self.emit_op(self.HALT)
        
        # Calculate table offset (after all code)
        table_offset = len(self.bytecode)
        
        # Write function table
        # Includes: __main__, user functions, and imported functions
        fn_count = 1 + len(self.functions) + len(imported_functions)
        
        # __main__ function entry (our entry point, not the module's)
        self.write_str("__main__")
        self.write_u64(main_start)  # Entry point for our main
        self.write_u8(1)  # isVoid = true (for main)
        self.write_u8(0)  # typed = false
        self.write_u8(0)  # return type (Type::Int) - u8!
        self.write_u64(0)  # param count
        
        # Write all user-defined functions
        for func in self.functions:
            self.write_str(func['name'] + '_')  # Add underscore suffix
            self.write_u64(func['entry'])
            self.write_u8(0)  # isVoid = false (functions return values)
            self.write_u8(0)  # typed = false
            self.write_u8(0)  # return type - u8!
            self.write_u64(0)  # param count (simplified - no params yet)
        
        # Write imported functions
        for func in imported_functions:
            self.write_str(func['name'])
            self.write_u64(func['entry'])
            self.write_u8(func['isVoid'])
            self.write_u8(func['typed'])
            self.write_u8(func['ret'])  # u8!
            self.write_u64(func['params'])
        
        # Now write the header at the beginning
        header = bytearray()
        # Magic number
        header.extend(b"AVOCADO1")
        # Table offset
        header.extend(struct.pack('<Q', table_offset))
        # Function count
        header.extend(struct.pack('<Q', fn_count))
        # Entry point (our __main__, not module's)
        header.extend(struct.pack('<Q', main_start))
        
        # Replace header
        self.bytecode[:header_size] = header
        
        return self.bytecode
    
    def compile_condition(self, condition):
        """Compile a condition expression and leave result on stack"""
        condition = condition.strip()
        
        # Simple comparisons: x == y, x < y, etc.
        if '==' in condition:
            parts = condition.split('==')
            self.compile_expression(parts[0].strip())
            self.compile_expression(parts[1].strip())
            self.emit_op(self.EQ)
        elif '!=' in condition:
            parts = condition.split('!=')
            self.compile_expression(parts[0].strip())
            self.compile_expression(parts[1].strip())
            self.emit_op(self.NE)
        elif '<=' in condition:
            parts = condition.split('<=')
            self.compile_expression(parts[0].strip())
            self.compile_expression(parts[1].strip())
            self.emit_op(self.LE)
        elif '<' in condition:
            parts = condition.split('<')
            self.compile_expression(parts[0].strip())
            self.compile_expression(parts[1].strip())
            self.emit_op(self.LT)
        elif '>=' in condition:
            # a >= b is same as b <= a
            parts = condition.split('>=')
            self.compile_expression(parts[1].strip())
            self.compile_expression(parts[0].strip())
            self.emit_op(self.LE)
        elif '>' in condition:
            # a > b is same as b < a
            parts = condition.split('>')
            self.compile_expression(parts[1].strip())
            self.compile_expression(parts[0].strip())
            self.emit_op(self.LT)
        else:
            # Just an expression (variable or literal)
            self.compile_expression(condition)
    
    def compile_expression(self, expr):
        """Compile an expression and push result on stack"""
        expr = expr.strip()
        
        if expr == 'true' or expr == 'false':
            self.emit_op(self.PUSH_B)
            self.write_u8(1 if expr == 'true' else 0)
        elif expr.isdigit() or (expr.startswith('-') and expr[1:].isdigit()):
            self.emit_op(self.PUSH_I)
            self.write_f64(float(expr))
        elif expr.startswith('"') and expr.endswith('"'):
            self.emit_op(self.PUSH_S)
            self.write_str(expr[1:-1])
        else:
            # Variable
            self.emit_op(self.GET)
            self.write_str(expr)
    
    def compile_block(self, lines):
        """Compile a block of lines with control flow support"""
        i = 0
        while i < len(lines):
            line = lines[i].strip()
            
            # if statement
            if line.startswith('if '):
                condition = line[3:].strip().rstrip('{').strip()
                
                # Compile condition
                self.compile_condition(condition)
                
                # Emit JF (jump if false)
                self.emit_op(self.JF)
                jf_offset = len(self.bytecode)
                self.write_u64(0)  # Placeholder
                
                # Collect if body
                i += 1
                if_body = []
                brace_count = 1
                while i < len(lines) and brace_count > 0:
                    if '{' in lines[i]:
                        brace_count += lines[i].count('{')
                    if '}' in lines[i]:
                        brace_count -= lines[i].count('}')
                    if brace_count > 0:
                        if_body.append(lines[i])
                    i += 1
                
                # Check for elif/else
                has_else = False
                else_jmp_offsets = []
                
                # Compile if body
                self.compile_block(if_body)
                
                # Handle elif/else
                while i < len(lines):
                    next_line = lines[i].strip()
                    
                    if next_line.startswith('elif '):
                        # Jump over elif from if block
                        self.emit_op(self.JMP)
                        else_jmp_offsets.append(len(self.bytecode))
                        self.write_u64(0)  # Placeholder
                        
                        # Patch JF to here
                        after_if = len(self.bytecode)
                        self.bytecode[jf_offset:jf_offset+8] = struct.pack('<Q', after_if)
                        
                        # Compile elif condition
                        condition = next_line[5:].strip().rstrip('{').strip()
                        self.compile_condition(condition)
                        
                        # New JF for elif
                        self.emit_op(self.JF)
                        jf_offset = len(self.bytecode)
                        self.write_u64(0)
                        
                        # Collect elif body
                        i += 1
                        elif_body = []
                        brace_count = 1
                        while i < len(lines) and brace_count > 0:
                            if '{' in lines[i]:
                                brace_count += lines[i].count('{')
                            if '}' in lines[i]:
                                brace_count -= lines[i].count('}')
                            if brace_count > 0:
                                elif_body.append(lines[i])
                            i += 1
                        
                        self.compile_block(elif_body)
                        
                    elif next_line.startswith('else'):
                        has_else = True
                        
                        # Jump over else from if/elif block
                        self.emit_op(self.JMP)
                        else_jmp_offsets.append(len(self.bytecode))
                        self.write_u64(0)
                        
                        # Patch JF to here
                        after_elif = len(self.bytecode)
                        self.bytecode[jf_offset:jf_offset+8] = struct.pack('<Q', after_elif)
                        
                        # Collect else body
                        i += 1
                        else_body = []
                        brace_count = 1
                        while i < len(lines) and brace_count > 0:
                            if '{' in lines[i]:
                                brace_count += lines[i].count('{')
                            if '}' in lines[i]:
                                brace_count -= lines[i].count('}')
                            if brace_count > 0:
                                else_body.append(lines[i])
                            i += 1
                        
                        self.compile_block(else_body)
                        break
                    else:
                        break
                
                # Patch all jumps to end
                after_all = len(self.bytecode)
                if not has_else:
                    # Patch final JF to here
                    self.bytecode[jf_offset:jf_offset+8] = struct.pack('<Q', after_all)
                
                # Patch all JMPs from if/elif blocks
                for offset in else_jmp_offsets:
                    self.bytecode[offset:offset+8] = struct.pack('<Q', after_all)
                
                continue
            
            # while statement
            elif line.startswith('while '):
                loop_start = len(self.bytecode)
                condition = line[6:].strip().rstrip('{').strip()
                
                # Compile condition
                self.compile_condition(condition)
                
                # Emit JF (jump if false)
                self.emit_op(self.JF)
                jf_offset = len(self.bytecode)
                self.write_u64(0)  # Placeholder
                
                # Collect while body
                i += 1
                while_body = []
                brace_count = 1
                while i < len(lines) and brace_count > 0:
                    if '{' in lines[i]:
                        brace_count += lines[i].count('{')
                    if '}' in lines[i]:
                        brace_count -= lines[i].count('}')
                    if brace_count > 0:
                        while_body.append(lines[i])
                    i += 1
                
                # Compile body
                self.compile_block(while_body)
                
                # Jump back to start
                self.emit_op(self.JMP)
                self.write_u64(loop_start)
                
                # Patch JF to after loop
                after_loop = len(self.bytecode)
                self.bytecode[jf_offset:jf_offset+8] = struct.pack('<Q', after_loop)
                
                continue
            
            # Regular line
            else:
                self.compile_line(line)
            
            i += 1
    
    def compile_function(self, name, lines):
        """Compile a function definition"""
        # Emit JMP to skip over function body
        self.emit_op(self.JMP)
        skip_offset = len(self.bytecode)
        self.write_u64(0)  # Placeholder for jump target
        
        # Record function entry point
        entry_point = len(self.bytecode)
        
        # Compile function body
        for line in lines:
            line = line.strip()
            if line.startswith('fn ') or line == '{' or line == '}':
                continue
            self.compile_line(line)
        
        # Ensure function ends with RET
        # Check if last instruction was already RET
        # For simplicity, always add RET_VOID
        self.emit_op(self.RET_VOID)
        
        # Patch the JMP
        after_function = len(self.bytecode)
        current_pos = len(self.bytecode)
        # Go back and write the jump target
        temp = self.bytecode[skip_offset:skip_offset+8]
        self.bytecode[skip_offset:skip_offset+8] = struct.pack('<Q', after_function)
        
        # Add function to table
        self.functions.append({
            'name': name,
            'entry': entry_point,
            'params': [],
            'isVoid': False,
            'typed': False,
            'ret': 0
        })
    
    def save(self, output_path):
        """Save bytecode to file"""
        with open(output_path, 'wb') as f:
            f.write(self.bytecode)
        print(f"✓ Compiled to {output_path} ({len(self.bytecode)} bytes)")

def main():
    if len(sys.argv) < 2:
        print("Usage: python compile.py <input.mi> [-o output.mic]")
        sys.exit(1)
    
    input_file = sys.argv[1]
    
    # Determine output file
    output_file = None
    if '-o' in sys.argv:
        idx = sys.argv.index('-o')
        if idx + 1 < len(sys.argv):
            output_file = sys.argv[idx + 1]
    
    if not output_file:
        # Default: replace .mi with .mic
        base = os.path.splitext(input_file)[0]
        output_file = base + '.mic'
    
    # Read source
    try:
        with open(input_file, 'r') as f:
            source = f.read()
    except FileNotFoundError:
        print(f"Error: File '{input_file}' not found")
        sys.exit(1)
    
    # Compile
    compiler = MinisCompiler()
    compiler.compile(source)
    compiler.save(output_file)
    
    print(f"✓ Compilation successful!")

if __name__ == '__main__':
    main()
