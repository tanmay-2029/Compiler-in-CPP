# Hydrogen Compiler (in C++)

A small compiler I made for my own toy language called **Hydrogen**.
It reads a `.hyd` file, checks it for errors, and prints the same program written in C++.
Then we compile that C++ file with `g++` and run it.

The whole compiler is one file, `main.cpp`, and uses only the C++ standard library (no Bison/Flex).

## What the compiler does

1. **Scanning** - removes blank lines and `#` comments, checks the indentation (4 spaces per level, no tabs).
2. **Parsing** - recursive descent, one function for each kind of statement.
3. **Semantic checks** - symbol table + type checking (undeclared variables, wrong types, etc.).
4. **Code generation** - writes the equivalent C++ code.

If there is any error, it prints the line number and the message and stops.

## The Hydrogen language

| Thing | How it looks |
|---|---|
| Types | `int`, `float`, `string`, `bool` |
| Declaration | `int x = 5` |
| Assignment | `x = x + 1` |
| Print | `print(x)` |
| If / else | `if x > 3:` ... `else:` |
| While loop | `while i < 3:` |
| Operators | `+ - * %` and `== != <= >= < >` |
| Comments | a line starting with `#` |
| Blocks | indent by **4 spaces** (instead of `{ }`) |

Example (`test.hyd`):

```
# Hydrogen example

int a = 10
int b = 20
int total = a + b

print(total)

string message = "Hello, Hydrogen!"
print(message)

if total > 20:
    print("Total is greater than 20")
else:
    print("Total is not greater than 20")

int i = 0

while i < 3:
    print(i)
    i = i + 1
```

## How to run it

You need `g++` (with C++17 support). Open a terminal in the folder that has `main.cpp`.
If your files are inside a folder like `src`, first run `cd src`.

**Step 1 - build the compiler (only once)**

```
g++ -std=c++17 main.cpp -o hydrogen
```

**Step 2 - translate a Hydrogen file into C++**

```
./hydrogen test.hyd > test.cpp
```

The compiler prints the C++ code on the screen, so `>` saves it into `test.cpp`.
`test.hyd` does not have to be in the same folder, you can give any path, e.g. `./hydrogen ~/Documents/test.hyd > test.cpp`.

**Step 3 - compile and run the generated C++**

```
g++ test.cpp -o test
./test
```

**All three steps in one go:**

```
g++ -std=c++17 main.cpp -o hydrogen && ./hydrogen test.hyd > test.cpp && g++ test.cpp -o test && ./test
```

### On Windows

```
g++ -std=c++17 main.cpp -o hydrogen.exe
hydrogen.exe test.hyd > test.cpp
g++ test.cpp -o test.exe
test.exe
```

(In PowerShell write `.\hydrogen.exe` and `.\test.exe`.)

## Expected output

For the `test.hyd` above, the program prints:

```
30
Hello, Hydrogen!
Total is greater than 20
0
1
2
```

And `test.cpp` (made by the compiler) looks like this:

```cpp
#include <iostream>
#include <string>

int main() {
int a = 10;
int b = 20;
int total = a + b;
std::cout << total << '\n';
std::string message = "Hello, Hydrogen!";
std::cout << message << '\n';
if (total > 20) {
    std::cout << "Total is greater than 20" << '\n';
} else {
    std::cout << "Total is not greater than 20" << '\n';
}
int i = 0;
while (i < 3) {
    std::cout << i << '\n';
    i = i + 1;
}
    return 0;
}
```

## Error message

Errors go to the terminal in this format:

```
Hydrogen compiler error: Line 2: cannot infer expression type: b
```

Some errors it catches:

| Mistake | Message |
|---|---|
| using a variable that was never declared | `cannot infer expression type: b` |
| declaring the same name twice | `variable already declared: a` |
| `int f = 3.5` | `cannot assign float to int` |
| `if` / `while` line without `:` | `expected ':' after if condition` |
| condition that is not true/false | `if condition must be boolean` |
| no indented block after `if`/`while` | `expected an indented block` |
| indentation not a multiple of 4 | `use multiples of 4 spaces` |
| tab used for indentation | `use spaces instead of tabs` |
| `int if = 3` (keyword as a name) | `invalid variable name` |

## Limitations (things not done yet)

- Division `/` is not supported yet.
- Brackets inside expressions like `(a + 2) * 3` do not work.
- Only whole-line comments (`# ...`), not comments at the end of a line.
- No `elif`, `for`, functions, `and`/`or`/`not`, or input.
- The compiler stops at the first error.

## Files

```
Compiler-in-CPP/
├── src/
│   ├── main.cpp      the compiler
│   └── test.hyd      sample Hydrogen program
└── README.md
```

## Concepts used

Tokens and lexical analysis, grammar, recursive descent parsing, symbol table, type checking, syntax-directed code generation.
