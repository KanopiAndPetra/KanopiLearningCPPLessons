// ============================================================================
// C++ Practice Session — 2026-04-15
// ============================================================================
// Topics Studied Today:
//   1. std::vector operations — push_back, pop_back, erase (with iterators),
//      insert (with iterators), begin/end, size(), range-based for loops
//   2. Classes split across files — header (.h) for declarations,
//      implementation (.cpp) for definitions, scope resolution operator (::)
//   3. Constructors with multiple parameters — initializing all member
//      variables in one shot instead of calling setters separately
//
// Today's Program: Student Roster Manager
//   A class (Student) defined across two files (header + implementation),
//   and a vector that holds Student objects. We demonstrate all the key
//   vector manipulation methods, plus sorting and searching.
// ============================================================================

#include <iostream>
#include <iomanip>    // setw, left, fixed, setprecision
#include <string>
#include <vector>     // std::vector — dynamic array container
#include <algorithm>  // std::sort, std::find_if, std::remove_if
#include "cpp-practice-2026-04-15.h"

using namespace std;

// ============================================================================
// HELPER: print a table header
// ============================================================================
void printRosterHeader() {
    cout << "\n  +--------------------+------+------------+------+\n";
    cout << "  | " << setw(20) << left << "Name"
         << " | " << setw(4)  << "Age"
         << " | " << setw(10) << "GPA"
         << " | " << setw(5) << "Grade" << " |\n";
    cout << "  +--------------------+------+------------+------+\n";
}

// ============================================================================
// HELPER: print the full roster using a RANGE-BASED FOR LOOP
// C++11 feature — clean syntax that iterates over all elements in a container.
// Using a const reference (& const) so we don't copy the object or modify it.
// ============================================================================
void printRoster(const vector<Student>& roster) {
    if (roster.empty()) {
        cout << "  (roster is empty)\n";
        return;
    }
    printRosterHeader();
    for (const Student& s : roster) {
        s.display();
    }
    cout << "  +--------------------+------+------------+------+\n";
    cout << "  Total students: " << roster.size() << "\n";
}

// ============================================================================
// DEMO 1: Basic vector operations
// ============================================================================
void demoBasicVectorOps(vector<Student>& roster) {
    cout << "\n========================================\n";
    cout << "  DEMO 1: Basic std::vector Operations\n";
    cout << "========================================\n";

    // ------------------------------------------------------------------
    // push_back() — adds an element to the END of the vector
    // The vector grows automatically; no need to manage size manually.
    // ------------------------------------------------------------------
    cout << "\n  Adding two students with push_back():\n";
    roster.push_back(Student("Alice",    20, 3.8));
    roster.push_back(Student("Marcus",   22, 3.5));

    // ------------------------------------------------------------------
    // insert() — inserts an element at a SPECIFIC POSITION
    // Takes an iterator (position) and the value to insert.
    // New element goes BEFORE the iterator position.
    // roster.begin() + 1 = second position → "Zoe" inserted at index 1
    // ------------------------------------------------------------------
    cout << "\n  Inserting 'Zoe' at index 1 with insert(begin()+1, value):\n";
    roster.insert(roster.begin() + 1, Student("Zoe", 21, 3.9));

    printRoster(roster);

    // ------------------------------------------------------------------
    // erase() — removes an element at a SPECIFIC POSITION
    // Takes an iterator. Use begin() + n to target index n.
    // Removes "Marcus" (index 2)
    // ------------------------------------------------------------------
    cout << "\n  Erasing student at index 2 (Marcus):\n";
    roster.erase(roster.begin() + 2);
    printRoster(roster);

    // ------------------------------------------------------------------
    // pop_back() — removes the LAST element only
    // Does NOT return the removed value — read it with back() first if needed.
    // ------------------------------------------------------------------
    cout << "\n  Removing last student with pop_back():\n";
    roster.pop_back();
    printRoster(roster);

    // ------------------------------------------------------------------
    // Adding more students for later demos
    // ------------------------------------------------------------------
    cout << "\n  Adding more students for later demos:\n";
    roster.push_back(Student("Diana",    19, 3.2));
    roster.push_back(Student("Chen",     23, 3.6));
    roster.push_back(Student("Boris",    20, 2.9));
    printRoster(roster);
}

// ============================================================================
// DEMO 2: Accessing elements — at(), front(), back(), [ ]
// ============================================================================
void demoElementAccess(const vector<Student>& roster) {
    cout << "\n========================================\n";
    cout << "  DEMO 2: Accessing Elements\n";
    cout << "========================================\n";

    // at(index) — bounds-checked access; throws if out of range
    // roster[index] — no bounds check (faster but unsafe)
    cout << "\n  First student (at(0)):\n";
    cout << "  Name: " << roster.at(0).getName() << "\n";
    cout << "  Age:  " << roster.at(0).getAge()  << "\n";

    // front() and back() — convenient shortcuts
    cout << "\n  Last student (back()):\n";
    cout << "  Name: " << roster.back().getName()  << "\n";
    cout << "  GPA:  " << roster.back().getGPA()   << "\n";

    // Iterating with a classic index-based for loop
    // Using size() as the loop bound (counts from 1, so indices go 0..size-1)
    cout << "\n  All names via index loop:\n";
    for (size_t i = 0; i < roster.size(); i++) {
        cout << "  [" << i << "] " << roster[i].getName() << "\n";
    }
}

// ============================================================================
// DEMO 3: Sorting the vector
// ============================================================================
void demoSorting(vector<Student>& roster) {
    cout << "\n========================================\n";
    cout << "  DEMO 3: Sorting with std::sort + Lambdas\n";
    cout << "========================================\n";

    // std::sort takes two iterators (begin, end) and sorts in ascending order.
    // We pass a LAMBDA (anonymous function) to define the sort criterion.
    // Lambda syntax: [](const Student& a, const Student& b) { return ...; }

    // Sort by NAME alphabetically
    cout << "\n  Sorting by name (A→Z):\n";
    sort(roster.begin(), roster.end(),
         [](const Student& a, const Student& b) {
             return a.getName() < b.getName();
         });
    printRoster(roster);

    // Sort by GPA descending (highest first)
    cout << "\n  Sorting by GPA (highest first):\n";
    sort(roster.begin(), roster.end(),
         [](const Student& a, const Student& b) {
             return a.getGPA() > b.getGPA();
         });
    printRoster(roster);

    // Sort by AGE ascending
    cout << "\n  Sorting by age (youngest first):\n";
    sort(roster.begin(), roster.end(),
         [](const Student& a, const Student& b) {
             return a.getAge() < b.getAge();
         });
    printRoster(roster);
}

// ============================================================================
// DEMO 4: Searching with std::find_if
// ============================================================================
void demoSearch(const vector<Student>& roster) {
    cout << "\n========================================\n";
    cout << "  DEMO 4: Searching with std::find_if + Lambda\n";
    cout << "========================================\n";

    // Search for a student by name
    string target = "Diana";
    auto it = find_if(roster.begin(), roster.end(),
        [&target](const Student& s) {
            return s.getName() == target;
        });

    if (it != roster.end()) {
        cout << "\n  Found " << target << "!\n";
        cout << "  Age: " << it->getAge() << ", GPA: " << it->getGPA() << "\n";
    } else {
        cout << "\n  " << target << " not found in roster.\n";
    }

    // Search for first student with GPA >= 3.5 (the "honor roll" check)
    cout << "\n  Searching for first student with GPA >= 3.5:\n";
    auto honorIt = find_if(roster.begin(), roster.end(),
        [](const Student& s) {
            return s.getGPA() >= 3.5;
        });

    if (honorIt != roster.end()) {
        cout << "  Found: " << honorIt->getName()
             << " with GPA " << honorIt->getGPA() << "\n";
    } else {
        cout << "  No honor roll students found.\n";
    }
}

// ============================================================================
// DEMO 5: Removing elements conditionally with remove_if + erase
// This is the "erase-remove idiom" — the standard C++ way to filter a vector.
// ============================================================================
void demoConditionalRemove(vector<Student>& roster) {
    cout << "\n========================================\n";
    cout << "  DEMO 5: Conditional Remove (erase-remove idiom)\n";
    cout << "========================================\n";

    size_t before = roster.size();

    // std::remove_if moves all "non-removed" elements to the front,
    // returning an iterator to the new "logical end."
    // Then we erase everything from that point to the actual end.
    auto newEnd = remove_if(roster.begin(), roster.end(),
        [](const Student& s) {
            return s.getGPA() < 3.0;  // Remove students with GPA below 3.0
        });

    roster.erase(newEnd, roster.end());

    cout << "\n  Removed " << (before - roster.size())
         << " student(s) with GPA < 3.0.\n";
    printRoster(roster);
}

// ============================================================================
// DEMO 6: Vector memory concepts
// ============================================================================
void demoVectorMemory() {
    cout << "\n========================================\n";
    cout << "  DEMO 6: Vector Memory Behavior\n";
    cout << "========================================\n";

    vector<int> numbers(5);  // Creates vector with 5 elements, all 0
    cout << "\n  Created vector<int> numbers(5):\n";
    cout << "  Size: " << numbers.size() << "\n";
    cout << "  Capacity: " << numbers.capacity() << "\n";
    cout << "  Elements: ";
    for (int n : numbers) cout << n << " ";
    cout << "\n";

    // push_back can trigger reallocation as the vector grows
    cout << "\n  Growing the vector with push_back:\n";
    for (int i = 1; i <= 8; i++) {
        numbers.push_back(i * 10);
        cout << "  After push_back(" << i * 10 << "): "
             << "size=" << numbers.size()
             << ", capacity=" << numbers.capacity() << "\n";
    }

    // shrink_to_fit() — requests the vector to release extra capacity
    cout << "\n  Calling shrink_to_fit():\n";
    numbers.shrink_to_fit();
    cout << "  After shrink_to_fit(): "
         << "size=" << numbers.size()
         << ", capacity=" << numbers.capacity() << "\n";

    // clear() — removes all elements, size becomes 0, but capacity stays
    cout << "\n  Calling clear():\n";
    numbers.clear();
    cout << "  After clear(): size=" << numbers.size()
         << ", capacity=" << numbers.capacity() << "\n";
    cout << "  (Note: capacity is retained — vector reserves memory for reuse)\n";
}

// ============================================================================
// MAIN
// ============================================================================
int main() {
    cout << "\n";
    cout << "╔══════════════════════════════════════════════════════════════╗\n";
    cout << "║  C++ PRACTICE — 2026-04-15                               ║\n";
    cout << "║  std::vector Operations + Multi-File Classes + Constructors║\n";
    cout << "╚══════════════════════════════════════════════════════════════╝\n";

    // -------------------------------------------------------------------------
    // Roster stored in a std::vector — grows/shrinks dynamically
    // -------------------------------------------------------------------------
    vector<Student> roster;

    // -------------------------------------------------------------------------
    // Demos
    // -------------------------------------------------------------------------
    demoBasicVectorOps(roster);
    demoElementAccess(roster);
    demoSorting(roster);
    demoSearch(roster);
    demoConditionalRemove(roster);
    demoVectorMemory();

    // -------------------------------------------------------------------------
    // Final roster
    // -------------------------------------------------------------------------
    cout << "\n========================================\n";
    cout << "  FINAL ROSTER\n";
    cout << "========================================\n";
    printRoster(roster);

    cout << "\n  Program complete!\n";
    return 0;
}

// ============================================================================
// LEARNING NOTES
// ============================================================================
//
// MULTI-FILE CLASSES:
//   Student.h  → class declaration (what the class HAS)
//   Student.cpp → method definitions (how the class WORKS)
//   main.cpp   → uses the class
//   #include "Student.h" tells the compiler about the class before using it
//   Student::methodName connects each definition to its class (scope resolution)
//
// CONSTRUCTORS:
//   - Same name as the class, no return type
//   - Runs automatically when an object is created
//   - Can take parameters to initialize member variables immediately
//   - Eliminates the need to call setters one by one after construction
//
// std::vector — dynamic array:
//   push_back(val)    → append to end
//   pop_back()        → remove last element
//   insert(pos, val)  → insert before position (iterator-based)
//   erase(pos)        → remove at position (iterator-based)
//   begin() / end()   → iterators to first and one-past-last elements
//   size()            → number of elements
//   capacity()        → slots allocated (may be > size)
//   clear()           → remove all elements (capacity kept)
//   shrink_to_fit()   → release extra capacity
//   empty()           → true if size is 0
//
// ITERATORS:
//   - Objects that point to elements in a container
//   - begin() → first element
//   - end()   → one-past-last (sentinel)
//   - Use begin()+n to get the nth element
//
// RANGE-BASED FOR LOOP (C++11):
//   for (const Type& item : container) { ... }
//   - Cleaner than index-based loops
//   - const ref = no copy, no modification
//
// LAMBDA FUNCTIONS (C++11):
//   [capture](parameters) { body }
//   - Anonymous functions defined inline
//   - Used with sort, find_if, remove_if, etc.
//   - [=] captures all variables by value; [&] by reference
//
// ERASE-REMOVE IDIOM:
//   auto it = remove_if(v.begin(), v.end(), predicate);
//   v.erase(it, v.end());
//   - Standard C++ way to conditionally remove elements
// ============================================================================
