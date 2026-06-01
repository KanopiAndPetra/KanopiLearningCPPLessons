// cpp-practice-2026-03-24.cpp
// C++ Practice: Classes, Pointers, and String Manipulation
// Created: 2026-03-24

#include <iostream>
#include <string>
#include <vector>

// ============================================================================
// CLASSES & OBJECTS: A simple "Book" class with member variables and methods
// ============================================================================

class Book {
private:
    std::string title;
    std::string author;
    int yearPublished;
    int pages;

public:
    // Constructor - initializes a new Book
    Book(std::string t, std::string a, int year, int p) {
        title = t;
        author = a;
        yearPublished = year;
        pages = p;
    }

    // Getter methods
    std::string getTitle() { return title; }
    std::string getAuthor() { return author; }
    int getYear() { return yearPublished; }
    int getPages() { return pages; }

    // A method that returns a formatted string description
    std::string getDescription() {
        return title + " by " + author + " (" + std::to_string(yearPublished) + ")";
    }

    // Check if this book is a "long read"
    bool isLongRead() {
        return pages > 300;
    }
};

// ============================================================================
// POINTERS: Demonstrate pointer usage with our Book objects
// ============================================================================

void demonstratePointers(Book* bookPtr) {
    // Arrow operator (->) accesses members through pointer
    std::cout << "  [Pointer demo] Title via pointer: " << bookPtr->getTitle() << "\n";
    std::cout << "  [Pointer demo] Pages via pointer: " << bookPtr->getPages() << "\n";
    
    // We can also dereference and use dot notation
    std::cout << "  [Pointer demo] Author via (*ptr): " << (*bookPtr).getAuthor() << "\n";
}

// ============================================================================
// STRING MANIPULATION: Various string operations
// ============================================================================

std::string manipulateString(std::string input) {
    std::cout << "\n  Original: \"" << input << "\"\n";
    
    // Get length
    std::cout << "  Length: " << input.length() << "\n";
    
    // Convert to uppercase (character by character)
    std::string upper = input;
    for (char& c : upper) {
        c = std::toupper(c);
    }
    std::cout << "  Uppercase: \"" << upper << "\"\n";
    
    // Find a substring
    std::string searchFor = "World";
    size_t pos = input.find(searchFor);
    if (pos != std::string::npos) {
        std::cout << "  Found \"" << searchFor << "\" at position " << pos << "\n";
    } else {
        std::cout << "  \"" << searchFor << "\" not found\n";
    }
    
    // Replace substring
    std::string replaced = input;
    replaced.replace(pos, searchFor.length(), "Universe");
    std::cout << "  After replace: \"" << replaced << "\"\n";
    
    // Append something
    std::string appended = input + " - Added!";
    std::cout << "  Appended: \"" << appended << "\"\n";
    
    // Substring extraction
    std::string sub = input.substr(0, 5);
    std::cout << "  First 5 chars: \"" << sub << "\"\n";
    
    return upper;
}

// ============================================================================
// MAIN: Put it all together
// ============================================================================

int main() {
    std::cout << "=== C++ Practice: Classes, Pointers & Strings ===\n\n";
    
    // --- Part 1: Classes and Objects ---
    std::cout << "--- Part 1: Classes & Objects ---\n";
    
    // Create Book objects
    Book book1("The Pragmatic Programmer", "David Thomas", 1999, 352);
    Book book2("1984", "George Orwell", 1949, 328);
    Book book3("In Search of Lost Time", "Marcel Proust", 1913, 4215);
    
    // Display book info using member functions
    std::cout << "Book 1: " << book1.getDescription() << "\n";
    std::cout << "  Pages: " << book1.getPages() << "\n";
    std::cout << "  Long read? " << (book1.isLongRead() ? "Yes" : "No") << "\n";
    
    std::cout << "\nBook 2: " << book2.getDescription() << "\n";
    std::cout << "  Pages: " << book2.getPages() << "\n";
    std::cout << "  Long read? " << (book2.isLongRead() ? "Yes" : "No") << "\n";
    
    std::cout << "\nBook 3: " << book3.getDescription() << "\n";
    std::cout << "  Pages: " << book3.getPages() << "\n";
    std::cout << "  Long read? " << (book3.isLongRead() ? "Yes" : "No") << "\n";
    
    // --- Part 2: Pointers ---
    std::cout << "\n--- Part 2: Pointers ---\n";
    
    // Create a pointer to book1
    Book* bookPtr = &book1;
    std::cout << "Pointer points to: " << bookPtr->getTitle() << "\n";
    
    // Pass pointer to function
    demonstratePointers(&book2);
    
    // Array of pointers
    Book* books[] = {&book1, &book2, &book3};
    std::cout << "\n  Array of book pointers:\n";
    for (int i = 0; i < 3; i++) {
        std::cout << "    Book " << (i+1) << ": " << books[i]->getTitle() << "\n";
    }
    
    // --- Part 3: String Manipulation ---
    std::cout << "\n--- Part 3: String Manipulation ---\n";
    
    std::string testStr = "Hello, World!";
    std::string result = manipulateString(testStr);
    std::cout << "  Returned uppercase: \"" << result << "\"\n";
    
    // More string examples
    std::string name = "Alice";
    std::string greeting = "Hello, " + name + "!";
    std::cout << "\n  Concatenation: \"" << greeting << "\"\n";
    
    // Using string methods
    if (name == "Alice") {
        std::cout << "  Equality check passed!\n";
    }
    
    // --- Summary ---
    std::cout << "\n=== Practice Complete! ===\n";
    std::cout << "Concepts covered:\n";
    std::cout << "  - Class definition with private/public members\n";
    std::cout << "  - Constructors and member functions\n";
    std::cout << "  - Pointers and the arrow operator (->)\n";
    std::cout << "  - Pointer to object and dereferencing\n";
    std::cout << "  - String length, uppercase, find, replace, substr\n";
    
    return 0;
}
