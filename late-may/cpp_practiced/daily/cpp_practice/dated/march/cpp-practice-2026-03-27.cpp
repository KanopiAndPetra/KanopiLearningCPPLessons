// C++ Practice: String Manipulation with Pointers and a Simple Class
// Date: 2026-03-27
// Topics: Classes/Objects, Pointers, String Manipulation
//
// This program demonstrates:
// 1. A simple class with member variables and functions
// 2. Pointer usage to access and modify objects
// 3. String manipulation using C++ string methods

#include <iostream>
#include <string>
#include <vector>

// ============================================================
// CLASS: Book
// Represents a simple book with title, author, and page count
// ============================================================
class Book {
private:
    std::string title;      // Book title
    std::string author;     // Author name
    int pageCount;          // Number of pages

public:
    // Constructor - initializes a new Book
    Book(std::string t, std::string a, int pages)
        : title(t), author(a), pageCount(pages) {}

    // Getter for title
    std::string getTitle() const {
        return title;
    }

    // Getter for author
    std::string getAuthor() const {
        return author;
    }

    // Getter for page count
    int getPageCount() const {
        return pageCount;
    }

    // Display book info
    void display() const {
        std::cout << "Title: \"" << title << "\"\n";
        std::cout << "Author: " << author << "\n";
        std::cout << "Pages: " << pageCount << "\n";
    }

    // STRING MANIPULATION: Get a formatted summary
    // Uses string methods: substr(), length(), find()
    std::string getSummary() const {
        std::string summary = title + " by " + author;
        
        // Demonstrate string::length()
        int titleLength = title.length();
        
        // Demonstrate string::find()
        size_t spacePos = author.find(' ');
        std::string lastName = (spacePos != std::string::npos) 
            ? author.substr(spacePos + 1) 
            : author;
        
        return summary + " (" + std::to_string(pageCount) + " pages)";
    }
};

// ============================================================
// FUNCTION: Demonstrate Pointer Operations
// ============================================================
void demonstratePointers() {
    std::cout << "=== Pointer Demonstration ===\n";
    
    // Create a book on the stack
    Book book1("The Pragmatic Programmer", "Dave Thomas", 352);
    
    // Pointer to the book object
    Book* bookPtr = &book1;
    
    // Access object through pointer using dereference
    std::cout << "Accessing via pointer:\n";
    std::cout << "Title: " << bookPtr->getTitle() << "\n";
    std::cout << "Author: " << bookPtr->getAuthor() << "\n\n";
    
    // Dynamic allocation (heap) - remember to delete!
    Book* dynamicBook = new Book("Clean Code", "Robert Martin", 464);
    std::cout << "Dynamically allocated book:\n";
    dynamicBook->display();
    std::cout << "\n";
    
    // Clean up dynamically allocated memory
    delete dynamicBook;
    std::cout << "Dynamic book deleted.\n\n";
}

// ============================================================
// FUNCTION: String Manipulation Examples
// ============================================================
void demonstrateStringManipulation() {
    std::cout << "=== String Manipulation Demo ===\n";
    
    std::string text = "Hello, C++ Programming World!";
    
    // length() - get string length
    std::cout << "Original: \"" << text << "\"\n";
    std::cout << "Length: " << text.length() << "\n";
    
    // find() - find substring position
    size_t pos = text.find("C++");
    std::cout << "\"C++\" found at position: " << pos << "\n";
    
    // substr() - extract substring
    std::string extracted = text.substr(0, 5);
    std::cout << "First 5 chars: \"" << extracted << "\"\n";
    
    // replace() - replace substring
    std::string modified = text;
    modified.replace(0, 5, "Greetings");
    std::cout << "After replace: \"" << modified << "\"\n";
    
    // append() - concatenate strings
    std::string appended = "C++";
    appended.append(" is awesome!");
    std::cout << "Appended: \"" << appended << "\"\n\n";
}

// ============================================================
// MAIN: Entry Point
// ============================================================
int main() {
    std::cout << "╔══════════════════════════════════════════╗\n";
    std::cout << "║   C++ Practice: Classes, Pointers,       ║\n";
    std::cout << "║   and String Manipulation                ║\n";
    std::cout << "║   Date: 2026-03-27                       ║\n";
    std::cout << "╚══════════════════════════════════════════╝\n\n";
    
    // Demonstrate string manipulation
    demonstrateStringManipulation();
    
    // Demonstrate pointers with the Book class
    demonstratePointers();
    
    // Create and use Book objects
    std::cout << "=== Book Class Demo ===\n";
    Book myBook("1984", "George Orwell", 328);
    myBook.display();
    std::cout << "\nSummary: " << myBook.getSummary() << "\n\n";
    
    // Vector of Book pointers (demonstrates pointer arrays)
    std::cout << "=== Book Collection (using pointers) ===\n";
    std::vector<Book*> library;
    library.push_back(new Book("Brave New World", "Aldous Huxley", 268));
    library.push_back(new Book("Fahrenheit 451", "Ray Bradbury", 194));
    library.push_back(new Book("Dune", "Frank Herbert", 412));
    
    for (size_t i = 0; i < library.size(); i++) {
        std::cout << (i + 1) << ". " << library[i]->getSummary() << "\n";
    }
    
    // Clean up all dynamically allocated books
    for (Book* b : library) {
        delete b;
    }
    
    std::cout << "\nAll resources cleaned up. Goodbye!\n";
    return 0;
}
