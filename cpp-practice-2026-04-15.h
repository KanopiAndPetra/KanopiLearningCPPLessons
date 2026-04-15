#pragma once
// ============================================================================
// Student.h — Header file for the Student class
// ============================================================================
// A class declaration lives in the header (.h) file.
// This tells the compiler WHAT the class has (members + methods).
// The actual HOW (implementation) lives in Student.cpp.
// ============================================================================

#include <string>

class Student {
public:
    // --------------------------------------------------------------------
    // CONSTRUCTOR — same name as the class, no return type.
    // Called automatically when an object is created.
    // Takes three parameters and initializes all member variables at once.
    // --------------------------------------------------------------------
    Student(std::string name, int age, double gpa);

    // --------------------------------------------------------------------
    // GETTERS — read-only access to member variables
    // --------------------------------------------------------------------
    std::string getName() const;   // const means these don't modify the object
    int         getAge()  const;
    double      getGPA()  const;

    // --------------------------------------------------------------------
    // SETTERS — modify member variables
    // --------------------------------------------------------------------
    void setGPA(double newGPA);

    // --------------------------------------------------------------------
    // OTHER METHODS
    // --------------------------------------------------------------------
    // Returns a letter grade based on the GPA value
    char getLetterGrade() const;

    // Prints the student's info to stdout
    void display() const;

private:
    // Member variables — only accessible inside the class
    std::string m_name;
    int         m_age;
    double      m_gpa;
};
