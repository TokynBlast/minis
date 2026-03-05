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
Minis tries to solve the `int* a, b` vs. `int *a, b` problem by removing `int*` in variables entirely.
This both solves typos where `int *a, b` is meant to be typed, and removes confusion of difference by making it constant.
To make all values pointers, you explicitly state which to be pointers.

A pointer in programming does not give you the adderess, it gets the value at the adderess. This is a common misunderstanding. To ge the adderess, you use `&`. For more info, read [this](./variables.md#references).

## References

References are immutable by default.
