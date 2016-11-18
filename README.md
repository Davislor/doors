# doors
A C port of the Doors library from Solaris to POSIX.  Google Summer of Code project.

The included `Makefile` compiles the source file into a library called `libdoor.so` or `libdoor.a`.  You can link your programs to this library with the header files in the `include` directory and test them with the provided test programs (which will throw an assertion on failure, and complete without error if the tests succeed).
