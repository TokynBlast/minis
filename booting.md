The boot strap version increases speed by implementing some parts of an ISA (Instruction Set Arcitetchure).<br>
Rather than search all of the opcode values, we only check for types (Currently). This only adds seven new opcodes (29 bytes), to check only seven opcodes then determine what next, rather than look over 38 every time.<br>
It also introduces libraries, no need for "let" to declare a variable, and the ability to do var++, alongside ++var.<br>
Previous versions used a long long for int. With this one, it lets the system handle sizing, and adds another int size.<br>
The MVME (Minis Virtual Machine Engine) used to have error handling. This removes that, since majority of errors are caught by the compiler, especially the ones it looked for.<br>
An error in the MVME now also stops the program, compared to what it originally did.<br>
There's also working imports, which have been simplified when compared to previous attempts.<br>
It also has a reduced size of ~87.5%, provided by byte packing and using ISA rules.<br>
This version also has different opcodes, along with registers, which allowed for a decreased size.