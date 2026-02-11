# Getting started
Since this is C++, not Rust, we don't have safety built-in.<br>
We must manualy implement it.

### C Runtime
Not using the C runtime is a bit hard to do. But, it is possible.

### Strings
Strings are stored as pointers to the value.<br>
This reduces size, and allows expansion of a string to happen more easily.

### Lists
Non-trivial types are held raw.<br>
Strings, are held via a pointer.

### Style
Indentation uses double spaces<br>
```
while (true)
{
  print("Hi!");
}
```
Functins use
