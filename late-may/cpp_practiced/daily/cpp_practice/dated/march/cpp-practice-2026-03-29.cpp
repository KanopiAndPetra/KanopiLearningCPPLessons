// cpp-practice-2026-03-29.cpp
// Practice program: Classes, Pointers, and String Manipulation
// Combines what we learned about OOP, memory management, and strings

#include <iostream>
#include <string>
#include <vector>

using namespace std;

// ============================================
// CLASS: Book - represents a book with title, author, and page count
// Demonstrates: member variables, constructors, methods, const correctness
// ============================================
class Book {
private:
    string title;
    string author;
    int pages;
    int currentPage;

public:
    // Constructor - initializes a new Book
    Book(string t, string a, int p) : title(t), author(a), pages(p), currentPage(0) {
        cout << "📚 Created book: \"" << title << "\" by " << author << endl;
    }

    // Destructor - called when Book is destroyed
    ~Book() {
        cout << "📕 Destroyed book: \"" << title << "\"" << endl;
    }

    // Getters (const because they don't modify the object)
    string getTitle() const { return title; }
    string getAuthor() const { return author; }
    int getPages() const { return pages; }
    int getCurrentPage() const { return currentPage; }

    // Read a number of pages, returns false if not enough pages remaining
    bool read(int numPages) {
        if (currentPage + numPages > pages) {
            cout << "⚠️  Not enough pages! Only " << (pages - currentPage) << " remaining." << endl;
            return false;
        }
        currentPage += numPages;
        cout << "📖 Read " << numPages << " pages. Now on page " << currentPage << "/" << pages << endl;
        return true;
    }

    // Get reading progress as a percentage
    double getProgress() const {
        return (static_cast<double>(currentPage) / pages) * 100.0;
    }
};

// ============================================
// CLASS: Library - manages a collection of Books
// Demonstrates: pointers to objects, dynamic memory, vectors of objects
// ============================================
class Library {
private:
    string name;
    vector<Book*> books;  // Vector of pointers to Book objects

public:
    Library(string n) : name(n) {
        cout << "🏛️  Opened library: " << name << endl;
    }

    ~Library() {
        cout << "🏛️  Closing library: " << name << endl;
        // IMPORTANT: Delete all dynamically allocated books
        for (Book* b : books) {
            delete b;
        }
        cout << "🗑️  All books deleted." << endl;
    }

    // Add a book to the library (takes ownership of the pointer)
    void addBook(Book* book) {
        books.push_back(book);
        cout << "✅ Added \"" << book->getTitle() << "\" to " << name << endl;
    }

    // Display all books using pointer arithmetic
    void displayBooks() const {
        cout << "\n========================================" << endl;
        cout << "📚 Library: " << name << endl;
        cout << "========================================" << endl;
        
        if (books.empty()) {
            cout << "No books in library." << endl;
            return;
        }

        // Using pointer iteration
        for (size_t i = 0; i < books.size(); i++) {
            Book* b = books[i];  // Pointer dereference
            cout << "\n[" << (i + 1) << "] \"" << b->getTitle() << "\"" << endl;
            cout << "    Author: " << b->getAuthor() << endl;
            cout << "    Pages: " << b->getPages() << endl;
            cout << "    Progress: " << b->getProgress() << "%" << endl;
        }
        cout << "========================================\n" << endl;
    }

    // Find a book by title (returns pointer or nullptr)
    Book* findBook(const string& title) const {
        for (Book* b : books) {
            if (b->getTitle() == title) {
                return b;
            }
        }
        return nullptr;  // Not found - return null pointer
    }
};

// ============================================
// STRING MANIPULATION DEMO
// ============================================
void demonstrateStringManipulation() {
    cout << "\n========================================" << endl;
    cout << "🔤 STRING MANIPULATION DEMO" << endl;
    cout << "========================================" << endl;

    string text = "The quick brown fox jumps over the lazy dog";

    // String length
    cout << "Original: \"" << text << "\"" << endl;
    cout << "Length: " << text.length() << " characters" << endl;

    // Substring
    cout << "Substring (0, 19): \"" << text.substr(0, 19) << "\"" << endl;

    // Find
    cout << "\"fox\" found at index: " << text.find("fox") << endl;

    // Replace
    string modified = text;
    modified.replace(10, 5, "slow");  // Replace "brown" with "slow"
    cout << "Replaced \"brown\" with \"slow\": \"" << modified << "\"" << endl;

    // String concatenation
    string part1 = "Hello";
    string part2 = "World";
    cout << "Concatenated: \"" << part1 + ", " + part2 + "!\"" << endl;

    // Convert to uppercase (manual)
    string upper = text;
    for (char& c : upper) {
        c = toupper(c);
    }
    cout << "Uppercase: \"" << upper << "\"" << endl;

    cout << "========================================\n" << endl;
}

// ============================================
// POINTER DEMO - swap two numbers using pointers
// ============================================
void swapWithPointers(int* a, int* b) {
    cout << "\n========================================" << endl;
    cout << "🔄 POINTER SWAP DEMO" << endl;
    cout << "========================================" << endl;
    
    cout << "Before swap: a = " << *a << ", b = " << *b << endl;
    
    int temp = *a;
    *a = *b;
    *b = temp;
    
    cout << "After swap: a = " << *a << ", b = " << *b << endl;
    cout << "========================================\n" << endl;
}

// ============================================
// MAIN - puts it all together
// ============================================
int main() {
    cout << "\n🎓 C++ PRACTICE PROGRAM" << endl;
    cout << "========================\n" << endl;

    // --- Demo 1: String manipulation ---
    demonstrateStringManipulation();

    // --- Demo 2: Pointer swap ---
    int x = 42, y = 99;
    swapWithPointers(&x, &y);  // Pass addresses (pointers)

    // --- Demo 3: Classes and Objects ---
    Library myLibrary("Adam's Personal Library");

    // Create books on the heap (using 'new' returns a pointer)
    Book* book1 = new Book("The Pragmatic Programmer", "David Thomas", 352);
    Book* book2 = new Book("Clean Code", "Robert Martin", 464);
    Book* book3 = new Book("Design Patterns", "Gang of Four", 395);

    // Add books to library
    myLibrary.addBook(book1);
    myLibrary.addBook(book2);
    myLibrary.addBook(book3);

    // Interact with books
    cout << "\n📖 Reading session:" << endl;
    book1->read(50);
    book1->read(100);
    book2->read(200);
    book3->read(50);

    // Find and display a specific book
    cout << "\n🔍 Looking for 'Clean Code':" << endl;
    Book* found = myLibrary.findBook("Clean Code");
    if (found != nullptr) {
        cout << "Found it! Progress: " << found->getProgress() << "%" << endl;
    } else {
        cout << "Book not found." << endl;
    }

    // Display all books
    myLibrary.displayBooks();

    // --- Cleanup ---
    // Note: Library destructor will delete all books automatically
    // This demonstrates RAII (Resource Acquisition Is Initialization)
    
    cout << "🎉 Program complete!" << endl;
    return 0;
}
