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

### flush
Once you're done with writing, you call flush.
The will clear the buffer

### Write or Print?
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
