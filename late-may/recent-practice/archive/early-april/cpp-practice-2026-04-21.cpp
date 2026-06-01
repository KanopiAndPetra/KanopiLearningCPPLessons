// cpp-practice-2026-04-21.cpp
// Topics: Exception Handling + Template Specialization
// Repos studied: 1.1Oppie1CPP (lessons 1.60, 1.61, 1.62)

#include <iostream>
#include <string>
#include <stdexcept>
using namespace std;

// ============================================================
// TEMPLATE SPECIALIZATION
// A generic Container class that works with any type.
// The char specialization acts differently — it counts vowels!
// ============================================================

// Generic template — works with int, double, float, etc.
template<typename T>
class Container {
private:
    T value;
public:
    Container(T v) : value(v) {}
    T getValue() const { return value; }
    void describe() const {
        cout << "  Generic container holds: " << value << endl;
    }
};

// Template specialization for char — has its own custom behavior!
template<>
class Container<char> {
private:
    char value;
public:
    Container(char v) : value(v) {}
    char getValue() const { return value; }
    void describe() const {
        cout << "  Char container holds: '" << value << "'" << endl;
        // Bonus: tell us if it's a vowel
        char lower = tolower(value);
        bool isVowel = (lower == 'a' || lower == 'e' || lower == 'i' ||
                        lower == 'o' || lower == 'u');
        cout << "  -> Is vowel: " << (isVowel ? "YES" : "no") << endl;
        cout << "  -> ASCII code: " << static_cast<int>(value) << endl;
    }
};

// ============================================================
// EXCEPTION HANDLING
// We define custom exception types for domain-specific errors
// ============================================================

class AgeError : public runtime_error {
public:
    int age;
    AgeError(const string& msg, int a) : runtime_error(msg), age(a) {}
};

class DivisionError : public runtime_error {
public:
    DivisionError(const string& msg) : runtime_error(msg) {}
};

// Validates that an age makes logical sense
void validateAge(int age, const string& person) {
    if (age < 0) {
        throw AgeError("Age cannot be negative!", age);
    }
    if (age > 150) {
        throw AgeError("That age is unrealistic.", age);
    }
}

// Validates two people where one shouldn't be older than the other
void validateRelativeAge(int age1, const string& name1,
                          int age2, const string& name2,
                          bool youngerCanBeOlder) {
    if (!youngerCanBeOlder && age2 > age1) {
        throw AgeError(name2 + " cannot be older than " + name1 + "!", age2);
    }
}

// Safe division — throws if denominator is zero
double safeDivide(int a, int b) {
    if (b == 0) {
        throw DivisionError("Cannot divide by zero!");
    }
    return static_cast<double>(a) / b;
}

// ============================================================
// MAIN
// ============================================================

int main() {
    cout << "========================================" << endl;
    cout << " C++ Practice: Exceptions + Templates " << endl;
    cout << "========================================" << endl;
    cout << endl;

    // ----------------------------------------------------------
    // SECTION 1: Template Specialization in action
    // ----------------------------------------------------------
    cout << "[1] TEMPLATE SPECIALIZATION" << endl;
    cout << "    Container<T> works for all types," << endl;
    cout << "    but Container<char> has extra char-specific behavior!" << endl;
    cout << endl;

    Container<int>    c_int(42);
    Container<double> c_dbl(3.14159);
    Container<string> c_str(string("hello"));

    c_int.describe();
    c_dbl.describe();
    c_str.describe();

    cout << "  [char specializations:]" << endl;
    Container<char> c_a('a');   // a vowel
    Container<char> c_z('Z');   // a consonant
    Container<char> c_7('7');   // a digit

    c_a.describe();
    c_z.describe();
    c_7.describe();
    cout << endl;

    // ----------------------------------------------------------
    // SECTION 2: Exception handling with custom exceptions
    // ----------------------------------------------------------
    cout << "[2] EXCEPTION HANDLING" << endl;
    cout << "    Custom exception classes: AgeError, DivisionError" << endl;
    cout << endl;

    // --- Part A: Relative age validation ---
    int momAge = 52;
    int sonAge = 23;
    cout << "  Checking: mom(" << momAge << ") vs son(" << sonAge << ")..." << endl;
    try {
        validateAge(momAge, "Mom");
        validateAge(sonAge, "Son");
        validateRelativeAge(momAge, "Mom", sonAge, "Son", false);
        cout << "  -> Ages are valid!" << endl;
    } catch (const AgeError& e) {
        cout << "  -> CAUGHT: " << e.what() << endl;
    }
    cout << endl;

    // --- Part B: What if son is older than mom? (invalid!) ---
    int badSonAge = 67;
    cout << "  Checking: mom(" << momAge << ") vs son(" << badSonAge << ")..." << endl;
    try {
        validateAge(momAge, "Mom");
        validateAge(badSonAge, "Son");
        validateRelativeAge(momAge, "Mom", badSonAge, "Son", false);
        cout << "  -> Ages are valid!" << endl;
    } catch (const AgeError& e) {
        cout << "  -> CAUGHT AgeError: " << e.what()
             << " (caught age value: " << e.age << ")" << endl;
    }
    cout << endl;

    // --- Part C: Division by zero ---
    cout << "  Testing safeDivide(10, 2)..." << endl;
    try {
        double result = safeDivide(10, 2);
        cout << "  -> Result: " << result << endl;
    } catch (const DivisionError& e) {
        cout << "  -> CAUGHT: " << e.what() << endl;
    }

    cout << "  Testing safeDivide(10, 0)..." << endl;
    try {
        double result = safeDivide(10, 0);  // will throw!
        cout << "  -> Result: " << result << endl;
    } catch (const DivisionError& e) {
        cout << "  -> CAUGHT DivisionError: " << e.what() << endl;
    }
    cout << endl;

    // --- Part D: Negative age ---
    cout << "  Testing negative age (-5)..." << endl;
    try {
        validateAge(-5, "Person");
    } catch (const AgeError& e) {
        cout << "  -> CAUGHT AgeError: " << e.what() << endl;
    }
    cout << endl;

    // ----------------------------------------------------------
    // SECTION 3: Catching with catch(...) — the catch-all
    // ----------------------------------------------------------
    cout << "[3] CATCH-ALL with catch(...)" << endl;
    cout << "    catch(...) catches ANY exception type!" << endl;
    cout << endl;

    try {
        int x = 0;
        cout << "  Throwing a char 'X' to catch(...)..." << endl;
        throw 'X';  // throw a char
    } catch (...) {
        cout << "  -> catch(...) caught something (type unknown)!" << endl;
    }
    cout << endl;

    // ----------------------------------------------------------
    // Summary
    // ----------------------------------------------------------
    cout << "========================================" << endl;
    cout << " KEY TAKEAWAYS" << endl;
    cout << "========================================" << endl;
    cout << endl;
    cout << "  1. TEMPLATE SPECIALIZATION:" << endl;
    cout << "     template<> class Container<char> overrides" << endl;
    cout << "     the generic Container<T> for char only." << endl;
    cout << "     Compiler picks the right one automatically." << endl;
    cout << endl;
    cout << "  2. EXCEPTION HIERARCHY:" << endl;
    cout << "     AgeError and DivisionError inherit from" << endl;
    cout << "     std::runtime_error, giving them a what() string." << endl;
    cout << "     Custom fields (age) let you access error context." << endl;
    cout << endl;
    cout << "  3. try/catch/throw:" << endl;
    cout << "     throw sends a value to the nearest catch block." << endl;
    cout << "     catch(int x) catches only int throws." << endl;
    cout << "     catch(...) catches everything." << endl;
    cout << endl;
    cout << "  Program completed successfully!" << endl;

    return 0;
}
