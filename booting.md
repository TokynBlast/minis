The boot strap version increases speed by implementing some parts of an ISA (Instruction Set Arcitetchure).\n
Rather than search all of the opcode values, we only check for types (Currently). This only adds seven new opcodes (29 bytes), to check only seven opcodes then determine what next, rather than look over 38 every time.\n
It also introduces libraries, no need for "let" to declare a variable, and the ability to do var++, alongside ++var.
