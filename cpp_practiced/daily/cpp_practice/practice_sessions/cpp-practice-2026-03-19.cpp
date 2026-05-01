// C++ Practice - March 19, 2026
// Topic: Classes, Pointers, and String Manipulation
// Based on lessons from 1.1ClaudeCPP tutorial series

#include <iostream>
#include <string>
#include <vector>

using namespace std;

// ============================================
// CLASS: Book - demonstrates OOP fundamentals
// ============================================
class Book {
private:
    string title;      // Member variable - stores book title
    string author;     // Member variable - stores author name
    int pages;         // Member variable - number of pages
    string* pTitle;    // Pointer to title - demonstrates pointer usage

public:
    // Constructor - initializes the Book object
    Book(string t, string a, int p) {
        title = t;
        author = a;
        pages = p;
        pTitle = &title;  // Point to the title string
        cout << "Book created: \"" << title << "\"" << endl;
    }

    // Destructor - cleanup when object is destroyed
    ~Book() {
        cout << "Book destroyed: \"" << title << "\"" << endl;
    }

    // Getter - returns the book title
    string getTitle() {
        return title;
    }

    // Setter - updates the book title
    void setTitle(string newTitle) {
        title = newTitle;
        pTitle = &title;  // Update pointer to point to new title
    }

    // Function demonstrating string manipulation
    string getFormattedInfo() {
        // Using string methods: length(), substr(), find()
        string info = title + " by " + author;
        
        // Convert pages to string and append
        // Using to_string() function
        info += " (" + to_string(pages) + " pages)";
        
        return info;
    }

    // Function demonstrating pointer usage
    void displayTitleViaPointer() {
        cout << "Title via pointer: " << *pTitle << endl;
        cout << "Pointer address: " << pTitle << endl;
    }

    // Function demonstrating string finding
    bool containsWord(string word) {
        // Using string::find() - returns position or string::npos if not found
        // Converting to lowercase for case-insensitive search
        string lowerTitle = title;
        string lowerWord = word;
        
        // Manual lowercase conversion (demonstrates string iteration)
        for (int i = 0; i < lowerTitle.length(); i++) {
            lowerTitle[i] = tolower(lowerTitle[i]);
        }
        for (int i = 0; i < lowerWord.length(); i++) {
            lowerWord[i] = tolower(lowerWord[i]);
        }
        
        return lowerTitle.find(lowerWord) != string::npos;
    }
};

// ============================================
// FUNCTION: Demonstrates pass-by-reference with pointers
// ============================================
void updateBookPointer(Book* book, string newTitle) {
    // Arrow operator (->) to access members through pointer
    book->setTitle(newTitle);
    cout << "  [Pointer function] Updated title to: " << newTitle << endl;
}

// ============================================
// MAIN: Program entry point
// ============================================
int main() {
    cout << "=== C++ Practice: Classes, Pointers, Strings ===" << endl << endl;

    // --------------------------------------------
    // Part 1: Creating objects (classes/objects)
    // --------------------------------------------
    cout << "--- Part 1: Creating Book Objects ---" << endl;
    
    Book book1("The C++ Programming Language", "Bjarne Stroustrup", 1360);
    Book book2("Clean Code", "Robert C. Martin", 464);
    
    cout << endl;

    // --------------------------------------------
    // Part 2: Using member functions
    // --------------------------------------------
    cout << "--- Part 2: Using Member Functions ---" << endl;
    cout << "Book 1 info: " << book1.getFormattedInfo() << endl;
    cout << "Book 2 info: " << book2.getFormattedInfo() << endl;
    cout << endl;

    // --------------------------------------------
    // Part 3: Pointer demonstration
    // --------------------------------------------
    cout << "--- Part 3: Pointers ---" << endl;
    
    // Creating a pointer to a Book object
    Book* pBook = &book1;
    
    // Using arrow operator to access members through pointer
    cout << "Accessing via pointer: " << pBook->getTitle() << endl;
    
    // Calling function that uses pointers
    updateBookPointer(pBook, "C++: The Complete Reference");
    cout << "Updated book1 title: " << book1.getTitle() << endl;
    cout << endl;

    // Display title using the internal pointer
    cout << "--- Using Internal Pointer ---" << endl;
    book1.displayTitleViaPointer();
    cout << endl;

    // --------------------------------------------
    // Part 4: String manipulation
    // --------------------------------------------
    cout << "--- Part 4: String Manipulation ---" << endl;
    
    string sample = "Learning C++ is fun and rewarding!";
    
    // Demonstrating string::length()
    cout << "String length: " << sample.length() << endl;
    
    // Demonstrating string::substr()
    cout << "Substring (0-15): " << sample.substr(0, 15) << endl;
    
    // Demonstrating string::find()
    size_t pos = sample.find("fun");
    if (pos != string::npos) {
        cout << "Found 'fun' at position: " << pos << endl;
    }
    
    // Using the class's containsWord function
    cout << "book1 contains 'C++': " << (book1.containsWord("C++") ? "Yes" : "No") << endl;
    cout << "book1 contains 'Python': " << (book1.containsWord("Python") ? "Yes" : "No") << endl;
    cout << endl;

    // --------------------------------------------
    // Part 5: Dynamic memory (new/delete)
    // --------------------------------------------
    cout << "--- Part 5: Dynamic Memory ---" << endl;
    
    // Creating object dynamically with 'new'
    Book* dynamicBook = new Book("Effective C++", "Scott Meyers", 297);
    cout << "Dynamic book info: " << dynamicBook->getFormattedInfo() << endl;
    
    // Don't forget to delete! (destructor will be called)
    delete dynamicBook;
    cout << "Dynamic book deleted." << endl;
    cout << endl;

    // --------------------------------------------
    // Part 6: Vector of pointers (STL)
    // --------------------------------------------
    cout << "--- Part 6: Vector of Pointers ---" << endl;
    
    vector<Book*> bookCollection;
    bookCollection.push_back(&book1);
    bookCollection.push_back(&book2);
    
    cout << "Books in collection:" << endl;
    for (int i = 0; i < bookCollection.size(); i++) {
        cout << "  " << (i + 1) << ". " << bookCollection[i]->getTitle() << endl;
    }
    cout << endl;

    // --------------------------------------------
    // Summary
    // --------------------------------------------
    cout << "=== Practice Complete! ===" << endl;
    cout << "Concepts demonstrated:" << endl;
    cout << "  - Classes with member variables and functions" << endl;
    cout << "  - Constructors and destructors" << endl;
    cout << "  - Pointers and pointer arithmetic" << endl;
    cout << "  - Arrow operator (->) for pointer access" << endl;
    cout << "  - String methods: length(), substr(), find()" << endl;
    cout << "  - Dynamic memory allocation with new/delete" << endl;
    cout << "  - Vectors (STL container)" << endl;

    return 0;
}