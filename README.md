This is a very simple 2d platformer. It is meant as a learning exercise to develop my own understanding of "from-scratch" programming 
in a lower level language like c++. As such it boasts several, hastily programmed self-rolled features. They might be scrappy in nature 
but they work, have led to me learning quite a fair bit about programming in general, and have been a worthwhile effort.

## Features
- vector math (see source/math.h)
- text rendering with decent performance
- manual memory management using memory arenas
- a batched renderer for quads that has decent performance

## Building
I have not targeted this as something that should be convenient for others to "just build and run". 
Truth be told, I do not much understand some fundamentals of building static libraries and whether or not they are expected to "just build". 
Regardless, if you are on linux, you can try to run `build.sh` and it should for the most part run. If not, I can't help you very much, feel free 
to peruse the code in that case.
