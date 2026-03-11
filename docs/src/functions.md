# Functions

A Minis function can get quite complex, so we'll break it up into manageable parts.

## Bare Function

A bare funciton in Minis is one with return type, name, and logic.
Functions have an extra return type that is only available with functions.

## Pointers

Returning a pointer from a function in Minis is simple.
```minis
int* add(int a, int b) {
  return a + b;
}
```

## Return Modifiers

In Minis, return modifiers are placed afterwards, and modify how the returned value works.
You can add the following:
```minis
const
static
```

Here is an add function example, where the value returned cannot be changed after the function returns a value.
```minis
int add(int a, int b) -> static {
  return a + b;
}
```

To read more, check out [here](./variables.md#modifiers).

## Function Modifiers
Unlike *return* modifiers, a funciton modifier will affect how the funciton itself is handled.
This list is quite short...
```minis
inline
alwaysinline
constexpr
```

## Return Types
Function return types are required. They tell the program what to expect.
The only way to tell the program to expect anything at all. Whether it be a string, an int, etc., is with `void*`.
Though, `void*` is really dangerous, as accepting *ANY* type can lead to corruption or misuse of data.
