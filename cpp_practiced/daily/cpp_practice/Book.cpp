#include <iostream>
#include <string>
#include <vector>
#include "Book.h"
using namespace std;

// Book.cpp — Implementation file
// This is where we define what each method declared in Book.h actually does.
// The :: (scope resolution operator) connects the method to the Book class.

// CONSTRUCTOR: runs automatically when a Book object is created
// Here it uses .assign() to copy the passed-in strings into the member variables
Book::Book(string title, string author, string genre) {
    this->title.assign(title);   // .assign() copies the string — one way to copy strings
    this->author = author;       // = also works for string assignment
    this->genre = genre;
}

// GETTERS: return the private member variables
string Book::getTitle() const {
    return title;
}

string Book::getAuthor() const {
    return author;
}

string Book::getGenre() const {
    return genre;
}

// SETTERS: change the private member variables
void Book::setTitle(string newTitle) {
    title = newTitle;
}

void Book::setAuthor(string newAuthor) {
    author = newAuthor;
}

void Book::setGenre(string newGenre) {
    genre = newGenre;
}

// printAuthorInitials:
// Uses .at() to access individual characters in the author string.
// Assumes author is "FirstName LastName" format — prints first letter of each word.
void Book::printAuthorInitials() const {
    cout << "Author initials: ";
    bool newWord = true; // start of string counts as a new word
    for (size_t i = 0; i < author.length(); i++) {
        // .at(i) returns the character at index i in the string
        if (newWord && author.at(i) != ' ') {
            cout << author.at(i) << ".";
            newWord = false;
        }
        // A space marks the start of a new word (first name -> last name)
        if (author.at(i) == ' ') {
            newWord = true;
        }
    }
    cout << endl;
}

// printTitleBanner:
// Uses .length() to calculate how wide to make the decorative border.
// Prints the title centered inside a box of = characters.
void Book::printTitleBanner() const {
    int width = 50; // total width of the banner
    int padding;

    // .length() returns the number of characters in the string
    padding = (width - title.length()) / 2;

    // Top border
    for (int i = 0; i < width; i++) cout << "=";
    cout << endl;

    // Padding spaces + title
    cout << string(padding, ' ') << title << endl;

    // Bottom border
    for (int i = 0; i < width; i++) cout << "=";
    cout << endl;
}

// copyGenreTo:
// Uses .assign() to copy the genre string into a destination string variable.
// Demonstrates that .assign() is a string class method for copying.
void Book::copyGenreTo(string& dest) const {
    dest.assign(genre); // .assign() copies genre into dest
}
