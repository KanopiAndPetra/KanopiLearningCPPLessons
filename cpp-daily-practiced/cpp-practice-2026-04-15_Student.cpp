// ============================================================================
// Student.cpp — Implementation of the Student class
// ============================================================================
// The implementation file contains the actual code for methods declared
// in the header. Use the scope resolution operator (::) to connect
// each method to its class.
// ============================================================================

#include <iostream>
#include <iomanip>   // for setprecision
#include "cpp-practice-2026-04-15.h"

using namespace std;

// ============================================================================
// CONSTRUCTOR
// Takes the values passed at object creation and stores them in member vars.
// Example: Student s("Alice", 20, 3.8);
//   -> Creates a Student object with m_name="Alice", m_age=20, m_gpa=3.8
// ============================================================================
Student::Student(string name, int age, double gpa) {
    m_name = name;
    m_age  = age;
    m_gpa  = gpa;
    // No setter calls needed — constructor handles all initialization at once!
    cout << "  [Student created: " << m_name << ", age " << m_age
         << ", GPA " << fixed << setprecision(2) << m_gpa << "]\n";
}

// ============================================================================
// GETTERS
// Return the current value of member variables.
// Marked const so they promise not to modify the object.
// ============================================================================
string Student::getName() const {
    return m_name;
}

int Student::getAge() const {
    return m_age;
}

double Student::getGPA() const {
    return m_gpa;
}

// ============================================================================
// SETTER — updates the GPA
// ============================================================================
void Student::setGPA(double newGPA) {
    if (newGPA < 0.0) {
        cout << "  GPA cannot be negative! Keeping current value.\n";
        return;
    }
    if (newGPA > 4.0) {
        cout << "  GPA capped at 4.0.\n";
        m_gpa = 4.0;
    } else {
        m_gpa = newGPA;
    }
}

// ============================================================================
// getLetterGrade() — converts numeric GPA to a letter grade
// ============================================================================
char Student::getLetterGrade() const {
    if (m_gpa >= 3.7) return 'A';
    if (m_gpa >= 3.3) return 'B';
    if (m_gpa >= 2.7) return 'C';
    if (m_gpa >= 2.0) return 'D';
    return 'F';
}

// ============================================================================
// display() — prints all student info in a formatted table row
// ============================================================================
void Student::display() const {
    cout << "  | " << setw(20) << left << m_name
         << " | " << setw(4)  << m_age
         << " | " << fixed << setprecision(2) << m_gpa
         << "       | " << getLetterGrade() << "    |\n";
}
