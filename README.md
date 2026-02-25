> [!WARNING]
> The compiler is being developed!

# Minis
### Minis is a general purpose programming language, designed to be "good" at whatever you make it do.
In terms of "good," it doesn't mean it's perfect or the best at what it does. Just highly efficent, with an aim for efficiency.<br>

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
```
You can even embed logic within the circuit! Just make another scope :)
