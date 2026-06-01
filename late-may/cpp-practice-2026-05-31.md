# C++ Practice Notes — May 31, 2026

## Session Info
- **Time:** 3:00 PM CDT (Sunday)
- **Topic:** Separate Compilation (.h / .cpp files)
- **Binary:** `cpp-practice-2026-05-31` ✅ compiled cleanly, ran successfully

## Key Concepts Learned

### Why Separate Compilation Matters
1. **Faster builds:** Change one `.cpp` → only recompile that file
2. **Organization:** Headers declare "what", `.cpp` files define "how"
3. **Reusability:** `#include` headers in multiple `.cpp` files
4. **Encapsulation:** Hide implementation in `.cpp`, expose interface in `.h`
5. **One Definition Rule (ODR):** Every function/class must be defined exactly once

### How a Multi-File Project is Built
```
Rectangle.h      ← class definition, declarations
Rectangle.cpp    ← IMPLEMENTATION (g++ -c Rectangle.cpp → Rectangle.o)
main.cpp         ← uses Rectangle  (g++ -c main.cpp → main.o)
Link:            g++ main.o Rectangle.o -o program
```
The **LINKER** stitches `.o` files together.

### Header Guards
```cpp
#ifndef RECTANGLE_H
#define RECTANGLE_H
class Rectangle { ... };
#endif
```
Never put `using namespace std;` in a header!

### What Goes in .h vs .cpp
| Header (.h) | Source (.cpp) |
|---|---|
| Class definition | Full method implementations |
| Inline function bodies | Non-inline function bodies |
| Templates (must be in header!) | |

### Common Errors
| Error | Cause |
|---|---|
| `undefined reference` | Forgot to link `.o` file |
| `multiple definition` | Non-inline code in `.h` (ODR violation!) |
| `included twice` | Missing `#ifndef` guard |

### Makefiles
```makefile
OBJECTS = main.o rectangle.o
program: $(OBJECTS)
    g++ $(OBJECTS) -o program
main.o: main.cpp rectangle.h
    g++ -c main.cpp -std=c++17
rectangle.o: rectangle.cpp rectangle.h
    g++ -c rectangle.cpp -std=c++17
```

## Programs Created
- **Rectangle class** — constructors, operators (+, ==, <), static factory, inline vs non-inline methods
- **Point3D class** — distanceTo(), works with std::sort via lambda

## Output
```
r1: Living Room (4 x 5) | Area: 20 | Perim: 18
r1 + r2 (combined area): Living Room+Kitchen (6.40312 x 6.40312) | Area: 41
Sorted rectangles by area: Office(16) Kitchen(21) Storage(36) Living Room(60)
Points sorted by distance from origin: C(1,1,1) → A(3,4,0) → B(0,0,5)
```

## Next Steps
- Inheritance and polymorphism
- Virtual functions and abstract base classes
- Smart pointers (unique_ptr, shared_ptr)
- Templates and STL
