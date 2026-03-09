# Variables

## Initalization

In Minis, a variable is garunteed to be initalized upon declaration.
This means that if you make a variable, even if you set no value, it has grabbed the memory to put something there.
A string will grab

## Ownership <!-- TODO: Change this to explain how ownership works, not what it does internally -->

In Minis, if you move a variable, or set another variable to that value, minis by default does name moving.
Let's look at this:
```minis
int x = 32;
int z = x;
```
If you try to use `x` afterwards, Minis will error.
The reason Minis does this, is for complex types (string, list, dictionary, etc.), where a copy is O(n). This means the more in it, the longer it takes.
To keep Minis fast, it will simple change out `x` with `z`.
Internally, this is just pointer switching.
`x` no longer exist, but `z` will not only take over, but also live in the exact same place.

## Pointers

Pointers use the common `*`.
When defining multiple variables of the same type, you can do `int* a, b`. Both a and b will be pointers.
If you want just `a` to be a pointer, you would move the * to be next to `a` like so: `int *a, b`.

A pointer points to a value in an adderess. It does not point to the adderes itself. To point to the adderess, you would use a [reference](./variables.md#references).

By default, Minis uses smart pointers.
To use a raw pointer, you can make it like so:
```minis
usafe int* a() {}
```

## References
References are immutable by default.
They are like a pointer, but use `&` instead. Unlike a pointer, they only give the adderess.

## Types
Minis has many, many types. This will walk you through them all, with examples on how to use them, and what they can do or hold.

### int
`int` stands for integer. During compiling, it will become either 32-bit, or a 64-bit integer.

### void
The `void` type can change meaning depending on whether it is a function, or a value.
In a value, it can be any type. In a function, it means there will be no returned value.
Further, a `void*` (void pointer), means any return type.

### _t type
The `_t` type is probably one of the most important types.
`_t` on it's own is useless. But, when combined with things like `time`, `size`, etc., it can change size depending on the system.

#### time
The `time_t` type is used for when holding a time value, which will ignore the max size the CPU can handle nativley and force a 64-bit signed integer. This is to avoid your code having a problem with the 2038 issue with 32-bit code.
It cannot be converted to any other other type, unless it is a copy.

#### size
The `size_t` type will set the number to be the largest size of integer the CPU can handle natively and naturally.
It will ignore [SIMD](./defines.md#)
