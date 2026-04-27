# Functions

A Minis function can get quite complex, so we'll break it up into manageable parts.

## Making A Function
To make a function in Minis, you need the return type and a name for the function.
A funciton does not need logic, as long as it has a void return type (no return, different from `void*`). If it has a return type, it must have logic.
The logic cannot just be `return;`. Minis expects the return to be explicit. It will not make a guess on what to return.

To take input for a function, you include the paramaters after the name, like most languages.
```minis
int add(int a, int b) {}
```
Inside the function, there is now `a` and `b` you can use.

### Setting a Default
To set a default for a value, you do it like initalizing a variable anywhere else, like so...
```minis
int add (int a=1, int b=1) {}
```
Now, if `a` or `b` aren't provided, it will just use `1` and `1`.
If you want to set b, but use the default for a, you would do...
```minis
add(b=3);
```
It will use default `a` (`1`), and set `b` to `3`, resulting in 4.

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
The function modifier name is placed before the type returned by the function.

### constexpr
This will make it so the function's output is determined at compile-time. This takes some processing power off of the person who runs the binary later, and *usually* results in smaller binaries by removing a funciton, and it's calls, replacing it with the output.
```minis
constexpr int add(int a, int b) {
  return a + b;
}
print(add(1, 3));
```
Rather than calling add, it will just print the value `4`.

## Return Types
Function return types are required. They tell the program what to expect.
The only way to tell the program to expect anything at all. Whether it be a string, an int, etc., is with `void*`.
Though, `void*` is really dangerous, as accepting *ANY* type can lead to corruption or misuse of data.


## Making The _start() Function Yourself
It is simple to make it. You just need to define `_start()`, and it will be called.
""

### Convert On Pass - COP
Minis has a special feature for functions.
You can tell a function, to try and convert the type if it's not already in the expected type! This is called `Convert On Pass`.
Lets take the following function:
```minis
int add(int a, int b)
{
    returb a + b;
}
```
If you were to call...
```minis
float a = 32.0;
float b = 4.0;
float value = add(a, b);
```
It would error, because it expected an int.
However, we can slightly modify it to auto convert:
```minis
##int add(int a, int b)
{
    return a + b;
}
```
The `##` at the beginning tells it to convert on pass.
Double `#` tells it to convert them all.
A single `#` will only convert the attached pass or return.
