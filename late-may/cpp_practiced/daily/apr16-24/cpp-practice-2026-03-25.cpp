// cpp-practice-2026-03-25.cpp
// C++ Practice: Classes, Pointers, and String Manipulation
// 
// This program creates a simple "TextAnalyzer" class that:
// - Stores a string (uses pointers for internal storage)
// - Provides string manipulation methods
// - Demonstrates class/object concepts

#include <iostream>
#include <cstring>
#include <cctype>

// ============================================================================
// TextAnalyzer Class
// A simple class demonstrating encapsulation, pointers, and string operations
// ============================================================================
class TextAnalyzer {
private:
    // Using a pointer for dynamic string storage (demonstrates pointer usage)
    char* textData;
    int length;

public:
    // Constructor - allocates memory and copies the input string
    TextAnalyzer(const char* input) {
        length = strlen(input);
        textData = new char[length + 1];  // +1 for null terminator
        strcpy(textData, input);
        std::cout << "[TextAnalyzer] Created with: \"" << textData << "\"\n";
    }

    // Destructor - frees dynamically allocated memory (prevents memory leaks!)
    ~TextAnalyzer() {
        std::cout << "[TextAnalyzer] Destroying: \"" << textData << "\"\n";
        delete[] textData;
    }

    // Get the current stored text
    const char* getText() const {
        return textData;
    }

    // Get the length of stored text
    int getLength() const {
        return length;
    }

    // Count vowels in the stored text
    int countVowels() const {
        int count = 0;
        for (int i = 0; i < length; i++) {
            char c = tolower(textData[i]);
            if (c == 'a' || c == 'e' || c == 'i' || c == 'o' || c == 'u') {
                count++;
            }
        }
        return count;
    }

    // Count consonants in the stored text
    int countConsonants() const {
        int count = 0;
        for (int i = 0; i < length; i++) {
            char c = tolower(textData[i]);
            if (isalpha(c) && !(c == 'a' || c == 'e' || c == 'i' || c == 'o' || c == 'u')) {
                count++;
            }
        }
        return count;
    }

    // Convert text to UPPERCASE (demonstrates in-place string manipulation)
    void toUpperCase() {
        for (int i = 0; i < length; i++) {
            textData[i] = toupper(textData[i]);
        }
    }

    // Convert text to lowercase
    void toLowerCase() {
        for (int i = 0; i < length; i++) {
            textData[i] = tolower(textData[i]);
        }
    }

    // Reverse the string (demonstrates pointer arithmetic)
    void reverse() {
        char* start = textData;
        char* end = textData + length - 1;
        while (start < end) {
            char temp = *start;
            *start = *end;
            *end = temp;
            start++;
            end--;
        }
    }

    // Check if text is a palindrome
    bool isPalindrome() const {
        int left = 0;
        int right = length - 1;
        while (left < right) {
            if (tolower(textData[left]) != tolower(textData[right])) {
                return false;
            }
            left++;
            right--;
        }
        return true;
    }
};

// ============================================================================
// Demonstrate pointer concepts with a simple function
// ============================================================================
void demonstratePointers() {
    std::cout << "\n--- Pointer Demonstration ---\n";
    
    int numbers[] = {10, 20, 30, 40, 50};
    int* ptr = numbers;  // Pointer to array's first element
    
    std::cout << "Array elements via pointer arithmetic:\n";
    for (int i = 0; i < 5; i++) {
        std::cout << "  *(ptr + " << i << ") = " << *(ptr + i) << "\n";
    }
    
    // Pointer to TextAnalyzer object
    std::cout << "\nPointer to object address: " << ptr << "\n";
    std::cout << "Value at pointer: " << *ptr << "\n";
}

// ============================================================================
// Main function - entry point
// ============================================================================
int main() {
    std::cout << "=========================================\n";
    std::cout << "  C++ Practice: Classes, Pointers, Strings\n";
    std::cout << "=========================================\n\n";

    // Create TextAnalyzer objects (demonstrates class instantiation)
    TextAnalyzer analyzer1("Hello World");
    TextAnalyzer analyzer2("A man a plan a canal Panama");

    std::cout << "\n--- Analyzing: \"" << analyzer1.getText() << "\" ---\n";
    std::cout << "Length: " << analyzer1.getLength() << "\n";
    std::cout << "Vowels: " << analyzer1.countVowels() << "\n";
    std::cout << "Consonants: " << analyzer1.countConsonants() << "\n";
    
    analyzer1.toUpperCase();
    std::cout << "UPPERCASE: " << analyzer1.getText() << "\n";
    
    analyzer1.reverse();
    std::cout << "Reversed: " << analyzer1.getText() << "\n";

    std::cout << "\n--- Analyzing: \"" << analyzer2.getText() << "\" ---\n";
    std::cout << "Is Palindrome? " << (analyzer2.isPalindrome() ? "YES" : "NO") << "\n";

    // Demonstrate pointer concepts
    demonstratePointers();

    std::cout << "\n=========================================\n";
    std::cout << "  Program complete! Objects will be\n";
    std::cout << "  destroyed automatically via destructor\n";
    std::cout << "=========================================\n";

    return 0;
}
