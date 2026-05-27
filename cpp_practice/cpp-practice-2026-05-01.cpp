// cpp-practice-2026-05-01.cpp
// C++ Practice Session - 2026-05-01
// Topic: Classes with Member Variables, Functions, and Multi-Parameter Helpers
// Inspired by: https://github.com/Oppie1/1.1Oppie1CPP (lesson 1.10 - Functions with Multiple Parameters)

#include <iostream>
#include <string>
using namespace std;

// ============================================================================
// STUDENT CLASS
// A class encapsulates data (member variables) and behavior (member functions).
// ============================================================================
class Student {
private:
    // Member variables - data that belongs to each Student object
    string name;
    int studentId;
    int examScores[5];  // Array to store 5 exam scores

public:
    // Constructor - called when a new Student is created
    Student(string n, int id) {
        name = n;
        studentId = id;
        // Initialize all exam scores to 0
        for (int i = 0; i < 5; i++) {
            examScores[i] = 0;
        }
    }

    // Setter for name - member function to modify the name variable
    void setName(string n) {
        name = n;
    }

    // Getter for name - member function to retrieve the name variable
    string getName() {
        return name;
    }

    // Setter for student ID
    void setId(int id) {
        studentId = id;
    }

    // Getter for student ID
    int getId() {
        return studentId;
    }

    // Set an exam score at a specific position (0-4)
    void setExamScore(int examNumber, int score) {
        if (examNumber >= 0 && examNumber < 5) {
            examScores[examNumber] = score;
        }
    }

    // Get an exam score from a specific position
    int getExamScore(int examNumber) {
        if (examNumber >= 0 && examNumber < 5) {
            return examScores[examNumber];
        }
        return -1;  // Return -1 if invalid exam number
    }

    // ========================================================================
    // MULTI-PARAMETER HELPER FUNCTION
    // This function takes 5 int parameters and returns their sum.
    // This mirrors the concept from lesson 1.10 of the repo:
    // "Functions with Multiple Parameters"
    // ========================================================================
    int calculateTotal(int s1, int s2, int s3, int s4, int s5) {
        int total = s1 + s2 + s3 + s4 + s5;
        return total;
    }

    // Calculate the average of all 5 exam scores
    // Uses the multi-parameter helper function internally!
    double calculateAverage() {
        // Get all 5 scores and pass them to the multi-parameter function
        int total = calculateTotal(
            examScores[0],
            examScores[1],
            examScores[2],
            examScores[3],
            examScores[4]
        );
        
        // Divide by number of exams to get average
        double average = static_cast<double>(total) / 5.0;
        return average;
    }

    // Display all student information
    void displayInfo() {
        cout << "Student Name: " << name << endl;
        cout << "Student ID: " << studentId << endl;
        cout << "Exam Scores: ";
        for (int i = 0; i < 5; i++) {
            cout << examScores[i];
            if (i < 4) cout << ", ";
        }
        cout << endl;
        cout << "Average Score: " << calculateAverage() << endl;
    }
};

// ============================================================================
// MAIN FUNCTION
// Creates Student objects, uses member functions, and demonstrates
// how multi-parameter functions work within a class.
// ============================================================================
int main() {
    cout << "========================================" << endl;
    cout << "C++ Practice: Classes & Multi-Parameter Functions" << endl;
    cout << "========================================" << endl;
    cout << endl;

    // Create a new Student object using the constructor
    Student student1("Alice", 1001);

    // Use member functions to set exam scores
    student1.setExamScore(0, 85);
    student1.setExamScore(1, 90);
    student1.setExamScore(2, 78);
    student1.setExamScore(3, 92);
    student1.setExamScore(4, 88);

    // Display the student's information
    cout << "--- Student 1 Info ---" << endl;
    student1.displayInfo();
    cout << endl;

    // Create another student
    Student student2("Bob", 1002);
    student2.setExamScore(0, 70);
    student2.setExamScore(1, 75);
    student2.setExamScore(2, 80);
    student2.setExamScore(3, 85);
    student2.setExamScore(4, 90);

    cout << "--- Student 2 Info ---" << endl;
    student2.displayInfo();
    cout << endl;

    // Demonstrate calling the multi-parameter function directly
    // This is the same concept from the repo: addNumbers(10, 15, 28, 7) = 60
    cout << "--- Direct call to calculateTotal(10, 15, 28, 7, 20) ---" << endl;
    int result = student1.calculateTotal(10, 15, 28, 7, 20);
    cout << "Result: " << result << endl;
    cout << endl;

    cout << "========================================" << endl;
    cout << "Practice session complete!" << endl;
    cout << "========================================" << endl;

    return 0;
}

// ============================================================================
// KEY CONCEPTS PRACTICED:
// 
// 1. CLASS STRUCTURE: 
//    - private: member variables (name, id, examScores)
//    - public: member functions (setters, getters, calculateAverage, displayInfo)
//
// 2. MEMBER VARIABLES vs LOCAL VARIABLES:
//    - Member variables belong to the object and persist across function calls
//    - Local variables (like 'total' in calculateTotal) exist only during that function call
//
// 3. MULTI-PARAMETER FUNCTIONS:
//    - Functions can accept multiple parameters of the same or different types
//    - Arguments passed must match parameter types in order
//    - calculateTotal(int s1, int s2, int s3, int s4, int s5) takes 5 integers
//
// 4. RETURN VALUES:
//    - Functions can return values that replace the function call
//    - calculateTotal returns an int, which is stored in 'total' variable
//    - calculateAverage returns a double
//
// 5. CONSTRUCTORS:
//    - Special function called when an object is created
//    - Initializes member variables to default values
// ============================================================================