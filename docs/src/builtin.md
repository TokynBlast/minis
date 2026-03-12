# Builtins
Minis has quite a few builtin functions to aid in development.

## Printing
Printing is for outputting data to the terminal.

### print
The print function takes `**kwargs` number of variables. The syntax `**kwargs` is from Python. It passes a list of args.

### println
This is the same as print, but it puts `\n` after everything else.
`\n` is newline. In reality, this expands to `\r\t`, where cursor is sent to the left and sent down by one.

### write
This will add a string to somewhere, where it will wait to be printed.
During this time, the string is considered to be borrowed, so you cannot delete the string before flushing.
Knowing that all values passes are references is really important, and often neccesary.
It will prevent you from trying to delete a variable that is in the write buffer.
The only way to get past this, is to force write to own it.

### flush
Once you're done with writing, you call flush.
The will clear the buffer

<h3>Write or Print?</h3>
This difference can help, especially with competetive programming.
Print is instant. For when you have a few things to print, or can't wait.
```minis
print("Hello, World!");
```

Write is better if you have a lot of stuff to print, but don't need it instantly.
It is much lighter, and can *sometimes* be better for formatting.

```minis
write("Hello,");
write(" World");
write("!");
write("\n");
flush();
```
The reason there is no & (reference) next to the strings, is because the write function's bufer must own it. There is nobody else to own it, so write is forced to.

## len
This will get you the length of an input string or list. It is currently slow because of how it works. It walks the item provided until the kernel stops it and says that's outside of  it's memory.

## IsAlpha
Returns a `bool`. It will tell you if the character(s) provided are roman characters. (A-Z, a-z).

## IsNumeric
Returns a `bool`. Will determine if the provided character(s) are numbers. (0-9).

## IsAlphaNumeric
Returns a `bool`. It will tell you if the character(s) provided are numbers and/or roman characters. (A-Z, a-z, 0-9).

## IsLetter
Returns a `bool`. This funciton will tell you if the provided character(s) are a letter of any language's alphabet that is supported in standard UTF-8.

## exit
No return. Takes a single int. It will unwrap the stack, deallocate everything, then return the given value to the system.
