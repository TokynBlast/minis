# Minis Minimal Self-Bootstrapping Compiler

## Overview

This is a minimal version of the minis compiler designed for self-bootstrapping. It supports the core language features needed to write a compiler in minis itself.

## Supported Language Features

### 1. Import Statements
```minis
import moduleName;
```
Imports a module for use in the current file.

### 2. Variable Declarations
```minis
let variableName = expression;
```
Declares a variable with an initial value.

### 3. Function Definitions
```minis
fn functionName(param1, param2) {
  // function body
  return_expression
}
```
Defines a function with parameters. Uses `fn` instead of `def`.

### 4. Basic Arithmetic
```minis
let result = a + b - c * d / e;
```
Supports: `+`, `-`, `*`, `/` with standard precedence.

### 5. Function Calls
```minis
let result = functionName(arg1, arg2);
```

### 6. Literals
- Numbers: `42`, `3.14`
- Strings: `"hello"`, `'world'`
- Identifiers: `variableName`

## Dev Library Functions

The dev library provides essential functions for compiler development:

### Bytecode Emission
- `dev.emitOp(op)` - Emit a single opcode byte
- `dev.emitu64(n)` - Emit unsigned 64-bit number
- `dev.emits64(n)` - Emit signed 64-bit number
- `dev.emitf64(n)` - Emit floating-point literal
- `dev.emitStr(string)` - Emit string literal
- `dev.emitId(name)` - Emit identifier name

### File Operations
- `dev.setFile(path)` - Begin writing bytecode to file
- `dev.close()` - Close the output file

### Label/Jump Management
- `dev.newLabel()` - Create a label ID
- `dev.mark(label)` - Record current position as label target
- `dev.emitJump(label)` - Emit jump instruction
- `dev.patch(label)` - Patch jump to point to label

### Tokenization
- `dev.tokenize(source)` - Convert source to tokens
- `dev.next()` - Consume and return next token
- `dev.peek()` - Look at next token without consuming
- `dev.match(type)` - Check if next token matches type
- `dev.expect(type)` - Match token or throw error

### Token Inspection
- `dev.type(token)` - Return token type enum
- `dev.value(token)` - Return token's literal value
- `dev.isIdent(token)` - Check if token is identifier
- `dev.isNumber(token)` - Check if token is number
- `dev.isString(token)` - Check if token is string

### Parsing
- `dev.parseExpression()` - Parse expression with bytecode emission
- `dev.parsePrimary()` - Parse literals, identifiers, etc.
- `dev.parseUnary()` - Parse prefix operators
- `dev.registerInfix(symbol, fn)` - Register infix operator handler
- `dev.registerPrefix(symbol, fn)` - Register prefix operator handler
- `dev.registerKeyword(type, fn)` - Register keyword handler

### Scope Management
- `dev.enterScope()` - Push new variable scope
- `dev.exitScope()` - Pop current scope
- `dev.lookup(name, depth)` - Look up variable name
- `dev.isLocal(name)` - Check if name exists in current scope

### Error Handling
- `dev.error(message)` - Non-fatal error
- `dev.panic(message)` - Fatal error

### AST Nodes (Optional)
- `dev.newNode(type)` - Create basic AST node
- `dev.setValue(node, name, value)` - Attach metadata
- `dev.addChild(node, child)` - Add child node
- `dev.nodeType(node)` - Return node type

### String Utilities
- `dev.isAlpha(c)` - Check if alphabetical
- `dev.isDigit(c)` - Check if numeric
- `dev.isSpace(c)` - Check if whitespace
- `dev.trim(str)` - Trim whitespace
- `dev.stringContains(str, sub)` - Check substring
- `dev.join(list, sep)` - Join strings with separator

## Usage

### Building
```bash
./build-minimal.sh
```

### Compiling
```bash
./minis-minimal input.mi -o output.mic
```

### Example Program
```minis
import dev;

let x = 42;
let y = 3.14;

fn add(a, b) {
  a + b
}

let result = add(x, 10);
```

## Architecture

1. **MinLexer**: Tokenizes source code into minimal token set
2. **MinParser**: Parses tokens and generates bytecode directly
3. **Dev Library**: Provides runtime functions for compilation
4. **Bytecode**: Uses existing minis VM bytecode format

## Self-Bootstrapping Plan

1. Use this minimal compiler to compile basic minis programs
2. Gradually implement more features in minis itself
3. Eventually replace C++ implementation with minis implementation
4. Continue extending the language while maintaining self-hosting capability

## Files

- `minis-minimal.cpp` - Main compiler executable
- `include/minimal_token.hpp` - Token definitions
- `include/minimal_lexer.hpp` - Lexical analyzer
- `include/minimal_parser.hpp` - Parser and code generator
- `src/dev.cpp` - Dev library implementation
- `test_minimal.mi` - Example program
- `CMakeLists-minimal.txt` - Build configuration