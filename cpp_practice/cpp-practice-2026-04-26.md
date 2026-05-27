# C++ Practice Notes - 2026-04-26

## Repositories Studied

### 1. `1.47CZTheHeapWNotes` - The Heap (Dynamic Memory)
**Language:** C++  
**Concept:** Dynamic memory allocation using `malloc()` and `free()`

Key learnings:
- The heap is leftover memory not being used by the OS or other programs
- Use `malloc(size_in_bytes)` to allocate memory from the heap
- Always cast the pointer to the appropriate type (e.g., `(int*)malloc(...)`)
- Use `free(pointer)` to return memory to the heap when done
- Without freeing, you get a **memory leak**

Example from repo:
```c
int* points;
points = (int*)malloc(5 * sizeof(int));  // Reserve space for 5 integers
free(points);  // Return memory to heap
```

### 2. `1.49CZStructuresWNotes` - Structures
**Language:** C  
**Concept:** Grouping related variables together

Key learnings:
- A `struct` groups multiple variables (called "members") under one name
- Useful for representing records like employees, users, team members
- Each member can have a different data type

Example from repo:
```c
struct user {
    int userID;
    char firstName[25];
    char lastName[25];
    int age;
    float weight;
};
```

---

## What I'm Building Today

Combining both concepts: **Dynamic Employee Database**

A program that:
1. Uses a `struct` to define an employee record (id, name, salary)
2. Allocates memory on the heap for an array of employees using `malloc()`
3. Allows adding employees, displaying them, then frees memory properly

This combines structure definition + dynamic memory allocation + practical data handling.

---

## Why These Topics?

- **Structures** are fundamental for representing real-world data
- **Heap/Dynamic memory** is essential for building flexible programs that can handle varying amounts of data
- Together they're the foundation for more advanced concepts like linked lists, trees, and dynamic data structures

---

## C++ Features Used

- `struct` for defining custom data types
- `malloc()` / `free()` from `<cstdlib>` for dynamic memory
- `std::cin` / `std::cout` for input/output
- C-strings (`char[]`) for fixed-size name storage
- Loop iteration and conditional logic