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

The def macro can also be expanded to more complex programs, like implementing whole functions, or ways of interacting and storing things.
You can make a list wrapper, or even your own way of doing lists.

## if
The `#if` macro is used to check for something at compile time. This ranges from hardware, to features, to things in the language.
There are other functions you can pair with it, like `defined()`. You can also add checks, or use it to replace `#pragma assert assert`, and make it compile-time.

## ifdef
The `#ifdef` macro will check simply if a macro is defined. It does not care about whether or not it has a value associated, or what that value is.
This is the same as doing `#if defined()`. However, `#ifdef` simplifies things a lot. `#if` can get a lot more complicated, and allows for more specific checks. The `#ifdef` macro simplifies it, and

## ifndef
When `#ifdef` returns true, this will return false.
It checks if it doesn't exist.
A really useful case is...
```minis
#ifndef var
  #def var
#endif
```
That way, if `var` already exist, you don't overwrite it. This is especially useful if you just want the var to exist, and you don't care about the value.
Another piece of code can define the same, where `var` has a value the other person's code needs, but you don't need.

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
The `#pragma` macro gives information about the file to the compiler, or tells the compiler what to do.

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
Error: main.mi:3:0: Redefenition of class `Apple` from apple.mi
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
╰→ Based on origins of each class, did you
   forget to include `¶#pragma once¶`?
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

### depreceated
Using this macro will warn at compile time that a function is depreceated, and give the alternative, new function (if any) to use.
You put it before the function is declared.

### align
This will allow you to align the data in memory to a specific point. It is useful for constraints, or systems where putting something to a specific point will give better performance.

### pack
This will pack the output machine code so that the point on disk to there will match the specified number of bytes.
```minis
int x = 32;
#pragma pack(400)
```
The output program will be ~400 bytes, since defining an int of x as 32 takes up virtually no storage.

### warn
`#pragma warn()` will warn about something at compile time.
It is used to warn about something wile compiling.

### debug
This will emit debugging symbols for a specific part of the code, whether debugging is on or off.

### breakpoint
This will add a yield to the code, so that it pauses at that spot during runtime.
