# Introduction

## About
Minis is a low-level, functional language, designed to be as safe and fast as possible.
The Minis language is an explicit and heavily typed language.
The language also aims to speed up development by including things like [the #unroll macro](./macros.md#macros).

## Setup

In Minis, you need an entrance point.
This is the main funciton, which returns any size of a whole number, or a bool.
Returning a void is not allowed, and will error on build.

### Main Function
Minis requires a main function to know what you want to do first. Many languages such as C, C++, Rust, and others do this. There is also the `_start()` function, which is placed in. It runs the `main()` function, and sets up the stack, variables, and other things. You can define your own `_start()` function. To see how to, read [this](./functions.md#making-the-_start-function-yourself).
Here is a simple main function:
```minis
int main() {
  print("Hello, World!\n");
  return 0; // 0 means success
}
```
> [!NOTE]
> The main function must return an int.

## Safety
Like Rust, Minis is meant to be safe. It does this via explicit declaration, garunteed initalization, and a WYSWYG model. What you see, is what you get. It's like C.
Every variable must be explicitly defined.
Minis also uses a combonation of memory ownership, RAII, and manual deletion.
There are also other things, like minimizing dependencies. Minis cannot garuntee other code is safe. It is much easier to manage a single project instead of 443 because of dependency branching.
This decision is based on the attack by Jia Tan.
A lot of the Minis safety comes from Rust ideas. The syntax is both organic and inheritive of other languages.

## Speed
By default, Minis tries to be as safe as possible with the data. It avoids copying unless explicitly told to, uses all features to it's abilities, tries to minimize size of [O](./defines.md#o). How much Minis does, depends what optimization level you have set the program to.
Minis is designed for competetive programming. There are some things you can do to make it even faster too. But by default, Minis doesn't enable these things, as they can cause issues in stability, portability, ammong other things.

## Compile-time Vs. Run-time
Compile time has a lot more restricitons and capabilities than runtime does.
Things like `const`, `static`, and more, are safety features for the compiler. It isn't easy to do the same to the runtime, where the RAM can have an error, and a value gets changed.
