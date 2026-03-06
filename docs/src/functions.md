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
onceset
const
static
constexpr
```

Here is an add example, where it is determined at compile time, and the value is stuck in place of where it was called.
```minis
int add(int a, int b) -> constexpr {
    return a + b;
}
```

## Return Types
Functions have all of the regular types from u8-i512.
Plus some extras, like void.
