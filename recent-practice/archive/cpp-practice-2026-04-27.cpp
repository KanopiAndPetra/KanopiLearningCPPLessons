// =============================================================================
// C++ Practice: Animal Shelter Management System
// Date: April 27, 2026
// Concepts: Classes, Member Variables/Functions, Constructors, Inheritance
// =============================================================================

#include <iostream>
#include <string>
#include <vector>

using namespace std;

// =============================================================================
// BASE CLASS: Animal
// A class is like a template/form for creating objects.
// Objects are instances of a class — think of them as boxes you fill with data.
// =============================================================================
class Animal {
public:
    // -------------------------------------------------------------------------
    // MEMBER VARIABLES (attributes/data each Animal object holds)
    // -------------------------------------------------------------------------
    string name;
    int age;
    string breed;

    // -------------------------------------------------------------------------
    // CONSTRUCTOR — Called automatically when an object is created
    // Initializes the object with starting values
    // -------------------------------------------------------------------------
    Animal(string n = "Unknown", int a = 0, string b = "Unknown") {
        name = n;
        age = a;
        breed = b;
        cout << "  [Animal constructor called for: " << name << "]" << endl;
    }

    // -------------------------------------------------------------------------
    // MEMBER FUNCTION — An action/behavior the Animal can perform
    // Uses the dot (.) operator to call from an object
    // -------------------------------------------------------------------------
    void speak() {
        cout << name << " makes a sound." << endl;
    }

    // -------------------------------------------------------------------------
    // GETTER FUNCTION — Returns the age when called
    // Encapsulates access to private-ish data
    // -------------------------------------------------------------------------
    int getAge() {
        return age;
    }

    // -------------------------------------------------------------------------
    // DISPLAY FUNCTION — Shows all animal info (version 1)
    // This will be OVERLOADED below with a different parameter signature
    // -------------------------------------------------------------------------
    void displayInfo() {
        cout << "  Name: " << name << endl;
        cout << "  Age: " << age << " years" << endl;
        cout << "  Breed: " << breed << endl;
    }
};

// =============================================================================
// DERIVED CLASS: Dog (inherits from Animal)
// Inheritance means Dog automatically gets all public members from Animal
// Think: Dog "is a" Animal with extra Dog-specific features
// =============================================================================
class Dog : public Animal {
public:
    // Additional member variable specific to Dogs
    bool isTrained;

    // -------------------------------------------------------------------------
    // CONSTRUCTOR — Uses member initializer list to call parent constructor
    // then initializes Dog-specific members
    // -------------------------------------------------------------------------
    Dog(string n, int a, string b, bool trained)
        : Animal(n, a, b)  // Call parent constructor with these values
    {
        isTrained = trained;
        cout << "  [Dog constructor called]" << endl;
    }

    // -------------------------------------------------------------------------
    // OVERRIDDEN speak() — Dog-specific version replaces parent's version
    // Virtual functions allow this polymorphic behavior
    // -------------------------------------------------------------------------
    void speak() {
        cout << name << " says: WOOF WOOF!" << endl;
    }

    // -------------------------------------------------------------------------
    // DOG-SPECIFIC FUNCTION — Only Dogs can do this
    // -------------------------------------------------------------------------
    void fetch() {
        cout << name << " fetches the ball!" << endl;
    }
};

// =============================================================================
// DERIVED CLASS: Cat (inherits from Animal)
// Cat gets everything from Animal plus Cat-specific behaviors
// =============================================================================
class Cat : public Animal {
public:
    // Cat-specific member variable
    bool isIndoorCat;

    // -------------------------------------------------------------------------
    // CONSTRUCTOR — Calls parent constructor, then sets Cat-specific data
    // -------------------------------------------------------------------------
    Cat(string n, int a, string b, bool indoor)
        : Animal(n, a, b)
    {
        isIndoorCat = indoor;
        cout << "  [Cat constructor called]" << endl;
    }

    // -------------------------------------------------------------------------
    // OVERRIDDEN speak() — Cat-specific version
    // -------------------------------------------------------------------------
    void speak() {
        cout << name << " says: MEOW!" << endl;
    }

    // -------------------------------------------------------------------------
    // CAT-SPECIFIC FUNCTION — Only Cats can do this
    // -------------------------------------------------------------------------
    void knockThingsOff() {
        cout << name << " knocks everything off the table!" << endl;
    }
};

// =============================================================================
// FUNCTION OVERLOADING DEMO
// Same function name "displayInfo" but different parameters
// The compiler decides which one to use based on what's passed
// =============================================================================

void displayInfo(Animal a) {
    // Version that takes a single Animal object
    cout << "\n  === ANIMAL INFO ===" << endl;
    a.displayInfo();
}

void displayInfo(Dog d) {
    // Version specifically for Dogs — shows extra trained status
    cout << "\n  === DOG INFO ===" << endl;
    d.displayInfo();
    cout << "  Trained: " << (d.isTrained ? "Yes" : "No") << endl;
}

void displayInfo(Cat c) {
    // Version specifically for Cats — shows indoor status
    cout << "\n  === CAT INFO ===" << endl;
    c.displayInfo();
    cout << "  Indoor: " << (c.isIndoorCat ? "Yes" : "No") << endl;
}

// =============================================================================
// MAIN FUNCTION — Program entry point
// Creates objects, calls functions, demonstrates concepts learned
// =============================================================================
int main() {

    cout << "==================================================" << endl;
    cout << "   ANIMAL SHELTER MANAGEMENT SYSTEM" << endl;
    cout << "   C++ Practice: Classes, Inheritance, Constructors" << endl;
    cout << "==================================================" << endl;

    // -------------------------------------------------------------------------
    // CREATING OBJECTS
    // objectName.ClassName() — calls constructor automatically
    // -------------------------------------------------------------------------
    cout << "\n--- Creating Animals ---\n" << endl;

    cout << "Creating a Dog object named 'Buddy':" << endl;
    Dog buddy("Buddy", 3, "Golden Retriever", true);

    cout << "\nCreating a Cat object named 'Whiskers':" << endl;
    Cat whiskers("Whiskers", 5, "Tabby", true);

    cout << "\n--- Testing Member Functions ---\n" << endl;

    // -------------------------------------------------------------------------
    // CALLING MEMBER FUNCTIONS
    // Use dot (.) operator: objectName.functionName()
    // -------------------------------------------------------------------------
    cout << "Buddy speaks: ";
    buddy.speak();

    cout << "Whiskers speaks: ";
    whiskers.speak();

    cout << "\nBuddy's age via getter: " << buddy.getAge() << " years" << endl;

    // -------------------------------------------------------------------------
    // USING CLASS-SPECIFIC FUNCTIONS
    // -------------------------------------------------------------------------
    cout << "\n--- Dog and Cat Specific Functions ---\n" << endl;
    buddy.fetch();
    whiskers.knockThingsOff();

    // -------------------------------------------------------------------------
    // FUNCTION OVERLOADING IN ACTION
    // The correct displayInfo() is called based on the object type passed
    // -------------------------------------------------------------------------
    cout << "\n--- Function Overloading Demo ---\n" << endl;
    displayInfo(buddy);      // Calls Dog version
    displayInfo(whiskers);    // Calls Cat version

    // -------------------------------------------------------------------------
    // USING A VECTOR TO STORE MULTIPLE ANIMALS (polymorphism demo)
    // Vector can hold different derived types via base class pointer/reference
    // -------------------------------------------------------------------------
    cout << "\n--- Shelter Animal List ---\n" << endl;

    // Create a vector to hold Animal objects (or derived types)
    vector<Animal*> shelter;

    // Add pointers to our animals (using pointers shows another use of classes)
    shelter.push_back(&buddy);
    shelter.push_back(&whiskers);

    cout << "Animals in shelter: " << shelter.size() << endl;

    // -------------------------------------------------------------------------
    // ACCESSING MEMBER VARIABLES DIRECTLY
    // Using dot operator to access public members
    // -------------------------------------------------------------------------
    cout << "\n--- Direct Member Variable Access ---\n" << endl;
    cout << "Buddy's name: " << buddy.name << endl;
    cout << "Buddy's breed: " << buddy.breed << endl;
    cout << "Whiskers' name: " << whiskers.name << endl;
    cout << "Whiskers' breed: " << whiskers.breed << endl;

    cout << "\n==================================================" << endl;
    cout << "   Program completed successfully!" << endl;
    cout << "==================================================" << endl;

    return 0;
}

// =============================================================================
// KEY CONCEPTS DEMONSTRATED:
//
// 1. CLASS — A template for creating objects (Animal, Dog, Cat)
// 2. OBJECT — An instance of a class (buddy, whiskers)
// 3. MEMBER VARIABLES — Data stored in each object (name, age, breed)
// 4. MEMBER FUNCTIONS — Actions objects can perform (speak(), getAge())
// 5. CONSTRUCTOR — Initializes objects when created
// 6. INHERITANCE — Derived classes (Dog, Cat) get parent's members
// 7. FUNCTION OVERLOADING — Same name, different parameters
// 8. PUBLIC/PRIVATE — Access control for class members
// =============================================================================