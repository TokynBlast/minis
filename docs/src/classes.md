# Classes
A class allows for a template to be created, wherein you can create operations and other values.
Unlike a struct or enum, it allows for functions to be embedded within.

## Public
A class must have a public section. These are things that are accesable from outside of the class.
The public sector is usually where thigs like functions live.
Something that *MUST*  live within the private section, is the initalization function. For more, read [this](./classes.md#initalization).

## Private
The private section is only able to be accessed from within the class.
This is where variables usually live, or other functions that don't need to be used from outside the class.
Although, you can put everything in a private section, this is not recommended, as that is unsafe and may cause confusion.

## Initalization
The initalization is a function that lives within the public section.
It is called when making a new instance of a class. When running it, you can run functions, set variables, and so on, where the only limit is what you can do with Minis.
