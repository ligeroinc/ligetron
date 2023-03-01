# Compile Example as WASM Module

We use `Emscripten` to compile C++ files into WebAssembly modules. 
To compile a sample C++ file, first make sure `Emscripten` is installed in you machine.
Use the command to compile:

```bash
em++ -c -Oz <source.cpp> -o <target.wasm>
```
