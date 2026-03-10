# Classes
A class allows for a template to be created, wherein you can create operations and other values.
Unlike a struct or enum, it allows for functions to be embedded within.

Throughout, we will be using an apple class defined as so:
```minis
class Apple {
  public {
    apple(str color, float weight, int seeds) {
      self.color = color;
      self.weight = weight;
      self.seeds = seeds;
    }

    int GetSeeds(&self) {
      return self.seedCount;
    }

    void ChangeColor(&self, str color) {
      self.color = color;
    }

    void eat { /* TODO: Implement */ }
  }
  private {
    int seeds;
    float weight;
    str color;
  }
}
```

> [!NOTE]
> The eat funciton is purposely left blank. It is to showcase an empty class function.
> [!NOTE]
> Minis will warn if you do `*self` or `self`. It is better to alias.

## Public
A class must have a public section. These are things that are accesable from outside of the class.
The public sector is usually where thigs like functions live.
Something that *MUST*  live within the private section, is the initalization function. For more, read [this](./classes.md#initalization).
Public also usually hosts *most* static functions. This does not mean the private section cannot hold static functions.

## Private
The private section is only able to be accessed from within the class.
This is where variables usually live, or other functions that don't need to be used from outside the class.
Although, you can put everything in a private section, this is not recommended, as that is unsafe and may cause confusion.

The private class is for safety

## Initalization
The initalization is a function that lives within the public section.
It is called when making a new instance of a class. When running it, you can run functions, set variables, and so on, where the only limit is what you can do with Minis.
