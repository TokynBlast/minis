# Macros

Macros are bits of code processed before anything else is done.
They are parsed as they it goes along. There is no particular order in which they are parsed.

## def
The `#def` macro defines a value that is only alive during compilation.
It is used for branching, like with [`#if`](./macros.md#if), [`#ifdef`](./macros.md#ifdef), [`#ifndef`](./macros.md#ifndef), and so on.
The compiler also defines some values too.
The `#def` macro can be formatted in one of two ways.
Giving it a value,
```minis
#def value 32
```
Or, you can just define it.
```minis
#def value
```

## if
The `#if` macro is used to check for something at compile time. This ranges from hardware, to features, to things in the language.

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

## pragma
The `#pragma` macro gives information about the file to the compiler.

### once
When you append `#pragma once` to the top of the file, it will let the compiler know, this file should be included once throughout the entire source.
If you do something across multiple files, like so...
```minis
// banana.mi
class Banana {
  public {
    int mass = 0;
  }
}
```
```minis
// apple.mi
class Apple {
  public {
    int mass = 0;
  }
}
```
```minis
// fruits.mi
import banana;
import apple;
```
```minis
// main.mi
import fruits;
import apple;
```
You come to a problem. The compiler will yell at you.
```minis-error
Error: main.mi:3:0: Redefining class `¶Apple¶` from apple.mi.
│
├→ Initial definition of class Apple: apple.mi
│  │
│  ╰─┬───────────────────╮
│    │ §// apple.mi§       │
│    │ §class Apple {§     │
│    │   §public {§        │
│    │     §int mass = 0;§ │
│    ╰───────────────────╯
├→ Redefenition of class Apple: main.mi
│  │
│  ╰─┬───────────────────╮
│    │ §// main.mi§        │
│    │ §import fruits;§    │
│    │ §import apple;§     │
│    │ ^~~~~~~~~~~~~     │
│    ╰───────────────────╯
╰→ Maybe you forgot to include `¶#pragma once¶`?
```
The errors in Minis are pretty errors. They are meant to be helpful, and easy to look at.
The `#pragma` macro will fix this, by keeping a list of what has been imported, and making sure to not include twice.

### optimize
The `#optimize` macro allows you to turn on, or off optimizations for a specific block.
If you do `#optimize(off)`, but never do `#optimize(on)` later (within the same file), Minis will warn you.
There is no way to set different optimization levels between files. The different optimization levels are set via args. You can only tell the compiler to not touch certain parts, but that is as far as it goes.

<!--
Say you have...
// file1.mi
int x() {}

file2.mi
x();

if you have file1 be O3, but file2 be O0,
there's a small chance where file1 has x
removed.
-->

### cold
`#pragma cold` will tell the compiler that the following function is rarely used, and should be optimized like so, where the funciton should be as small as possible at the loss of speed.

### hot
`#pragma hot` will tell the compiler that the following function is rarely used, and should be optimized like so, where the funciton should be as fast as possible at the loss of a small size.
