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
