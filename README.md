> [!WARNING]
> This refrences things not fully implemented yet.

> [!WARNING]
> There is no related compiler, to compile for the MVME yet.
> That is being developed.
> This project is undergoing a conversion of compilers.

# Minis
### Minis is a general purpose programming language, designed to be "good" at whatever you make it do.
In terms of "good," it doesn't mean it's perfect or the best at what it does. Just highly efficent.<br>

# Plugins & Libraries
## Math
### linAlg - Fast linear algebra support
```linAlg``` - Linear algebra (```LLVM```)<br>

## Networking
### Sysnet - Used for thousands of connections to a single place
```sysNet``` - Networking for large systems (```Go```)<br>
```sysNet::start(str port, )``` - Start up a server<br>
```sysNet::send(str port, str ip)``` - Send data to an ip

<br>

```goNet``` - Fast networking for small systems (```C++```)<br>
### Fewer connections, where speed is an absolute need
```goNet::start(str port, )``` - Start up a server<br>
```goNet::send(str port, str ip)``` - Send data to an ip

```random``` - PRNG & TRNG random generators (```C++``` & ```Fortran```)<br>
### True and pseudo randomness, with extreme speed
```random::trn(min=0, max)``` - Generate a true random number from min to max<br>
```random::prn::mersene(min=0, max)``` - Generate a number with the mersene twister<br>
```random::prn::``` -

```time``` - Time formatting ()


```physic``` - Accurate and fast physics math (```Ada``` & ```Fortran```)
### Fast physics calculations with run time verification


## Minis Library Gen File

Minis has a generation file type, which is a bit complex, but allows repetetive things, to be simplified and expanded upon compile.<br>
This makes it much, much easier to maintain repetetive code that spans large areas, and makes it more dense.

### Use of ```[[]]```
In a MILG file, [[]] is "repeat x times."<br>
If you have...
```
str x = "Hello, World!"
print([[x]])
```
It will expand to...
```
for i in 0..len(x) {
  print(i);
}
```

Sometimes, having the for loop expanded is useful.<br>
In some cases, you may have different paths for different things.<br>
This is the same reason that ```#if var is type``` exist.<br>
It's the same affect a JIT has... But AOT instead.<br>

### Dictionaries

Something that is simplified to allow defaulting, is dictionaries.
If you have...
```
dict of float prices = { "vanilla", "chocolate", "circuit" } -> 4.64
```
it expands to...
```
dict prices = {
  "vanilla": 4.64,
  "chocolate": 4.64,
  "circuit": 4.64
};
```
