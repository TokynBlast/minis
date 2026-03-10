# Introduction

## About

Minis is a language that has gone through multiple changes, from target to runtime to purpose. It has evolved and settled into a low-level, functional, somewhat simple language designed to be safe and as fast as possible with everything it does. From not relying on the slow stdout, to use Fortran style math internally.
The Minis language is very explicit and heavily typed language.
The language also aims to speed up development by including things like [the #unroll macro](./macros.md#macros).

## Setup

In Minis, you need an entrance point.
This is the main funciton, which returns any size of a whole number, or a bool.
Returning a void is not allowed, and will error on build.

### Main Function
Minis must know the entrance to your code. It will not assume you want to start at the top. You must be explicit. To make an entry, you define a main function. In the example below, I will make a main function to print "Hello, World!", then exit (return) on true. If you replace it with a whole number, this is just zero.

```minis
bool main() {
  print("Hello, World!\n");
  return true;
}
```
> [!NOTE]
> It is usually recommended to return an `int` type over a set size so you can utilize the full range of return error numbers.

## Safety

Like Rust, Minis is meant to be safe. It does this via explicit declaration, garunteed initalization, and a WYSWYG model. What you see, is what you get.
Every variable must be explicitly defined.
Minis also uses a combonation of memory ownership, RAII, and manual deletion.
There are also other things, like minimizing dependencies. Minis cannot garuntee other code is safe. It is much easier to manage a single project instead of 443 because of dependency branching.
This decision is based on the attack by Jia Tan.
A lot of the Minis safety comes from Rust ideas. The syntax is both organic and inheritive of other languages.

## Speed

By default, Minis tries to be as safe as possible with the data. It avoids copying unless explicitly told to, uses all features to it's abilities, tries to minimize size of [O](./defines.md#o). How much Minis does, depends what optimization level you have set the program to.
Minis is designed for competetive programming. There are some things you can do to make it even faster too. But by default, Minis doesn't enable these things, as they can cause issues in stability, portability, ammong other things.
