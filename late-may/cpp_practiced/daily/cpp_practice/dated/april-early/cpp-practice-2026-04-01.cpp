// cpp-practice-2026-04-01.cpp
// Practice: Classes, Pointers, and String Manipulation
// Combines all three topics into one cohesive program

#include <iostream>
#include <string>
#include <vector>

using namespace std;

// ============================================================
// CLASS: Book
// Represents a book with title, author, and page count.
// Demonstrates: member variables, constructors, methods
// ============================================================
class Book {
private:
    string title;
    string author;
    int pageCount;

public:
    // Constructor
    Book(string t, string a, int pages)
        : title(t), author(a), pageCount(pages) {}

    // Getters
    string getTitle() const { return title; }
    string getAuthor() const { return author; }
    int getPageCount() const { return pageCount; }

    // Returns a formatted description of the book
    string getDescription() const {
        return "\"" + title + "\" by " + author + " (" + to_string(pageCount) + " pages)";
    }

    // Checks if title contains a search term (case-insensitive)
    bool matchesSearch(const string& searchTerm) const {
        string lowerTitle = title;
        string lowerSearch = searchTerm;

        // Convert both to lowercase for case-insensitive comparison
        for (char& c : lowerTitle) c = tolower(c);
        for (char& c : lowerSearch) c = tolower(c);

        return lowerTitle.find(lowerSearch) != string::npos;
    }
};

// ============================================================
// FUNCTION: demonstratePointers()
// Shows basic pointer usage with Book objects
// ============================================================
void demonstratePointers() {
    cout << "=== Pointer Demo ===" << endl;

    // Create a book on the heap using 'new' (returns a pointer)
    Book* libraryBook = new Book("1984", "George Orwell", 328);

    // Stack-allocated book for comparison
    Book stackBook("Brave New World", "Aldous Huxley", 268);

    // Pointer to the stack book
    Book* pointerToStackBook = &stackBook;

    // Access object through pointer using ->
    cout << "Heap book (via pointer): " << libraryBook->getDescription() << endl;
    cout << "Stack book (via pointer): " << pointerToStackBook->getDescription() << endl;

    // Pointer arithmetic concept: array of pointers
    Book* bookShelf[3];
    bookShelf[0] = new Book("The Hobbit", "J.R.R. Tolkien", 310);
    bookShelf[1] = new Book("Dune", "Frank Herbert", 688);
    bookShelf[2] = new Book("Foundation", "Isaac Asimov", 244);

    cout << "\nBookshelf contents:" << endl;
    for (int i = 0; i < 3; i++) {
        // Dereference pointer to get object, then call method
        cout << "  " << (*bookShelf[i]).getDescription() << endl;
    }

    // Clean up heap-allocated memory (important!)
    delete libraryBook;
    for (int i = 0; i < 3; i++) {
        delete bookShelf[i];
    }

    cout << "Memory cleaned up successfully." << endl;
}

// ============================================================
// FUNCTION: demonstrateStringManipulation()
// Shows various string operations
// ============================================================
void demonstrateStringManipulation() {
    cout << "\n=== String Manipulation Demo ===" << endl;

    string phrase = "The quick brown fox jumps over the lazy dog";

    // Length
    cout << "Phrase: \"" << phrase << "\"" << endl;
    cout << "Length: " << phrase.length() << " characters" << endl;

    // Find substring
    string searchWord = "fox";
    size_t found = phrase.find(searchWord);
    if (found != string::npos) {
        cout << "Found \"" << searchWord << "\" at position " << found << endl;
    }

    // Replace substring
    string modified = phrase;
    modified.replace(10, 5, "slow");  // Replace 5 chars starting at position 10
    cout << "After replace: \"" << modified << "\"" << endl;

    // Extract substring
    string substring = phrase.substr(4, 15);  // 15 chars starting at position 4
    cout << "Substring (pos 4, 15 chars): \"" << substring << "\"" << endl;

    // String concatenation
    string part1 = "Hello";
    string part2 = "World";
    string combined = part1 + ", " + part2 + "!";
    cout << "Concatenation: \"" << combined << "\"" << endl;

    // Iterate through characters
    cout << "Characters in \"" << part1 << "\": ";
    for (char c : part1) {
        cout << "[" << c << "] ";
    }
    cout << endl;
}

// ============================================================
// FUNCTION: searchBookCollection()
// Uses pointers to traverse a collection of books
// Demonstrates: arrays of objects, pointers, string search
// ============================================================
void searchBookCollection() {
    cout << "\n=== Book Collection Search ===" << endl;

    // Create a dynamic array of Book pointers
    const int NUM_BOOKS = 5;
    Book** collection = new Book*[NUM_BOOKS];

    collection[0] = new Book("The Great Gatsby", "F. Scott Fitzgerald", 180);
    collection[1] = new Book("To Kill a Mockingbird", "Harper Lee", 281);
    collection[2] = new Book("1984", "George Orwell", 328);
    collection[3] = new Book("Animal Farm", "George Orwell", 112);
    collection[4] = new Book("Brave New World", "Aldous Huxley", 268);

    // Search for books with "orwell" (case-insensitive via Book::matchesSearch)
    string searchTerm = "orwell";
    cout << "Searching for \"" << searchTerm << "\"..." << endl;

    int matchCount = 0;
    for (int i = 0; i < NUM_BOOKS; i++) {
        // Use pointer dereference to call method
        if ((*collection[i]).matchesSearch(searchTerm)) {
            cout << "  Found: " << (*collection[i]).getDescription() << endl;
            matchCount++;
        }
    }

    if (matchCount == 0) {
        cout << "  No matches found." << endl;
    } else {
        cout << "  Total matches: " << matchCount << endl;
    }

    // Clean up
    for (int i = 0; i < NUM_BOOKS; i++) {
        delete collection[i];
    }
    delete[] collection;
}

// ============================================================
// MAIN
// ============================================================
int main() {
    cout << "C++ Practice: Classes, Pointers & Strings" << endl;
    cout << "==========================================" << endl;

    // 1. String manipulation demo
    demonstrateStringManipulation();

    // 2. Pointer demonstration with Book objects
    demonstratePointers();

    // 3. Combined: search a collection using pointers
    searchBookCollection();

    cout << "\n==========================================" << endl;
    cout << "Practice complete! Key concepts covered:" << endl;
    cout << "  - Classes with member variables & methods" << endl;
    cout << "  - Stack vs Heap allocation (new/delete)" << endl;
    cout << "  - Pointers: *, &, -> operators" << endl;
    cout << "  - String methods: find, replace, substr" << endl;
    cout << "  - Array of pointers traversal" << endl;

    return 0;
}
