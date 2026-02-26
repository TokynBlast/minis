> [!WARNING]
> The compiler is being developed!

# Minis
## What is Minis?
Minis is a language, designed to be low-level, but also high level.<br>
Along with expanding and adding many features to aaccelrate building.<br>
Things like a tribool, variable circuits, personal, variableless for loops, and much more!<br>
Like a toolbox, you don't pull out everything in there. You only pull out what you need.<br>

## Contributions
To contribute, make a branch, make the changes, and make a pull request explainging what you changed.

# Macros

## #roll
> [!WARN]
> The #roll macro is experimental, and may change.

Minis has something called the rolling macro.
It allows repetetive code to 
```minis
#roll (int var1 , str var2) for [
  (32, "hi"),
  (75, "no")
] in "print({var1}, {var2});"
#endroll
```

## #if and #if variants

### #if
```#if``` is a compile-time if statement. It is evaluated while compiling.<br>
Unlike ```constexpr```, it is not explicitly related to a variable. But rather, a block of code.<br>
You can use it to do different things based on the CPU, OS, etc.<br>

### #ifdef
This one works with other macros, and is generally used more often for targeting things based on CPU, OS, etc.<br>
It doesn't care about a value, only that it exist.<br>
You can use it in pair with things like #def

# Special Scopes

Minis has the ability, to make scopes like actual inline functions, that you can operate on like a variable or something.<br>
They can also be used like a regular scope.
```
(str){return 3;}
```
This will become ```"3"```.<br>
There are other purposes, such as using it for testing.<br>
They act as functions, but you can access outside variables from inside there.

# Circuit Variables
In Minis, a circuit variable is logic, that becomes a variable.<br>
Take the following if else:
```minis
import fs;
FILE* file = {
  fs::exist("text.txt") -> fs::open("text.txt"),
  fs::exist("config.toml") -> fs::open("config.toml")
};
print(file.fs::read(0));
```
You can even embed logic within the circuit! Just make another scope :)

# Variable Types
Minis introduces a few more variables types.<br>
Here is their guide:
```minis
u8 - unsigned 8-bit integer
u16 - unsigned 16-bit integer
u32 - unsigned 32-bit integer
u64 - unsigned 64-bit int
u128 - unsigned 128-bit integer
u256 - uinsigned 256-bit integer

i8 - signed 8-bit integer
i6 - signed 16-bit integer
i32 - signed 32-bit integer
i64 - signed 64-bit int
i128 - signed 128-bit integer
i256 - signed 256-bit integer

int - signed 32-bit int, or 64-bit int; determined by CPU
<size> float - a float, where <size> is size of bit to store it in

__<u|i><size>__ - a custom size integer of either signdness

bool - true or false
tribool - true, false, or nil

list - list of values
dict - map of key and value pairs

str - string of characters
char - single character
```
> [!NOTE]
> If you do a custom size integer, the larger you go, the longer it may take to compile...
