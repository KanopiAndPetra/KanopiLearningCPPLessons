#pragma once
#include <string>
using std::string;

// Book.h — Header file (declaration/blueprint)
// This declares what the Book class contains.
// The actual code for these methods lives in Book.cpp.

class Book {
public:
    // Constructor — takes title, author, and genre when a Book object is created.
    // It runs automatically when we write: Book myBook("Title", "Author", "Genre");
    Book(string title, string author, string genre);

    // Getters — let us read private member variables from outside the class
    string getTitle() const;
    string getAuthor() const;
    string getGenre() const;

    // Setters — let us change the private member variables
    void setTitle(string newTitle);
    void setAuthor(string newAuthor);
    void setGenre(string newGenre);  // setter for genre

    // Utility functions using string class methods
    void printAuthorInitials() const;      // Uses .at() to print first letter of each word in author name
    void printTitleBanner() const;          // Uses .length() to center and decorate the title
    void copyGenreTo(string& dest) const;  // Uses .assign() to copy the genre string

private:
    // Member variables — these "belong" to each Book object
    string title;
    string author;
    string genre;
};
