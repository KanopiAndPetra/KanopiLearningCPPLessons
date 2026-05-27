// cpp-practice-2026-03-30.cpp
// C++ Practice: Classes, Pointers, and String Manipulation
//
// This program demonstrates:
// - A class with member variables and functions
// - Pointer usage with objects
// - String manipulation

#include <iostream>
#include <string>

// =============================================================================
// CLASS DEFINITION: Book
// A simple class to represent a book with title, author, and page count.
// =============================================================================
class Book {
private:
    std::string title;     // Book title
    std::string author;    // Book author
    int pageCount;         // Number of pages

public:
    // Constructor - initializes a new Book
    Book(std::string t, std::string a, int pages) {
        title = t;
        author = a;
        pageCount = pages;
    }

    // Getter functions
    std::string getTitle() const { return title; }
    std::string getAuthor() const { return author; }
    int getPageCount() const { return pageCount; }

    // Display book info
    void displayInfo() const {
        std::cout << "  Title: \"" << title << "\"\n";
        std::cout << "  Author: " << author << "\n";
        std::cout << "  Pages: " << pageCount << "\n";
    }

    // STRING MANIPULATION: Check if title contains a search term
    bool titleContains(const std::string& searchTerm) const {
        // Convert both to lowercase for case-insensitive comparison
        std::string lowerTitle = title;
        std::string lowerSearch = searchTerm;
        
        for (char& c : lowerTitle) c = std::tolower(c);
        for (char& c : lowerSearch) c = std::tolower(c);
        
        return lowerTitle.find(lowerSearch) != std::string::npos;
    }
};

// =============================================================================
// MAIN FUNCTION
// =============================================================================
int main() {
    std::cout << "=== C++ Practice: Classes, Pointers, and Strings ===\n\n";

    // -------------------------------------------------------------------------
    // PART 1: Creating Objects (on the stack)
    // -------------------------------------------------------------------------
    std::cout << "PART 1: Creating Books (stack-allocated objects)\n";
    std::cout << "------------------------------------------------\n";
    
    Book book1("The Pragmatic Programmer", "Andrew Hunt", 352);
    Book book2("Clean Code", "Robert C. Martin", 464);
    Book book3("Design Patterns", "Gang of Four", 395);
    
    std::cout << "Created 3 books:\n";
    book1.displayInfo();
    std::cout << "\n";
    book2.displayInfo();
    std::cout << "\n";
    book3.displayInfo();
    std::cout << "\n";

    // -------------------------------------------------------------------------
    // PART 2: Pointer Usage
    // -------------------------------------------------------------------------
    std::cout << "PART 2: Using Pointers with Objects\n";
    std::cout << "------------------------------------------------\n";
    
    // Create a pointer to a Book (heap-allocated)
    Book* ptrBook = new Book("Effective C++", "Scott Meyers", 297);
    
    std::cout << "Created book on the heap via pointer:\n";
    std::cout << "  Title via pointer: " << ptrBook->getTitle() << "\n";
    std::cout << "  Author via pointer: " << ptrBook->getAuthor() << "\n";
    std::cout << "  Pages via pointer: " << ptrBook->getPageCount() << "\n";
    
    // Using the dereference operator
    std::cout << "\nAccessing via dereference (*ptrBook):\n";
    ptrBook->displayInfo();
    
    // Don't forget to free the memory!
    delete ptrBook;
    ptrBook = nullptr;  // Good practice: set to null after delete
    std::cout << "\nMemory freed, pointer set to nullptr.\n\n";

    // -------------------------------------------------------------------------
    // PART 3: Array of Pointers to Objects
    // -------------------------------------------------------------------------
    std::cout << "PART 3: Array of Book Pointers\n";
    std::cout << "------------------------------------------------\n";
    
    // Create an array of book pointers
    const int NUM_BOOKS = 3;
    Book* library[NUM_BOOKS] = {
        new Book("1984", "George Orwell", 328),
        new Book("Brave New World", "Aldous Huxley", 288),
        new Book("Fahrenheit 451", "Ray Bradbury", 194)
    };
    
    std::cout << "My small library:\n";
    for (int i = 0; i < NUM_BOOKS; i++) {
        std::cout << "Book " << (i + 1) << ":\n";
        library[i]->displayInfo();
        std::cout << "\n";
    }
    
    // Clean up all heap-allocated books
    for (int i = 0; i < NUM_BOOKS; i++) {
        delete library[i];
        library[i] = nullptr;
    }
    std::cout << "All library books deleted.\n\n";

    // -------------------------------------------------------------------------
    // PART 4: String Manipulation
    // -------------------------------------------------------------------------
    std::cout << "PART 4: String Manipulation Examples\n";
    std::cout << "------------------------------------------------\n";
    
    std::string text = "The quick brown fox jumps over the lazy dog";
    
    std::cout << "Original string:\n";
    std::cout << "  \"" << text << "\"\n\n";
    
    // Find substring
    std::string searchFor = "fox";
    size_t found = text.find(searchFor);
    if (found != std::string::npos) {
        std::cout << "Found \"" << searchFor << "\" at position " << found << "\n";
    }
    
    // Replace substring
    std::string modified = text;
    size_t pos = modified.find("lazy");
    if (pos != std::string::npos) {
        modified.replace(pos, 4, "energetic");
    }
    std::cout << "After replacement:\n";
    std::cout << "  \"" << modified << "\"\n\n";
    
    // Extract substring
    std::string extracted = text.substr(0, 19);  // "The quick brown fox"
    std::cout << "First 19 characters:\n";
    std::cout << "  \"" << extracted << "\"\n\n";
    
    // Using the class's titleContains method
    std::cout << "Testing book search:\n";
    std::cout << "Does \"" << book1.getTitle() << "\" contain \"code\"? ";
    std::cout << (book1.titleContains("code") ? "Yes" : "No") << "\n";
    std::cout << "Does \"" << book1.getTitle() << "\" contain \"pragmatic\"? ";
    std::cout << (book1.titleContains("pragmatic") ? "Yes" : "No") << "\n";

    std::cout << "\n=== Practice Complete! ===\n";
    
    return 0;
}
