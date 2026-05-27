// cpp-practice-2026-03-31.cpp
// C++ Practice: Bank Account with Classes, Pointers, and String Manipulation
// ===========================================================================

#include <iostream>
#include <string>
#include <vector>

// A simple BankAccount class demonstrating OOP concepts
class BankAccount {
private:
    std::string ownerName;   // String member variable
    double balance;          // Account balance
    int accountID;           // Unique account identifier

public:
    // Constructor - initializes a new account
    BankAccount(std::string name, double initialDeposit, int id) {
        ownerName = name;
        balance = initialDeposit;
        accountID = id;
        std::cout << "[Account Created] " << ownerName 
                  << " (ID: " << accountID << ") with $"
                  << balance << std::endl;
    }

    // Getter for balance
    double getBalance() const {
        return balance;
    }

    // Getter for owner name (demonstrates string return)
    std::string getOwnerName() const {
        return ownerName;
    }

    // Deposit money
    void deposit(double amount) {
        if (amount > 0) {
            balance += amount;
            std::cout << "[Deposit] Added $" << amount 
                      << " | New balance: $" << balance << std::endl;
        }
    }

    // Withdraw money
    bool withdraw(double amount) {
        if (amount > 0 && amount <= balance) {
            balance -= amount;
            std::cout << "[Withdrawal] Removed $" << amount 
                      << " | New balance: $" << balance << std::endl;
            return true;
        }
        std::cout << "[Withdrawal] FAILED - insufficient funds" << std::endl;
        return false;
    }

    // Display account info
    void display() const {
        std::cout << "---------------------------" << std::endl;
        std::cout << "Account ID:   " << accountID << std::endl;
        std::cout << "Owner:       " << ownerName << std::endl;
        std::cout << "Balance:     $" << balance << std::endl;
        std::cout << "---------------------------" << std::endl;
    }

    // Demonstrate string manipulation: uppercase the name
    std::string getUppercaseName() const {
        std::string upper = ownerName;
        for (size_t i = 0; i < upper.length(); i++) {
            upper[i] = std::toupper(upper[i]);
        }
        return upper;
    }

    // Destructor
    ~BankAccount() {
        std::cout << "[Account Closed] " << ownerName 
                  << " (ID: " << accountID << ")" << std::endl;
    }
};

// Function that uses a POINTER to an object (demonstrates pointer concepts)
void displayViaPointer(BankAccount* ptr) {
    if (ptr != nullptr) {
        std::cout << "[Via Pointer] " << ptr->getOwnerName() 
                  << " has balance: $" << ptr->getBalance() << std::endl;
    }
}

// Function that uses a REFERENCE to an object
void displayViaReference(BankAccount& ref) {
    std::cout << "[Via Reference] " << ref.getOwnerName() 
              << " has balance: $" << ref.getBalance() << std::endl;
}

// Main function
int main() {
    std::cout << "=== C++ Practice: Bank Account Demo ===" << std::endl;
    std::cout << std::endl;

    // Create a BankAccount object on the STACK
    BankAccount alice("Alice Smith", 1000.00, 1001);

    // Create another account on the HEAP using a pointer
    BankAccount* bob = new BankAccount("Bob Jones", 500.00, 1002);

    // Demonstrate string manipulation
    std::cout << "\n--- String Manipulation Demo ---" << std::endl;
    std::cout << "Original name: " << alice.getOwnerName() << std::endl;
    std::cout << "Uppercase:     " << alice.getUppercaseName() << std::endl;

    // Demonstrate string concatenation and length
    std::string greeting = "Hello, " + alice.getOwnerName() + "!";
    std::cout << "Greeting length: " << greeting.length() << " chars" << std::endl;
    std::cout << greeting << std::endl;

    // Use the object directly (stack)
    std::cout << "\n--- Direct Object Access ---" << std::endl;
    alice.display();

    // Use the object via POINTER (heap) - different syntax
    std::cout << "\n--- Pointer Access ---" << std::endl;
    displayViaPointer(bob);

    // Use the object via REFERENCE
    std::cout << "\n--- Reference Access ---" << std::endl;
    displayViaReference(alice);

    // Demonstrate pointer arrow syntax
    std::cout << "\n--- Pointer Arrow Syntax ---" << std::endl;
    std::cout << "Bob's balance via pointer: $" << bob->getBalance() << std::endl;

    // Perform some transactions
    std::cout << "\n--- Transactions ---" << std::endl;
    alice.deposit(250.00);
    alice.withdraw(100.00);
    alice.withdraw(2000.00);  // Should fail - not enough funds

    bob->deposit(1000.00);
    bob->withdraw(200.00);

    // Store pointers in a vector (demonstrates pointers with containers)
    std::cout << "\n--- Pointer Collection ---" << std::endl;
    std::vector<BankAccount*> accounts;
    accounts.push_back(&alice);    // Add address of stack object
    accounts.push_back(bob);        // Add heap pointer

    std::cout << "Total accounts: " << accounts.size() << std::endl;
    for (size_t i = 0; i < accounts.size(); i++) {
        std::cout << "  Account " << i << ": $"" 
                  << accounts[i]->getOwnerName() << "\" = $"
                  << accounts[i]->getBalance() << std::endl;
    }

    // Cleanup heap memory (important!)
    std::cout << "\n--- Cleanup ---" << std::endl;
    delete bob;  // Must explicitly delete heap-allocated objects

    std::cout << "\n=== Demo Complete ===" << std::endl;
    return 0;
}
