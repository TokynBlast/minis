# Optimizing

In Minis, and most other programming languages, there is something that happens during compiling... Unless you turn it off optimizations.
Optimizations range from removing things that are never used (called dead code), to making [for loops](./loops.md#for-loops) faster in various ways.
The [CLI](./defines.md#cli) args following this are turned on with `-<arg>`.

## O3

## O2

## O1
This will turn on minor optimizations.

## O0
This is a [CLI](./defines.md#cli) arg that will completely turn off optimizations. Your code will be a direct translation to machine code.

## Ofst
This will optimize the code to be as fast as possible, regardless of size.

## Osz

## comptive
This level of optimizations is for competetive programming, where you need the most speed possible.
It is even more aggresive than Ofst.
It has everything Ofst has, plus avoiding print for formatting, and instead relying on multiple `write()` functions, followed by a `flush()`.
It will also constexpr as much as it safely can.

## usafe-comptive
This is the dangerous version of comptive. It does fast math, and constexpr as much as possible. It doesn't restrict itself as much, and makes more assumptions.
Here is a list of what it does:
```
- inline
- loop unrolling
- calculating as much as possible
- pre-format
- minimal _start()
```
> [!WARN]
> Compiling with this is extremely unsafe, and only reccomended for competetive programmning where speed is ranked higher than safety, accuracy, and/or speed.
