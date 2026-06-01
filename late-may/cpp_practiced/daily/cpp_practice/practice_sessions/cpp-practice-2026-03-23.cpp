// cpp-practice-2026-03-23.cpp
// C++ Practice: Classes, Pointers, and String Manipulation
//
// This program demonstrates:
//   - Defining a class with member variables and functions
//   - Using pointers to manage object memory
//   - String manipulation with std::string
//   - Dynamic memory allocation and cleanup

#include <iostream>
#include <string>
#include <vector>

// ============================================================
// PART 1: A simple Contact class
// ============================================================
class Contact {
private:
    std::string name;      // Contact's name
    std::string phone;     // Contact's phone number
    std::string email;     // Contact's email address

public:
    // Constructor - initializes a new Contact
    Contact(const std::string& n, const std::string& p, const std::string& e)
        : name(n), phone(p), email(e) {}

    // Getters - return contact information
    std::string getName()  const { return name; }
    std::string getPhone() const { return phone; }
    std::string getEmail() const { return email; }

    // Setters - modify contact information
    void setPhone(const std::string& p) { phone = p; }
    void setEmail(const std::string& e) { email = e; }

    // display() - prints contact details in a formatted way
    void display() const {
        std::cout << "  Name:  " << name << "\n";
        std::cout << "  Phone: " << phone << "\n";
        std::cout << "  Email: " << email << "\n";
    }

    // hasName() - string manipulation example: checks if name contains substring
    bool hasName(const std::string& substring) const {
        // Convert both to lowercase for case-insensitive comparison
        std::string lowerName = name;
        std::string lowerSub = substring;
        
        for (char& c : lowerName) c = std::tolower(c);
        for (char& c : lowerSub)  c = std::tolower(c);
        
        return lowerName.find(lowerSub) != std::string::npos;
    }
};

// ============================================================
// PART 2: String manipulation helper functions
// ============================================================

// trim() - removes leading and trailing whitespace
std::string trim(const std::string& str) {
    size_t start = str.find_first_not_of(" \t\n");
    if (start == std::string::npos) return "";
    size_t end = str.find_last_not_of(" \t\n");
    return str.substr(start, end - start + 1);
}

// formatPhone() - ensures phone number is in (XXX) XXX-XXXX format
std::string formatPhone(const std::string& raw) {
    std::string digits;
    for (char c : raw) {
        if (std::isdigit(c)) digits += c;
    }
    if (digits.length() == 10) {
        return "(" + digits.substr(0, 3) + ") " + digits.substr(3, 3) + "-" + digits.substr(6, 4);
    }
    return raw; // return as-is if not 10 digits
}

// ============================================================
// PART 3: Pointer demonstrations
// ============================================================

// printViaPointer() - accepts a pointer to a Contact and prints it
void printViaPointer(Contact* ptr) {
    if (ptr == nullptr) {
        std::cout << "  [null pointer]\n";
        return;
    }
    std::cout << "  [via pointer] ";
    std::cout << ptr->getName() << " - " << ptr->getPhone() << "\n";
}

// ============================================================
// PART 4: Main - puts it all together
// ============================================================
int main() {
    std::cout << "=== C++ Practice: Contacts with Classes, Strings & Pointers ===\n\n";

    // --- Create contacts on the heap using pointers ---
    std::cout << "Creating contacts with 'new' (heap allocation):\n";
    
    Contact* contact1 = new Contact("Alice Smith", "555-123-4567", "alice@example.com");
    Contact* contact2 = new Contact("Bob Jones", "5559876543", "bob.jones@work.org"); // unformatted phone
    Contact* contact3 = new Contact("Carol Williams", "312-555-0000", "carol@home.net");

    // Store pointers in a vector for easy cleanup
    std::vector<Contact*> allContacts = {contact1, contact2, contact3};

    // --- Display all contacts ---
    for (size_t i = 0; i < allContacts.size(); ++i) {
        std::cout << "\nContact " << (i + 1):\n";
        allContacts[i]->display();
    }

    // --- String manipulation ---
    std::cout << "\n--- String Manipulation Examples ---\n";
    
    std::string messy = "   hello world!   ";
    std::cout << "trim(\"" << messy << "\") = \"" << trim(messy) << "\"\n";

    std::string rawPhone = "555.123.4567";
    std::cout << "formatPhone(\"" << rawPhone << "\") = \"" << formatPhone(rawPhone) << "\"\n";

    // --- Using pointers to functions ---
    std::cout << "\n--- Passing Pointers to Functions ---\n";
    printViaPointer(contact1);
    printViaPointer(contact2);
    printViaPointer(nullptr);  // demonstrates null pointer handling

    // --- Searching contacts by name (string find) ---
    std::cout << "\n--- Searching Contacts ---\n";
    std::string searchTerm = "alice";
    std::cout << "Searching for \"" << searchTerm << "\":\n";
    for (Contact* c : allContacts) {
        if (c->hasName(searchTerm)) {
            std::cout << "  Found: " << c->getName() << "\n";
        }
    }

    // --- Modify via pointer ---
    std::cout << "\n--- Modifying Contact via Pointer ---\n";
    std::cout << "Bob's old phone: " << contact2->getPhone() << "\n";
    contact2->setPhone(formatPhone("312-555-9999"));
    std::cout << "Bob's new phone: " << contact2->getPhone() << "\n";

    // --- Cleanup: delete heap-allocated objects ---
    std::cout << "\n--- Cleaning Up Heap Memory ---\n";
    for (Contact* c : allContacts) {
        std::cout << "  Deleting: " << c->getName() << "\n";
        delete c;
    }
    allContacts.clear();

    std::cout << "\n=== Done! ===\n";
    return 0;
}
