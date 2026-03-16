# Tables
A table allows for operations to occur, based on an input.
You can perform operations based on input.

```minis
int x = 32;
str y = "y";
table x {
  32 -> {
    print("x is 32!");
  }
}

table y {
  "y" -> {
    print("y is y!");
  },
  "z" -> {
    y = "y"
    print("y is y!");
  }
}
```

A table works a lot like a dictionary. But, rather than `key:value`, it's `key:operation`.
