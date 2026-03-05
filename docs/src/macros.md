# Macros

Macros are bits of code processed before anything else is done.
They are parsed as they it goes along. There is no particular order in which they are parsed.

## def

## if

## ifdef

## ifndef

## unroll

The rolling macro is used to write repetetive code that gets unrolled during compile time, making it more maintainable.
The unrolling macr is useful in all sorts of places. From embedding directly into variables, to making macros, to repeating loops.

The `unroll` macro is formatted like so:
```minis
#unroll (<variable name>) for [<variables>] in "<code>"
```

If there is one variable name, Minis is smart enough to know it doesn't need to use parentheses to seperate data.
Here is an example to print the pairs "Hello," and "World!":
```minis
#unroll (wordOne, wordTwo) for [("Hello,", "World!")] in "print("{wordOne} {wordTwo}");"
```
