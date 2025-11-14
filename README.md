# Minis Bootstrap Compiler

> **A simple bridge for bootstrapping - meant to be crossed, not lived on.**

This branch contains a **bootstrap compiler** for the Minis programming language. It's a temporary stepping stone - like a bird crossing a bridge that's falling apart. The bird doesn't stay on the bridge; it uses it to get where it needs to go. Even if the bridge breaks, the bird is already gone.

## What is This?

This is a **line-based Python compiler** (`compile.py`) that generates AVOCADO1 bytecode for a **C++ VM** (originally used for Minis Proto). It uses simple string matching - no tokenization, no parsing trees, just literal string comparisons. Each line is treated as a complete statement.

### Purpose

- **Bootstrapping**: Get Minis to a point where it can compile itself
- **Reference**: The C++ compiler code (`.hpp` and `.cpp` files) is kept here purely for reference, allowing me to check what the original Minis implementation did and ensure correctness
- **Developer Launchpad**: A starting point for other developers to build from, improve, or experiment with

This isn't legacy code gathering dust - it's an **active bridge for developers** to cross and build upon! 🚀

## Architecture

### Components

- **`compile.py`**: Python-based compiler (string matching, no tokenizer)
- **`runFront.cpp`**: C++ VM bytecode executor
- **`vm.cpp`**: Virtual machine implementation
- **`dev.mi`**: Development library with compiler building blocks
- **Legacy C++ Files**: Original Minis compiler code (kept for reference only)

### How It Works

#### Line-Based Compilation

Every line is a discrete statement:

```minis
let x = 42              // Variable declaration
print("Hello")          // Function call
if x > 10 {             // Control flow
  print(x)
}
```

The compiler uses **literal string matching**:
- `line.startswith('let ')` → variable declaration
- `line.startswith('if ')` → conditional
- `line.startswith('import ')` → module import
- etc.

#### Static Linking

When you `import dev`, the compiler:

1. **Reads `dev.mic`** (compiled bytecode file)
2. **Copies the entire code section** into your output
3. **Adjusts function entry points** to match the new positions
4. **Prefixes function names**: `dev.function()` becomes `dev_function_`
5. **Merges function tables** into the output file

**What is Static Linking?**  
Instead of loading modules at runtime, all imported code is physically copied into your compiled `.mic` file. This creates larger files but is simpler to implement and doesn't require a dynamic module loader in the VM.

Example: If `dev.mic` is 5KB and you import it, your output grows by ~5KB.

## Features

### Supported Syntax

- **Variables**: `let x = value`
- **Booleans**: `true`, `false`
- **Numbers**: `42`, `-17`, `3.14`
- **Strings**: `"Hello, world!"`
- **Lists**: `[]` (empty lists)
- **Functions**: `fn name() { ... }`
- **Control Flow**:
  - `if condition { ... }`
  - `elif condition { ... }`
  - `else { ... }`
  - `while condition { ... }`
- **Comparisons**: `==`, `!=`, `<`, `<=`, `>`, `>=`
- **Module Imports**: `import dev`
- **Module Calls**: `dev.function(args)`

### Built-in Functions

`print`, `abs`, `neg`, `range`, `max`, `min`, `sort`, `reverse`, `sum`, `input`, `len`, `split`, `upper`, `lower`, `round`, `random`, `open`, `close`, `write`, `read`, `flush`

### The `dev` Module

The `dev.mi` file contains compiler development utilities (all stubs that return placeholder values):

#### File Operations
- `setFile(filename)` - Set current file for operations
- `closeFile()` - Close current file
- `writeBytes()` - Write bytes to file
- `readBytes()` - Read bytes from file
- `flushFile()` - Flush file buffer

#### Bytecode Emission
- `emitOp()` - Emit an opcode
- `emitu64()` - Emit unsigned 64-bit integer
- `emits64()` - Emit signed 64-bit integer
- `emitf64()` - Emit 64-bit float
- `emitStr(str)` - Emit string
- `emitId()` - Emit identifier

#### Output Management
- `openOutput()` - Open output file
- `closeOutput()` - Close output file

#### Control Flow Helpers
- `newLabel()` - Create new label for jumps
- `mark()` - Mark current position
- `emitJump()` - Emit jump instruction
- `patch()` - Patch jump target

#### Tokenization & Parsing
- `tokenize()` - Tokenize input
- `next()` - Get next token
- `peek()` - Peek at next token
- `match()` - Match token type
- `getType()` - Get token type
- `isIdent()` - Check if identifier
- `isString()` - Check if string
- `parsePrimary()` - Parse primary expression

#### Scope & Variables
- `enterScope()` - Enter new scope
- `lookup()` - Lookup variable

#### Utilities
- `error()` - Report error
- `isAlpha()` - Check if alphabetic
- `isDigit()` - Check if digit
- `isSpace()` - Check if whitespace
- `stringContains()` - String contains check

## ⚠️ Important Warnings

### This is Largely Untested

This compiler is a **proof-of-concept bootstrap**. It works for basic cases but hasn't been battle-tested. Expect bugs, edge cases, and limitations. The goal is "good enough to bootstrap," not production-ready.

### Error Reporting is Broken

**All errors report as `:1:1:`** regardless of where they actually occur. The VM doesn't track source locations properly, so every error will point to line 1, character 1. You'll need to debug by process of elimination or adding print statements.

### Known Limitations

- No arithmetic expressions beyond simple comparisons
- No function parameters (all functions are 0-arity)
- No return value handling in variable assignments from module functions
- Very basic expression parsing
- No type checking
- No optimization

## Contributing & Improving

**This code is meant to be improved!**

Feel free to:
- ✅ Fix bugs you encounter
- ✅ Add missing features
- ✅ Improve error reporting
- ✅ Optimize the compiler
- ✅ Expand language capabilities
- ✅ Refactor the string-matching approach
- ✅ Add proper tokenization/parsing
- ✅ Implement dynamic module loading
- ✅ Add function parameters
- ✅ Build arithmetic expression support

This isn't locked-down legacy code - it's a **foundation for experimentation**. Make it better! Break it! Learn from it! Use it as a stepping stone to something greater.

## Usage

### Compile and Run

```bash
# Compile a Minis file
python3 compile.py program.mi

# Run the compiled bytecode
./runFront program.mic

# Compile module then import it
python3 compile.py dev.mi
python3 compile.py main.mi  # main.mi can now: import dev
./runFront main.mic
```

### Example Program

```minis
import dev

let x = true
if x {
  print("x is true!")
}

let count = 5
if count < 10 {
  print("Count is less than 10")
}

dev.isAlpha()
print("Done!")
```

## Building the VM

```bash
# Compile the VM
g++ -std=c++23 -o runFront runFront.cpp vm.cpp err.cpp

# Or use the existing build (if available)
./runFront
```

## Philosophy

> "Perfect is the enemy of done."

This compiler is **intentionally simple**. It's not elegant, it's not complete, and it's definitely not bug-free. But it works well enough to bootstrap the next stage of Minis development, and that's all it needs to do.

Think of it as a **disposable rocket stage** - it gets you to orbit, then you let it fall away. 🚀

---

**Remember**: This is a bridge. Cross it, don't live on it. And if you see a way to make it better for the next bird, please do! 🐦
