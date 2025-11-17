#!/usr/bin/env python3
"""
Simple Minis Compiler - String-based compilation to AVOCADO1 bytecode
No tokenizing, no uglifying - just simple string literal matching!
"""

import sys
import struct
import os

# ANSI color codes
RED = '\033[91m'
RESET = '\033[0m'

class CompilerError(Exception):
    """Custom exception for compiler errors"""
    def __init__(self, message, line_number=None, filename=None, column=1):
        self.message = message
        self.line_number = line_number
        self.filename = filename
        self.column = column
        super().__init__(self.format_message())
    
    def format_message(self):
        if self.filename and self.line_number is not None:
            return f"{self.filename}:{self.line_number}:{self.column}: {RED}error{RESET}: {self.message}"
        elif self.line_number is not None:
            return f"Line {self.line_number}: {RED}error{RESET}: {self.message}"
        return f"{RED}error{RESET}: {self.message}"

class MinisCompiler:
    def __init__(self):
        self.bytecode = bytearray()
        self.position = 0
        self.functions = []  # List of {name, entry, params, isVoid, typed, ret}
        self.strings = []
        self.imports = []
        self.used_functions = set()  # Track which imported functions are actually used
        self.plugins = {}  # Track plugin metadata: {module_name: {library, functions}}
        self.in_function = False
        self.current_function_lines = []
        self.current_line_number = 0  # Track current line for error reporting
        self.current_column = 1  # Track column for error reporting
        self.filename = None  # Track source filename for error reporting
        self.errors = []  # Collect non-fatal errors
        self.warnings = []  # Collect warnings
        self.line_map = []  # List of (bytecode_offset, line_number) pairs for debug info
        self.declared_vars = set()  # Track declared variables for compile-time checking
        
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
        # Record line number mapping
        if self.current_line_number > 0:
            self.line_map.append((self.position, self.current_line_number))
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
    
    def load_plugin_manifest(self, module_name):
        """Load plugin manifest file (.plug) for a module"""
        manifest_file = f"{module_name}.plug"
        if not os.path.exists(manifest_file):
            return False  # Not a plugin, might be regular .mic module
        
        plugin_info = {'functions': []}
        current_section = None
        
        with open(manifest_file, 'r') as f:
            for line in f:
                line = line.strip()
                if not line or line.startswith('#'):
                    continue
                
                if line.startswith('[') and line.endswith(']'):
                    current_section = line[1:-1]
                elif current_section == 'plugin' and '=' in line:
                    key, value = line.split('=', 1)
                    plugin_info[key.strip()] = value.strip()
                elif current_section == 'functions':
                    plugin_info['functions'].append(line)
        
        self.plugins[module_name] = plugin_info
        print(f"  Plugin manifest: {module_name} -> {plugin_info.get('library', 'unknown')}")
        
        # Add plugin functions to used_functions and builtins
        for func in plugin_info['functions']:
            prefixed = f"{module_name}_{func}_"
            self.builtins.add(prefixed)
        
        return True
    
    def error(self, message):
        """Record a non-fatal error"""
        error_msg = f"Line {self.current_line_number}: {message}"
        self.errors.append(error_msg)
        print(f"  ERROR: {error_msg}")
    
    def warning(self, message):
        """Record a warning"""
        warning_msg = f"Line {self.current_line_number}: {message}"
        self.warnings.append(warning_msg)
        print(f"  WARNING: {warning_msg}")
    
    def fatal(self, message):
        """Raise a fatal compiler error"""
        raise CompilerError(message, self.current_line_number, self.filename, self.current_column)
        
    def compile_expression(self, expr):
        """Compile an expression and leave result on stack"""
        expr = expr.strip()
        
        # Handle literals FIRST (before checking for operators inside strings!)
        if expr == 'true' or expr == 'false':
            self.emit_op(self.PUSH_B)
            self.write_u8(1 if expr == 'true' else 0)
            return
        elif expr.startswith('"') and expr.endswith('"'):
            self.emit_op(self.PUSH_S)
            self.write_str(expr[1:-1])
            return
        
        # Handle function calls: func() or func(args) or module.func(args)
        # Match pattern: identifier(...) or module.identifier(...)
        if '(' in expr and expr.endswith(')') and not expr.startswith('('):
            paren_idx = expr.find('(')
            if paren_idx > 0:
                before_paren = expr[:paren_idx].strip()
                
                # Check if it's a module-qualified call: module.function
                if '.' in before_paren:
                    parts = before_paren.split('.')
                    if len(parts) == 2:
                        module_name = parts[0].strip()
                        func_name = parts[1].strip()
                        args_str = expr[paren_idx+1:-1].strip()
                        
                        # Track this function usage
                        self.used_functions.add(f"{module_name}_{func_name}_")
                        
                        # Parse and compile arguments
                        args = []
                        if args_str:
                            current_arg = ""
                            depth = 0
                            for char in args_str:
                                if char in '([':
                                    depth += 1
                                    current_arg += char
                                elif char in ')]':
                                    depth -= 1
                                    current_arg += char
                                elif char == ',' and depth == 0:
                                    args.append(current_arg.strip())
                                    current_arg = ""
                                else:
                                    current_arg += char
                            if current_arg.strip():
                                args.append(current_arg.strip())
                        
                        # Compile each argument
                        for arg in args:
                            self.compile_expression(arg)
                        
                        # Emit CALL with module_function_ naming
                        self.emit_op(self.CALL)
                        self.write_str(f"{module_name}_{func_name}_")
                        self.write_u64(len(args))
                        return
                
                # Regular function call (not module-qualified)
                has_ops = any(c in before_paren for c in '+-*/%=<>!& ')
                starts_alpha = before_paren[0].isalpha() if before_paren else False
                # Simple check: function name should not contain operators or spaces
                if before_paren and not has_ops and starts_alpha:
                    func_name = before_paren
                    args_str = expr[paren_idx+1:-1].strip()
                    
                    # Parse arguments - simple split by comma
                    args = []
                    if args_str:
                        # Count parentheses/brackets to handle nested structures
                        current_arg = ""
                        depth = 0
                        for char in args_str:
                            if char in '([':
                                depth += 1
                                current_arg += char
                            elif char in ')]':
                                depth -= 1
                                current_arg += char
                            elif char == ',' and depth == 0:
                                args.append(current_arg.strip())
                                current_arg = ""
                            else:
                                current_arg += char
                        if current_arg.strip():
                            args.append(current_arg.strip())
                    
                    # Push arguments onto stack
                    for arg in args:
                        self.compile_expression(arg)
                    
                    # Emit CALL instruction
                    self.emit_op(self.CALL)
                    if func_name in [f['name'] for f in self.functions]:
                        self.write_str(func_name + '_')
                    else:
                        self.write_str(func_name)
                    self.write_u64(len(args))
                    return
        
        # Handle arithmetic: a + b, a - b, a * b, a / b
        # Only split on operators that are outside of parentheses
        for op_str, op_code in [('+', self.ADD), ('-', self.SUB), ('*', self.MUL), ('/', self.DIV)]:
            depth = 0
            for i, char in enumerate(expr):
                if char in '([':
                    depth += 1
                elif char in ')]':
                    depth -= 1
                elif char == op_str and depth == 0 and i > 0:
                    # Found operator at top level, not at start
                    left = expr[:i].strip()
                    right = expr[i+1:].strip()
                    if left and right:
                        # Compile left operand
                        self.compile_expression(left)
                        # Compile right operand
                        self.compile_expression(right)
                        # Emit operator
                        self.emit_op(op_code)
                        return
        
        # Handle list literals: [1, 2, 3]
        if expr.startswith('[') and expr.endswith(']'):
            list_content = expr[1:-1].strip()
            if list_content:
                # Parse list elements with depth tracking
                elements = []
                current_elem = ""
                depth = 0
                for char in list_content:
                    if char in '([':
                        depth += 1
                        current_elem += char
                    elif char in ')]':
                        depth -= 1
                        current_elem += char
                    elif char == ',' and depth == 0:
                        elements.append(current_elem.strip())
                        current_elem = ""
                    else:
                        current_elem += char
                if current_elem.strip():
                    elements.append(current_elem.strip())
                
                # Push each element
                for elem in elements:
                    self.compile_expression(elem)
                
                # MAKE_LIST with count
                self.emit_op(self.MAKE_LIST)
                self.write_u64(len(elements))
            else:
                # Empty list
                self.emit_op(self.MAKE_LIST)
                self.write_u64(0)
            return
        
        # Handle indexing: var[index]
        if '[' in expr and ']' in expr:
            bracket_start = expr.find('[')
            bracket_end = expr.find(']')
            base_var = expr[:bracket_start].strip()
            index_expr = expr[bracket_start+1:bracket_end].strip()
            
            self.emit_op(self.GET)
            self.write_str(base_var)
            
            if index_expr.isdigit():
                self.emit_op(self.PUSH_I)
                self.write_f64(float(int(index_expr) - 1))
            else:
                self.compile_expression(index_expr)
                self.emit_op(self.PUSH_I)
                self.write_f64(1.0)
                self.emit_op(self.SUB)
            
            self.emit_op(self.INDEX)
            return
        
        # Handle numbers and variables (final fallback)
        # Try number first, then variable reference
        try:
            num_val = float(expr)
            self.emit_op(self.PUSH_I)
            self.write_f64(num_val)
        except ValueError:
            # Variable reference - check if it's been declared
            if expr not in self.declared_vars and expr not in self.builtins:
                self.fatal(f"Use of undeclared variable '{expr}'")
            self.emit_op(self.GET)
            self.write_str(expr)

    def compile_line(self, line):
        """Compile a single line using string literal comparison"""
        line = line.strip()
        
        # Skip empty lines and comments
        if not line or line.startswith('//'):
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
                    
                    # Track this function usage
                    self.used_functions.add(f"{module_name}_{func_name}_")
                    
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
        
        # Print statements: print(expression) - unified handling
        if line.startswith('print(') and line.endswith(')'):
            arg = line[6:-1].strip()  # Extract content between print( and )
            
            # Compile the argument expression
            self.compile_expression(arg)
            
            # CALL for print
            self.emit_op(self.CALL)
            self.write_str("print")
            self.write_u64(1)
            return

        # Variable assignment (without let): x = 42
        if '=' in line and not line.startswith('let ') and not line.startswith('if ') and not line.startswith('while ') and '==' not in line and '!=' not in line and '<=' not in line and '>=' not in line:
            parts = line.split('=', 1)
            var_name = parts[0].strip()
            value = parts[1].strip().rstrip(';')
            
            # Use compile_expression to handle the value
            self.compile_expression(value)
            
            # SET opcode - reassign variable
            self.emit_op(self.SET)
            self.write_str(var_name)
            return

        # Variable declarations: let x = 42
        if line.startswith('let ') and '=' in line:
            parts = line[4:].split('=', 1)  # Split on first = only
            var_name = parts[0].strip()
            value = parts[1].strip().rstrip(';')
            
            # Track this variable as declared BEFORE compiling expression
            # This prevents errors when the expression is complex
            self.declared_vars.add(var_name)
            
            # Check for list literal FIRST: [1, 2, 3] or ["a", "b"]
            if value.startswith('[') and value.endswith(']'):
                list_content = value[1:-1].strip()
                if list_content:
                    # Parse list elements
                    elements = [elem.strip() for elem in list_content.split(',')]
                    # Push each element
                    for elem in elements:
                        if elem.startswith('"') and elem.endswith('"'):
                            self.emit_op(self.PUSH_S)
                            self.write_str(elem[1:-1])
                        elif elem == 'true' or elem == 'false':
                            self.emit_op(self.PUSH_B)
                            self.write_u8(1 if elem == 'true' else 0)
                        else:
                            # Try to parse as number
                            try:
                                num_val = float(elem)
                                self.emit_op(self.PUSH_I)
                                self.write_f64(num_val)
                            except ValueError:
                                # Variable reference
                                self.emit_op(self.GET)
                                self.write_str(elem)
                    # MAKE_LIST with count
                    self.emit_op(self.MAKE_LIST)
                    self.write_u64(len(elements))
                else:
                    # Empty list
                    self.emit_op(self.MAKE_LIST)
                    self.write_u64(0)
            else:
                # Use compile_expression for everything else
                self.compile_expression(value)
            
            # DECL opcode - declare and store
            # DECL format: string(varname) + u64(type) where 0xECull means infer type
            self.emit_op(self.DECL)
            self.write_str(var_name)
            self.write_u64(0xEC)  # Type inference marker
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
        
        # If we get here, the line is not recognized
        # Check if it looks like an invalid statement
        if line and not line.isspace():
            self.fatal(f"Unrecognized statement: '{line}'")
    
    def compile(self, source_code):
        """Compile source code to AVOCADO1 bytecode"""
        # Split into lines
        lines = source_code.split('\n')
        
        # First pass: gather imports and track function usage
        self.current_line_number = 0
        for line in lines:
            self.current_line_number += 1
            line_stripped = line.strip()
            if line_stripped.startswith('import '):
                module_name = line_stripped[7:].strip()
                if module_name not in self.imports:
                    self.imports.append(module_name)
                    print(f"  Imported: {module_name}")
            # Track module function calls
            elif '.' in line_stripped and '(' in line_stripped:
                parts = line_stripped.split('.')
                if len(parts) >= 2:
                    module_name = parts[0].strip()
                    func_part = '.'.join(parts[1:])
                    paren_idx = func_part.find('(')
                    if paren_idx != -1:
                        func_name = func_part[:paren_idx].strip()
                        self.used_functions.add(f"{module_name}_{func_name}_")
        
        # Reset line counter
        self.current_line_number = 0
        
        # Reserve space for header
        # Header: magic(8) + table_offset(8) + fn_count(8) + entry_point(8) + line_map_offset(8) = 40 bytes
        header_size = 40
        code_start = header_size
        
        # Reset and skip header
        self.bytecode = bytearray([0] * header_size)
        self.position = header_size
        
        # Load and link imported modules BEFORE compiling our code
        imported_functions = []
        for module_name in self.imports:
            # Check if it's a plugin first
            if self.load_plugin_manifest(module_name):
                # It's a plugin - functions are registered, no bytecode to link
                continue
            
            bytecode_file = f"{module_name}.mic"
            if os.path.exists(bytecode_file):
                print(f"  Linking {bytecode_file}...")
                try:
                    with open(bytecode_file, 'rb') as f:
                        # Read magic
                        magic = f.read(8).decode('ascii')
                        if magic != 'AVOCADO1':
                            self.error(f"Invalid magic in {bytecode_file}")
                            continue
                    
                        # Read header
                        mod_table_offset = struct.unpack('<Q', f.read(8))[0]
                        mod_fn_count = struct.unpack('<Q', f.read(8))[0]
                        mod_entry_main = struct.unpack('<Q', f.read(8))[0]
                        mod_line_map_off = struct.unpack('<Q', f.read(8))[0]  # Read but don't use
                        
                        # Read code section (from header end to table start)
                        f.seek(40)  # Skip header (magic + 4 u64s = 40 bytes)
                        code_size = mod_table_offset - 40
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
                            
                            # Only include if this function is actually used
                            if prefixed_name not in self.used_functions:
                                print(f"    Skipped (unused): {prefixed_name}")
                                continue
                            
                            imported_functions.append({
                                'name': prefixed_name,
                                'entry': adjusted_entry,
                                'isVoid': is_void,
                                'typed': typed,
                                'ret': ret_type,
                                'params': param_count
                            })
                            print(f"    Linked: {prefixed_name} @ {adjusted_entry}")
                except Exception as e:
                    self.error(f"Failed to link {bytecode_file}: {str(e)}")
                    continue
            else:
                self.warning(f"{bytecode_file} not found")
        
        # Now compile this module's code
        # Parse and handle control flow structures
        main_lines = []  # Store as (line_number, line_text) tuples
        i = 0
        while i < len(lines):
            line = lines[i].strip()
            line_num = i + 1  # 1-indexed line numbers
            
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
                main_lines.append((line_num, lines[i]))
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
        
        # Write line number debug info after function table
        # Format: u64(count) followed by pairs of u64(offset), u32(line)
        line_map_offset = len(self.bytecode)
        self.write_u64(len(self.line_map))
        for offset, line in self.line_map:
            self.write_u64(offset)
            self.write_u32(line)
        
        # Write plugin table after line map
        # Format: u64(plugin_count) followed by plugin entries
        # Each entry: str(module_name) + str(library_path)
        plugin_table_offset = len(self.bytecode)
        self.write_u64(len(self.plugins))
        for module_name, plugin_info in self.plugins.items():
            self.write_str(module_name)
            self.write_str(plugin_info.get('library', ''))
        
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
        # Line map offset
        header.extend(struct.pack('<Q', line_map_offset))
        
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
    
    def compile_block(self, lines):
        """Compile a block of lines with control flow support"""
        i = 0
        while i < len(lines):
            # Handle both tuples (line_num, text) and plain strings
            if isinstance(lines[i], tuple):
                line_num, line_text = lines[i]
                self.current_line_number = line_num
                line = line_text.strip()
            else:
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
                    # Get line text for brace counting
                    line_text = lines[i][1] if isinstance(lines[i], tuple) else lines[i]
                    if '{' in line_text:
                        brace_count += line_text.count('{')
                    if '}' in line_text:
                        brace_count -= line_text.count('}')
                    if brace_count > 0:
                        if_body.append(lines[i])  # Keep as tuple if it is one
                    i += 1
                
                # Check for elif/else
                has_else = False
                else_jmp_offsets = []
                
                # Compile if body
                self.compile_block(if_body)
                
                # Handle elif/else
                while i < len(lines):
                    next_line_text = lines[i][1] if isinstance(lines[i], tuple) else lines[i]
                    next_line = next_line_text.strip()
                    
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
                            line_text = lines[i][1] if isinstance(lines[i], tuple) else lines[i]
                            if '{' in line_text:
                                brace_count += line_text.count('{')
                            if '}' in line_text:
                                brace_count -= line_text.count('}')
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
                            line_text = lines[i][1] if isinstance(lines[i], tuple) else lines[i]
                            if '{' in line_text:
                                brace_count += line_text.count('{')
                            if '}' in line_text:
                                brace_count -= line_text.count('}')
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
                    line_text = lines[i][1] if isinstance(lines[i], tuple) else lines[i]
                    if '{' in line_text:
                        brace_count += line_text.count('{')
                    if '}' in line_text:
                        brace_count -= line_text.count('}')
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
        # Print error/warning summary
        if self.warnings:
            print(f"\n⚠ {len(self.warnings)} warning(s):")
            for warning in self.warnings:
                print(f"  {warning}")
        
        if self.errors:
            print(f"\n✗ {len(self.errors)} error(s):")
            for error in self.errors:
                print(f"  {error}")
            print(f"\nCompilation completed with errors. Output may be invalid.")
        
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
    compiler.filename = input_file
    try:
        compiler.compile(source)
        compiler.save(output_file)
        print(f"✓ Compiled to {output_file} ({len(compiler.bytecode)} bytes)")
        print(f"✓ Compilation successful!")
    except CompilerError as e:
        print(str(e), file=sys.stderr)
        sys.exit(1)

if __name__ == '__main__':
    main()
