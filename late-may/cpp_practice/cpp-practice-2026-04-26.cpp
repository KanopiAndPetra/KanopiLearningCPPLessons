// cpp-practice-2026-04-26.cpp
// C++ Practice: Structures + Dynamic Memory Allocation
// 
// Topic: Using structs with heap memory (malloc/free) to build a simple employee database
// Based on learnings from:
//   - 1.47CZTheHeapWNotes (dynamic memory with malloc/free)
//   - 1.49CZStructuresWNotes (struct definitions)

#include <cstdlib>
#include <iostream>
#include <cstring>

// =============================================================================
// STRUCT DEFINITION
// A struct groups related variables together. Here we define an "Employee"
// record with fields for ID, name, and salary.
// =============================================================================
struct Employee {
    int id;                // Employee ID number
    char name[50];         // Employee name (fixed-size C-string, 49 chars + null)
    float salary;          // Annual salary
};

// =============================================================================
// FUNCTION: Create a new employee with user input
// Takes a pointer to an Employee struct and populates its fields
// =============================================================================
void inputEmployee(Employee* emp, int id) {
    emp->id = id;  // Arrow operator (->) dereferences pointer to access member
    
    std::cout << "  Enter name: ";
    std::cin.getline(emp->name, 50);
    
    // If the user enters nothing, give a default name
    if (strlen(emp->name) == 0) {
        strcpy(emp->name, "Unknown");
    }
    
    std::cout << "  Enter salary: ";
    std::cin >> emp->salary;
    std::cin.ignore();  // Clear the newline character from input buffer
}

// =============================================================================
// FUNCTION: Display a single employee
// Takes a pointer to an Employee and prints its data
// =============================================================================
void displayEmployee(Employee* emp) {
    std::cout << "  ID #" << emp->id << " | " << emp->name 
              << " | $" << emp->salary << std::endl;
}

// =============================================================================
// MAIN PROGRAM
// Demonstrates dynamic memory allocation by creating an employee array on the heap
// =============================================================================
int main() {
    std::cout << "=== C++ Practice: Structures + Dynamic Memory ===" << std::endl;
    std::cout << "Learning how to use struct + malloc/free with heap memory" << std::endl;
    std::cout << std::endl;
    
    // -------------------------------------------------------------------------
    // STEP 1: Ask user how many employees to create
    // -------------------------------------------------------------------------
    int numEmployees;
    std::cout << "How many employees to create? ";
    std::cin >> numEmployees;
    std::cin.ignore();  // Remove trailing newline
    
    // -------------------------------------------------------------------------
    // STEP 2: Allocate memory on the HEAP for the employee array
    // malloc() returns a void*, so we cast it to Employee*
    // sizeof(Employee) * numEmployees calculates total bytes needed
    // -------------------------------------------------------------------------
    Employee* employees = (Employee*)malloc(sizeof(Employee) * numEmployees);
    
    // Always check if malloc succeeded (returned NULL on failure)
    if (employees == NULL) {
        std::cout << "ERROR: Failed to allocate memory!" << std::endl;
        return 1;  // Exit with error code
    }
    
    std::cout << "\nAllocated " << sizeof(Employee) * numEmployees 
              << " bytes on the heap for " << numEmployees << " employees." << std::endl;
    
    // -------------------------------------------------------------------------
    // STEP 3: Fill in employee data using our input function
    // -------------------------------------------------------------------------
    std::cout << "\n--- Enter Employee Data ---" << std::endl;
    for (int i = 0; i < numEmployees; i++) {
        std::cout << "\nEmployee #" << (i + 1) << ":" << std::endl;
        inputEmployee(&employees[i], i + 1);  // Pass address of each array element
    }
    
    // -------------------------------------------------------------------------
    // STEP 4: Display all employees
    // -------------------------------------------------------------------------
    std::cout << "\n--- All Employees ---" << std::endl;
    float totalSalary = 0;
    for (int i = 0; i < numEmployees; i++) {
        displayEmployee(&employees[i]);
        totalSalary += employees[i].salary;
    }
    
    std::cout << "\nTotal payroll: $" << totalSalary << std::endl;
    std::cout << "Average salary: $" << (totalSalary / numEmployees) << std::endl;
    
    // -------------------------------------------------------------------------
    // STEP 5: FREE the heap memory when done (prevents memory leak!)
    // -------------------------------------------------------------------------
    free(employees);
    std::cout << "\nMemory freed successfully. Good job!" << std::endl;
    
    return 0;
}

/*
================================================================================
KEY CONCEPTS DEMONSTRATED:
================================================================================

1. STRUCT (lines 14-18):
   - Grouping related data (id, name, salary) into one type
   - Like a custom data type for representing real-world entities

2. POINTERS TO STRUCTS (lines 29, 45, 56):
   - Arrow operator (->) used to access members through a pointer
   - Example: emp->id is equivalent to (*emp).id

3. DYNAMIC MEMORY ALLOCATION (lines 77-79):
   - malloc() requests memory from the heap (not stack)
   - Heap memory persists until explicitly freed with free()
   - Stack memory is automatic; heap memory is manual

4. MEMORY LEAK PREVENTION (lines 93-94):
   - Every malloc() should have a corresponding free()
   - Memory leaks happen when you forget to free memory

5. INPUT HANDLING:
   - Using std::cin.getline() for C-strings
   - std::cin.ignore() to clear input buffer after numeric input

6. ADDRESS-OF OPERATOR (&):
   - Used when passing struct variables to functions that expect pointers
   - Example: &employees[i] passes the address of the struct element

================================================================================
WHY THIS MATTERS:
================================================================================
- Structures let you model real-world data (employees, students, products)
- Heap memory lets you handle data of unknown size at compile time
- Together, they're the foundation for linked lists, trees, and dynamic arrays
================================================================================
*/