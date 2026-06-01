// C++ Practice: Book Library Manager
// Topics covered: Classes/Objects, Pointers, String Manipulation
// Date: 2026-03-26

#include <iostream>
#include <string>
#include <vector>

using namespace std;

// ---------------------------------------------------------
// Book class - represents a single book with title, author
// and a pointer to the next book (linked list structure)
// ---------------------------------------------------------
class Book {
private:
    string title;
    string author;
    int yearPublished;
    Book* nextBook;  // Pointer to next book in our list

public:
    // Constructor
    Book(string t, string a, int year) {
        title = t;
        author = a;
        yearPublished = year;
        nextBook = nullptr;  // Initialize pointer to null
    }

    // Getters
    string getTitle() { return title; }
    string getAuthor() { return author; }
    int getYear() { return yearPublished; }

    // Set the pointer to the next book
    void setNext(Book* next) {
        nextBook = next;
    }

    // Get the next book pointer
    Book* getNext() {
        return nextBook;
    }

    // Display book info
    void display() {
        cout << "  \"" << title << "\" by " << author
             << " (" << yearPublished << ")" << endl;
    }

    // Search within the title using string manipulation
    bool titleContains(string searchTerm) {
        // Convert both to lowercase for case-insensitive search
        string lowerTitle = title;
        string lowerTerm = searchTerm;
        
        for (int i = 0; i < lowerTitle.length(); i++) {
            lowerTitle[i] = tolower(lowerTitle[i]);
        }
        for (int i = 0; i < lowerTerm.length(); i++) {
            lowerTerm[i] = tolower(lowerTerm[i]);
        }
        
        // Check if the search term appears in the title
        return lowerTitle.find(lowerTerm) != string::npos;
    }
};

// ---------------------------------------------------------
// Library class - manages a collection of books using
// a linked list structure with pointers
// ---------------------------------------------------------
class Library {
private:
    Book* head;  // Pointer to first book in the list

public:
    Library() {
        head = nullptr;
    }

    // Add a book to the front of the list
    void addBook(string title, string author, int year) {
        Book* newBook = new Book(title, author, year);  // Dynamic allocation
        newBook->setNext(head);
        head = newBook;
    }

    // Display all books in the library
    void displayAll() {
        cout << "\n📚 Your Library:" << endl;
        cout << "----------------" << endl;
        
        Book* current = head;  // Start at head
        int count = 0;
        
        while (current != nullptr) {
            cout << (count + 1) << ". ";
            current->display();
            current = current->getNext();  // Move to next using pointer
            count++;
        }
        
        if (count == 0) {
            cout << "  (No books in library yet)" << endl;
        } else {
            cout << "----------------" << endl;
            cout << "Total books: " << count << endl;
        }
    }

    // Search for books by title keyword
    void searchByTitle(string keyword) {
        cout << "\n🔍 Searching for \"" << keyword << "\" in titles..." << endl;
        
        Book* current = head;
        bool found = false;
        
        while (current != nullptr) {
            if (current->titleContains(keyword)) {
                current->display();
                found = true;
            }
            current = current->getNext();
        }
        
        if (!found) {
            cout << "  No books found matching that search." << endl;
        }
    }

    // Clean up memory when done
    ~Library() {
        Book* current = head;
        while (current != nullptr) {
            Book* next = current->getNext();
            delete current;  // Free dynamically allocated memory
            current = next;
        }
    }
};

// ---------------------------------------------------------
// Main function - demonstrates the classes and pointers
// ---------------------------------------------------------
int main() {
    cout << "=== C++ Practice: Book Library Manager ===" << endl;
    cout << "Demonstrating: Classes, Pointers, and String Manipulation" << endl;
    
    // Create a library instance (an object)
    Library myLibrary;
    
    // Add some books to our library
    myLibrary.addBook("The C++ Programming Language", "Bjarne Stroustrup", 2013);
    myLibrary.addBook("Effective Modern C++", "Scott Meyers", 2015);
    myLibrary.addBook("Clean Code", "Robert C. Martin", 2008);
    myLibrary.addBook("Design Patterns", "Gang of Four", 1994);
    myLibrary.addBook("C++ Concurrency in Action", "Anthony Williams", 2012);
    
    // Display all books
    myLibrary.displayAll();
    
    // Demonstrate string manipulation - searching for books
    cout << "\n--- String Search Demo ---" << endl;
    myLibrary.searchByTitle("C++");
    myLibrary.searchByTitle("code");
    myLibrary.searchByTitle("python");  // Should find nothing
    
    cout << "\n=== End of Program ===" << endl;
    
    // Note: Library destructor will clean up all Book objects
    // demonstrating proper memory management with pointers
    
    return 0;
}
