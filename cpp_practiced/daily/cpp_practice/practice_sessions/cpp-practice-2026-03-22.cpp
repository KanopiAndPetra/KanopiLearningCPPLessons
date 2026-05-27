// C++ Practice: Classes, Pointers, and String Manipulation
// Date: 2026-03-22
// Topic: Building a simple "Book" class with dynamic memory and string operations

#include <iostream>
#include <string>
#include <vector>

using namespace std;

// ---------------------------------------------------------------
// CLASS: Book
// A simple class demonstrating member variables, methods,
// constructors, and destructor (for pointer management).
// ---------------------------------------------------------------
class Book {
private:
    string title;      // Title of the book
    string author;     // Author's name
    int* pageCountPtr; // Pointer to page count (demonstrates dynamic memory)

public:
    // Constructor with member initializer list
    Book(string t, string a, int pages) : title(t), author(a) {
        pageCountPtr = new int(pages); // Allocate memory on the heap
        cout << "[Book created] \"" << title << "\" by " << author << endl;
    }

    // Copy constructor (demonstrates deep copy)
    Book(const Book& other) {
        title = other.title;
        author = other.author;
        pageCountPtr = new int(*other.pageCountPtr); // Copy the int, not the pointer
        cout << "[Book copied] \"" << title << "\"" << endl;
    }

    // Destructor - cleans up dynamically allocated memory
    ~Book() {
        delete pageCountPtr;
        pageCountPtr = nullptr;
        cout << "[Book destroyed] \"" << title << "\"" << endl;
    }

    // Getter methods
    string getTitle() const { return title; }
    string getAuthor() const { return author; }
    int getPages() const { return *pageCountPtr; }

    // Setter methods
    void setTitle(string newTitle) { title = newTitle; }
    void setAuthor(string newAuthor) { author = newAuthor; }

    // Display book info
    void display() const {
        cout << "  Title:  " << title << endl;
        cout << "  Author: " << author << endl;
        cout << "  Pages:  " << *pageCountPtr << endl;
    }

    // Demonstrate string manipulation: uppercase the title
    void makeTitleUppercase() {
        for (char& c : title) {
            c = toupper(c);
        }
    }

    // Demonstrate string manipulation: check if title contains a word
    bool titleContains(string word) const {
        // Case-insensitive search
        string lowerTitle = title;
        for (char& c : lowerTitle) {
            c = tolower(c);
        }
        string lowerWord = word;
        for (char& c : lowerWord) {
            c = tolower(c);
        }
        return lowerTitle.find(lowerWord) != string::npos;
    }
};

// ---------------------------------------------------------------
// FUNCTION: demonstratePointers
// Shows basic pointer operations with the Book class
// ---------------------------------------------------------------
void demonstratePointers(Book* bookPtr) {
    cout << "\n--- Pointer Demo ---" << endl;
    
    // Arrow operator (->) to access members through pointer
    cout << "Accessing via pointer: " << bookPtr->getTitle() << endl;
    
    // Dereference and use dot operator (equivalent)
    cout << "Via dereference: " << (*bookPtr).getAuthor() << endl;
    
    // Get memory address of pointer
    cout << "Pointer address: " << bookPtr << endl;
}

// ---------------------------------------------------------------
// MAIN
// ---------------------------------------------------------------
int main() {
    cout << "=== C++ Practice: Classes, Pointers & Strings ===" << endl;
    cout << endl;

    // --- 1. Create Book objects (demonstrating classes & constructors) ---
    cout << "--- Creating Books ---" << endl;
    Book book1("The Pragmatic Programmer", "Andrew Hunt", 352);
    Book book2("Clean Code", "Robert C. Martin", 464);

    // --- 2. Display book info ---
    cout << "\n--- Book Details ---" << endl;
    book1.display();
    cout << endl;
    book2.display();

    // --- 3. String manipulation ---
    cout << "\n--- String Manipulation ---" << endl;
    
    book1.makeTitleUppercase();
    cout << "Title after UPPERCASE: \"" << book1.getTitle() << "\"" << endl;
    
    cout << "book2 title contains 'Clean'? " << (book2.titleContains("Clean") ? "Yes" : "No") << endl;
    cout << "book2 title contains 'Code'? " << (book2.titleContains("Code") ? "Yes" : "No") << endl;
    cout << "book2 title contains 'Harry'? " << (book2.titleContains("Harry") ? "Yes" : "No") << endl;

    // --- 4. Pointer demonstration ---
    demonstratePointers(&book1);

    // --- 5. Array of pointers to objects ---
    cout << "\n--- Array of Book Pointers ---" << endl;
    vector<Book*> library;
    library.push_back(&book1);
    library.push_back(&book2);

    cout << "Iterating through library:" << endl;
    for (size_t i = 0; i < library.size(); i++) {
        cout << "  Book " << (i + 1) << ": " << library[i]->getTitle() 
             << " (" << library[i]->getPages() << " pages)" << endl;
    }

    // --- 6. Copy constructor demo ---
    cout << "\n--- Copy Constructor Demo ---" << endl;
    Book book3 = book1; // This triggers the copy constructor
    cout << "Copied book title: \"" << book3.getTitle() << "\"" << endl;

    // --- Cleanup happens automatically when objects go out of scope ---
    cout << "\n--- End of Main (destructors will fire) ---" << endl;

    return 0;
}
