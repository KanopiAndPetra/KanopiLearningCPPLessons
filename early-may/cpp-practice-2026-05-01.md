// cpp-practice-2026-05-01.md
// C++ Practice Session - 2026-05-01
// Topic: Classes with Member Variables, Functions, and Multi-Parameter Helpers

## Repos Studied

### 1. 1.1Oppie1CPP
- **Repo:** https://github.com/Oppie1/1.1Oppie1CPP
- **Lesson studied:** "1.10Oppie1FuncWMultiplParmetrs" — Functions with multiple parameters
- **Key concept:** A function can take multiple parameters of the same or different types. 
  The arguments passed must match the parameter types in order. The function returns a value
  that replaces the function call in the calling code.

Example from repo:
```cpp
int addNumbers(int x, int y, int a, int b) {
    int answer = x + y + a + b;
    return answer;
}
// Called as: addNumbers(10, 15, 28, 7) returns 60
```

## What I Built

I created a `Student` class that demonstrates:
1. **Member variables** — name, id, and an array of 5 exam scores
2. **Member functions** — setName(), setId(), setExamScore(), calculateAverage()
3. **Multi-parameter helper function** — calculateTotal() takes 5 parameters and sums them

This combines the multi-parameter function concept with OOP (Object-Oriented Programming)
to show how classes can encapsulate data and behavior.

## Concepts Practiced

- Defining a class with private/public sections
- Constructor initialization
- Using member functions to modify object state
- Passing multiple arguments to a helper function
- Return values from both member functions and helper functions

## Files

- Code: `cpp-practice-2026-05-01.cpp`
- Notes: `cpp-practice-2026-05-01.md`