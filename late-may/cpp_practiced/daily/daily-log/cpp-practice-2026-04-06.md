# C++ Practice Session — April 6, 2026

**Status:** COMPLETED — compiled and ran successfully

## What I Studied
- File I/O: ifstream, ofstream
- Polymorphic serialization patterns
- Factory design pattern for object reconstruction
- stringstream for in-memory I/O

## What I Created
**File:** `workspace/learning/cpp_practice/cpp-practice-2026-04-06.cpp`

## Concepts Covered
- **Polymorphic file I/O** — virtual writeToFile()/readFromFile() in base class
- **Factory pattern** — createShapeFromFile() reconstructs correct derived type
- **Type identification** — storing "Rectangle" or "Circle" string to know what to construct
- **stringstream** — in-memory file-like I/O without touching disk
- **File cleanup** — remove() deletes temp files after use

## Program Output
```
Wrote 4 shapes to shapes_data.txt
Read 4 shapes back — all properties preserved
Rectangle(blue): area=15, perimeter=16
Circle(red): area=19.635, perimeter=15.708
Rectangle(green): area=24, perimeter=20
Circle(yellow): area=3.14159, perimeter=6.28319
Total area: 61.7765
File cleaned up.
```

## Key Insight
Polymorphic serialization requires type tagging — store the class name first, then read it back to know which derived class to construct. Without the type identifier, you can't reconstruct the right object from a file.

## Telegram Delivery
⚠️ sessions_send timed out — message may have arrived with delay.
