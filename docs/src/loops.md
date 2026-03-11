# Loops

## While Loops
A while loop is a simple loop, where it only repeats while a condition is true (or 1).
It can have anything. From malicously deleting files, to asking for the right password until the correct one is given.
The syntax is...
```minis
while (<thing to check>) {}
```
As soon as the value to check is either 0 or false, it will stop.

## For Loops
A for loop allows for a more complex loops than a while loop. It has a value related to it, a check, and an increment specified.
The syntax is like so...
```minis
for (<type> <name> = <value>; <thing to check>; <how to increment>) {}
``
Alternatively, you can leave them all blank, and do it more like a while loop...
```minis
for (;;) {}
```
This is the most basic form, and can be used over a while loop. But a while loop is a bit more clear with intent.
