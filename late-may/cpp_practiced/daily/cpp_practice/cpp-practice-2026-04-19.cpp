#include <iostream>
#include <string>
#include <vector>
#include "Book.h"
using namespace std;

// cpp-practice-2026-04-19.cpp
// Main file — creates Book objects and demonstrates:
//   - Classes in separate files (Book.h + Book.cpp)
//   - Constructor that takes parameters
//   - String class methods: .length(), .at(), getline(), .assign()
//   - Getters, setters, and utility methods

int main() {

    cout << "========================================" << endl;
    cout << "  C++ PRACTICE 2026-04-19" << endl;
    cout << "  Classes, Constructors & String Methods" << endl;
    cout << "========================================" << endl << endl;

    // ============================================================
    // PART 1: Create Book objects using the constructor
    // When we call Book("title", "author", "genre"), the constructor
    // runs automatically and initializes the member variables.
    // ============================================================

    cout << "--- Part 1: Books Created with Constructor ---" << endl << endl;

    Book book1("The Pragmatic Programmer", "David Thomas", "Technology");
    Book book2("Dune", "Frank Herbert", "Science Fiction");
    Book book3("Clean Code", "Robert Martin", "Technology");

    // Print each book using our printTitleBanner() and printAuthorInitials() methods
    book1.printTitleBanner();
    book1.printAuthorInitials();
    cout << "Genre: " << book1.getGenre() << endl << endl;

    book2.printTitleBanner();
    book2.printAuthorInitials();
    cout << "Genre: " << book2.getGenre() << endl << endl;

    book3.printTitleBanner();
    book3.printAuthorInitials();
    cout << "Genre: " << book3.getGenre() << endl << endl;

    // ============================================================
    // PART 2: String class methods
    // Demonstrates .length(), .at(), .assign(), and getline()
    // ============================================================

    cout << "--- Part 2: String Class Methods Demo ---" << endl << endl;

    // .length() — returns how many characters are in the string
    string sample = "Hello, World!";
    cout << "String: \"" << sample << "\"" << endl;
    cout << ".length() = " << sample.length() << " characters" << endl;

    // .at() — accesses character at a specific index (0-based)
    cout << ".at(0) = '" << sample.at(0) << "'" << endl;
    cout << ".at(7) = '" << sample.at(7) << "'" << endl;
    cout << ".at(12) = '" << sample.at(12) << "'" << endl << endl;

    // .assign() — copies one string into another
    string source = "C++ is awesome";
    string destination;
    destination.assign(source); // copies source into destination
    cout << "Before .assign(): destination = \"" << destination << "\"" << endl;
    cout << ".assign() copied the source string." << endl << endl;

    // ============================================================
    // PART 3: getline() vs cin
    // cin >> only reads one word (stops at first whitespace)
    // getline(cin, var) reads the entire line including spaces
    // ============================================================

    cout << "--- Part 3: getline() vs cin ---" << endl << endl;

    // Demonstrate cin stopping at whitespace
    string wordInput;
    cout << "Enter a ONE-word answer to 'Best language?' : ";
    cin >> wordInput;
    cout << "cin >> stored: \"" << wordInput << "\"" << endl << endl;

    // Consume the leftover newline before using getline
    cin.ignore(numeric_limits<streamsize>::max(), '\n');

    // Demonstrate getline reading full sentences
    string fullSentence;
    cout << "Enter your FULL opinion about programming: " << endl;
    getline(cin, fullSentence);  // reads entire line including spaces
    cout << "getline() stored: \"" << fullSentence << "\"" << endl;
    cout << "Length of your response: " << fullSentence.length() << " characters" << endl << endl;

    // ============================================================
    // PART 4: User creates their own Book
    // Demonstrates setters and copying genre with .assign()
    // ============================================================

    cout << "--- Part 4: Build Your Own Book ---" << endl << endl;

    Book userBook("???", "???", "???");

    // No extra ignore needed here — Part 3's getline() already consumed its newline,
    // so the stream is clean and ready for the next getline call.

    string uTitle, uAuthor, uGenre;
    cout << "Enter book title: ";
    getline(cin, uTitle);
    userBook.setTitle(uTitle);

    cout << "Enter author name: ";
    getline(cin, uAuthor);
    userBook.setAuthor(uAuthor);

    cout << "Enter genre: ";
    getline(cin, uGenre);
    userBook.setGenre(uGenre);  // update the book's genre from user input

    // Use .assign() to copy uGenre into the book's genre via our method
    string genreCopy;
    userBook.copyGenreTo(genreCopy);  // demonstrates .assign() internally

    cout << endl;
    userBook.printTitleBanner();
    userBook.printAuthorInitials();
    cout << "Genre: " << userBook.getGenre() << endl;
    cout << "Genre (via .assign() copy): " << genreCopy << endl << endl;

    cout << "========================================" << endl;
    cout << "  Program complete!" << endl;
    cout << "========================================" << endl;

    return 0;
}
