// C++ Practice - March 20, 2026
// Topic: Classes, Objects, Pointers, and String Manipulation
// This program demonstrates a simple "Student" class with string manipulation and pointers

#include <iostream>
#include <string>
#include <vector>

// ============================================================
// CLASS DEFINITION: Student
// A class represents a student with a name, ID, and grades
// ============================================================
class Student {
private:
    std::string name;      // Student's name (string manipulation demo)
    int id;                // Student ID
    std::vector<int> grades;  // List of grades

public:
    // Constructor - initialize a new Student object
    Student(const std::string& studentName, int studentId) 
        : name(studentName), id(studentId) {
        std::cout << "Created student: " << name << " (ID: " << id << ")" << std::endl;
    }

    // Destructor - cleanup when object is destroyed
    ~Student() {
        std::cout << "Destroying student: " << name << std::endl;
    }

    // Add a grade to the student's record
    void addGrade(int grade) {
        grades.push_back(grade);
    }

    // Calculate and return the average grade
    double getAverage() const {
        if (grades.empty()) return 0.0;
        
        int sum = 0;
        for (int g : grades) {
            sum += g;
        }
        return static_cast<double>(sum) / grades.size();
    }

    // Get student's name (accessor)
    std::string getName() const {
        return name;
    }

    // Convert name to uppercase (string manipulation example)
    std::string getNameUppercase() const {
        std::string upper = name;
        for (char& c : upper) {
            c = std::toupper(c);
        }
        return upper;
    }

    // Display student info
    void displayInfo() const {
        std::cout << "\n--- Student Info ---" << std::endl;
        std::cout << "Name: " << name << std::endl;
        std::cout << "ID: " << id << std::endl;
        std::cout << "Grades: ";
        for (int g : grades) {
            std::cout << g << " ";
        }
        std::cout << std::endl;
        std::cout << "Average: " << getAverage() << std::endl;
    }
};

// ============================================================
// FUNCTION PROTOTYPES
// ============================================================
void demonstratePointers();
void demonstrateStringManipulation();

// ============================================================
// MAIN FUNCTION
// ============================================================
int main() {
    std::cout << "=== C++ Practice: Classes, Pointers, Strings ===" << std::endl;
    std::cout << std::endl;

    // --- Part 1: Creating and using objects ---
    std::cout << "--- Part 1: Working with Objects ---" << std::endl;
    
    // Create a Student object on the stack
    Student student1("Alice Smith", 1001);
    student1.addGrade(85);
    student1.addGrade(90);
    student1.addGrade(78);
    student1.displayInfo();

    // --- Part 2: String manipulation ---
    std::cout << "\n--- Part 2: String Manipulation ---" << std::endl;
    demonstrateStringManipulation();

    // --- Part 3: Pointers ---
    std::cout << "\n--- Part 3: Pointers Demo ---" << std::endl;
    demonstratePointers();

    std::cout << "\n=== Practice Complete! ===" << std::endl;
    return 0;
}

// ============================================================
// FUNCTION: Demonstrate string manipulation
// ============================================================
void demonstrateStringManipulation() {
    std::string original = "Hello, C++ World!";
    
    // String length
    std::cout << "Original: \"" << original << "\"" << std::endl;
    std::cout << "Length: " << original.length() << std::endl;
    
    // Substring
    std::string sub = original.substr(7, 3);  // Start at index 7, length 3
    std::cout << "Substring (7, 3): \"" << sub << "\"" << std::endl;
    
    // Find
    size_t pos = original.find("C++");
    std::cout << "Found \"C++\" at position: " << pos << std::endl;
    
    // Replace
    std::string modified = original;
    modified.replace(7, 3, "Awesome");
    std::cout << "Replaced: \"" << modified << "\"" << std::endl;
    
    // Concatenation
    std::string combined = original + " - Programming is fun!";
    std::cout << "Concatenated: \"" << combined << "\"" << std::endl;
}

// ============================================================
// FUNCTION: Demonstrate pointers
// ============================================================
void demonstratePointers() {
    // Create an object on the heap using 'new' (returns a pointer)
    Student* studentPtr = new Student("Bob Jones", 1002);
    
    // Use the arrow operator (->) to access members through the pointer
    studentPtr->addGrade(92);
    studentPtr->addGrade(88);
    studentPtr->addGrade(95);
    studentPtr->displayInfo();
    
    // Demonstrate pointer to the stack-allocated object from main
    Student stackStudent("Charlie Brown", 1003);
    Student* ptrToStack = &stackStudent;  // Get address of stack object
    
    std::cout << "\nPointer to stack object:" << std::endl;
    std::cout << "Name via pointer: " << ptrToStack->getName() << std::endl;
    std::cout << "Name uppercase: " << ptrToStack->getNameUppercase() << std::endl;
    
    // IMPORTANT: Free the heap memory to prevent memory leaks!
    delete studentPtr;
    std::cout << "\n(Heap-allocated student was deleted)" << std::endl;
}
