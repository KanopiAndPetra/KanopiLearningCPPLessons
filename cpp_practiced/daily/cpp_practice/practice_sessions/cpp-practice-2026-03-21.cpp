// C++ Practice - March 21, 2026
// Topic: Classes, Objects, Pointers, and String Manipulation

#include <iostream>
#include <string>
#include <cctype>
#include <vector>

using namespace std;

// ============================================
// CLASS: StringProcessor
// Demonstrates: member variables, functions, string manipulation
// ============================================
class StringProcessor {
private:
    string originalText;  // Member variable to store the original string

public:
    // Constructor - initializes the string
    StringProcessor(const string& text) {
        originalText = text;
    }

    // Getter: returns the original text
    string getOriginal() const {
        return originalText;
    }

    // String manipulation: convert to uppercase
    string toUpperCase() const {
        string result = originalText;
        for (char& c : result) {
            c = toupper(c);
        }
        return result;
    }

    // String manipulation: convert to lowercase
    string toLowerCase() const {
        string result = originalText;
        for (char& c : result) {
            c = tolower(c);
        }
        return result;
    }

    // String manipulation: reverse the string
    string reverse() const {
        string result = originalText;
        int n = result.length();
        for (int i = 0; i < n / 2; i++) {
            swap(result[i], result[n - 1 - i]);
        }
        return result;
    }

    // String manipulation: count vowels
    int countVowels() const {
        int count = 0;
        string lower = toLowerCase();
        for (char c : lower) {
            if (c == 'a' || c == 'e' || c == 'i' || c == 'o' || c == 'u') {
                count++;
            }
        }
        return count;
    }
};

// ============================================
// FUNCTION: demonstratePointerOperations
// Demonstrates: pointers, pointer arithmetic, dereferencing
// ============================================
void demonstratePointerOperations() {
    cout << "\n=== Pointer Operations ===" << endl;

    // Create a variable and a pointer to it
    int number = 42;
    int* ptr = &number;  // Pointer stores the address of number

    cout << "Value of number: " << number << endl;
    cout << "Address of number: " << ptr << endl;
    cout << "Value via pointer: " << *ptr << endl;

    // Modify value through pointer
    *ptr = 100;
    cout << "After modifying through pointer, number = " << number << endl;

    // Dynamic memory allocation (pointer to new memory)
    int* dynamicArray = new int[5];
    for (int i = 0; i < 5; i++) {
        dynamicArray[i] = i * 10;
    }

    cout << "Dynamic array values: ";
    for (int i = 0; i < 5; i++) {
        cout << dynamicArray[i] << " ";
    }
    cout << endl;

    // Important: Free dynamically allocated memory
    delete[] dynamicArray;
    dynamicArray = nullptr;

    // Pointer to object
    StringProcessor* objPtr = new StringProcessor("Hello Pointer!");
    cout << "Object via pointer: " << objPtr->toUpperCase() << endl;
    delete objPtr;
}

// ============================================
// MAIN FUNCTION
// ============================================
int main() {
    cout << "C++ Practice - Classes, Pointers, Strings" << endl;
    cout << "==========================================" << endl;

    // Create objects (demonstrating class/objects)
    StringProcessor sp1("Hello World!");
    StringProcessor sp2("C++ Programming is Fun!");

    // Use member functions (string manipulation)
    cout << "\n=== String Processing Results ===" << endl;
    cout << "Original: " << sp1.getOriginal() << endl;
    cout << "Uppercase: " << sp1.toUpperCase() << endl;
    cout << "Lowercase: " << sp1.toLowerCase() << endl;
    cout << "Reversed: " << sp1.reverse() << endl;
    cout << "Vowel count: " << sp1.countVowels() << endl;

    cout << "\n--- Another Example ---" << endl;
    cout << "Original: " << sp2.getOriginal() << endl;
    cout << "Uppercase: " << sp2.toUpperCase() << endl;
    cout << "Vowel count: " << sp2.countVowels() << endl;

    // Demonstrate pointers
    demonstratePointerOperations();

    cout << "\n=== Practice Complete! ===" << endl;

    return 0;
}
