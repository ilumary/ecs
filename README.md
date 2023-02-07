# ECS

This is my entity component system for use in my game engine [nvkg](https://github.com/ilumary/nvkg). It aims at delivering maximum performance and is written in 100% `c++20`.

This project is under active development. Some of the listed features may not be complete or work correctly. A `1.0` release is only considered if the below listed functionality is completely implemented and properly tested.
- [x] creating and deleting entities with components
- [x] getting reference to component(s)
- [x] const correctness
- [x] counting a type of component
- [x] iterate over specified components
- [ ] thread safety

## Contents

- [ECS](#ecs)
  - [Contents](#contents)
  - [Build](#build)
  - [Explanation](#explanation)
  - [Examples](#examples)
  - [Contributing](#contributing)
  - [Contributors](#contributors)
  - [Benchmark](#benchmark)
  - [References](#references)

## Build

There are no external dependencies. The project can be build with any compiler supporting `c++20`. Tested with `gcc` and `clang`.

## Explanation

A good entity component system has to perform great under load and allow for very fast iterations over components. Typical implementations store their components in a table where each entity has its own row and enough space for every component. This performs more than well enough for small applications but scales very badly due to the high number of cache misses being produced when trying to iterate over a specific set of components. 

Introducing archetypes. Archetypes represent a unique set of components. Using archetypes we can drastically reduce unused space. If we add a few nice matching functions for querying archetypes for their components, we're done right? No, we can do even better.

We may not have unused space right now but our components and entities are still laid out continously in memory. Consider the follwing example:
```
struct s1 {
    uint32_t u;
};

struct s2 {
    char c;
};
```
Say we create a few entities who each have `s1` and `s2` as components and therefore the same archetype and we store them in memory, it will look something like this:

`u|c|u|c|u|c|...`

This is the standard memory layout and is called Array of structures (= AoS). In theory that is fine, but in our use case it is still not optimal. If we want to iterate over every `s1` component, we will always have to jump over `s2`. It also prevents the usage of SIMD (= Single Instruction Multiple Data) instructions.

SoA to the rescue. SoA stands for Structures of Arrays and is exatly what is says. Instead of storing per structure, we store per member. The memory layout will look like this:

`u|u|u|...|c|c|c|...`

Iterating over the `u` component for example can now be done without knowing anything about `c`. We just match all archetypes with that component, and iterate over the given pointer with a specified size.

In addition, my implementation gives each archtype a dynamic number of memory blocks where each block has been initialized with the memory page size of the system. This allows, depending on size, to iterate over hundreds of objects without a single cache miss. The overhead for archetypes with very few entites may be greater than with other approaches, but this scales an order of magnitude better (not from an actual benchmark, just for drama).

## Examples
For examples, please refer to the `main.cpp` file in which a lot of use cases are tested.

## Contributing
Feel free to open an issue or a discussion!

## Contributors
- [ilumary](https://github.com/ilumary)

## Benchmark
TODO

## References
- [creating an ecs](https://ajmmertens.medium.com/building-an-ecs-1-where-are-my-entities-and-components-63d07c7da742)