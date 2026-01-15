> [!WARNING]
> This refrences things not fully implemented yet.

> [!WARNING]
> There is no related compiler, to compile for the MVME yet.
> That is being developed.

# Minis
### Minis is a general purpose programming language, designed to be "good" at whatever you make it do.<br>
In terms of "good", it doesn't mean it's perfect or the best at what it does. Just highly efficent.

## How is it so good?
Minis is a GCC project, which means it combines multiple languages to do one thing.<br>
Why multiple languages?<br>
Glad you asked, it's not about support. It's about efficency.<br>
For example, using the LinAlg plugin:
```
list arrayOne = [24, 53, 52];
list arrayTwo = [57, 73, 12];
print(linAlg::arrayAdd(arrayOne, arrayTwo))
```
To the dev, you just did a simple equation of adding two arrays.<br>
Internally, the math we just did is being accelerated.<br>
How? By having (most of) the math powered by Fortran!<br>
Fortran is DESIGNED to be good at math!<br><br>
Another example of where math is better, is:
```
u32 x = 1 + 1 + 42432 + 422 + 13 + 8420 + 492 - 492 + 20492 + 9204 + 2492 + 4920 + 2902 + 24902;
```
Fortran takes over, and starts adding them together, multiple at a time!
> [!NOTE] Due to current structure and limitations, it gets split at the subtraction.
<br><br>
Minis is built on multiple languages, where each gets to shine where they shine best!<br>
While still being fast AND memory safe!

# Plugins & Libraries
## Math
### linAlg - Fast linear algebra support
```linAlg``` - Linear algebra (```Fortran``` and ```Julia```)<br>

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
