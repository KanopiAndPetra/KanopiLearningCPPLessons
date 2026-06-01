// cpp-practice-2026-03-28.cpp
// C++ Practice: Classes, Pointers, and String Manipulation
// ============================================================
// This program demonstrates:
//   - Defining and using a class with member variables and functions
//   - Creating and using pointers to objects
//   - String manipulation (std::string methods)

#include <iostream>
#include <string>

// ============================================================
// CLASS: BankAccount
// A simple class representing a bank account with owner name,
// balance, and account number.
// ============================================================
class BankAccount {
private:
    std::string ownerName;   // Account owner's name
    double balance;          // Current balance in dollars
    int accountNumber;        // Unique account identifier

public:
    // Constructor - initializes a new bank account
    BankAccount(std::string name, double initialBalance, int accNum) {
        ownerName = name;
        balance = initialBalance;
        accountNumber = accNum;
    }

    // Get the owner's name
    std::string getOwner() {
        return ownerName;
    }

    // Get current balance
    double getBalance() {
        return balance;
    }

    // Get account number
    int getAccountNumber() {
        return accountNumber;
    }

    // Deposit money - adds to balance
    void deposit(double amount) {
        if (amount > 0) {
            balance += amount;
            std::cout << "  Deposited $" << amount << " successfully.\n";
        } else {
            std::cout << "  Deposit amount must be positive.\n";
        }
    }

    // Withdraw money - subtracts from balance
    void withdraw(double amount) {
        if (amount > 0 && amount <= balance) {
            balance -= amount;
            std::cout << "  Withdrew $" << amount << " successfully.\n";
        } else if (amount > balance) {
            std::cout << "  Insufficient funds for this withdrawal.\n";
        } else {
            std::cout << "  Withdrawal amount must be positive.\n";
        }
    }

    // Display account info
    void displayInfo() {
        std::cout << "  --- Account #" << accountNumber << " ---\n";
        std::cout << "  Owner: " << ownerName << "\n";
        std::cout << "  Balance: $" << balance << "\n";
    }
};

// ============================================================
// Helper function demonstrating string manipulation
// ============================================================
void demonstrateStringManipulation(const std::string& name) {
    std::cout << "\n=== String Manipulation Demo ===\n";
    
    // Convert to uppercase
    std::string upperName = name;
    for (char& c : upperName) {
        c = std::toupper(c);
    }
    std::cout << "  Uppercase: " << upperName << "\n";
    
    // Get substring (first 5 characters)
    std::string firstPart = name.substr(0, 5);
    std::cout << "  First 5 chars: " << firstPart << "\n";
    
    // Find character
    size_t spacePos = name.find(' ');
    if (spacePos != std::string::npos) {
        std::string lastName = name.substr(spacePos + 1);
        std::cout << "  Last name extracted: " << lastName << "\n";
    }
    
    // String length
    std::cout << "  Total length: " << name.length() << " characters\n";
}

// ============================================================
// Main function
// ============================================================
int main() {
    std::cout << "========================================\n";
    std::cout << "  C++ Practice: Classes, Pointers &\n";
    std::cout << "  String Manipulation\n";
    std::cout << "  Date: March 28, 2026\n";
    std::cout << "========================================\n";

    // ------------------------------------------------------
    // PART 1: Creating objects and using member functions
    // ------------------------------------------------------
    std::cout << "\n=== Creating BankAccount Objects ===\n";
    
    BankAccount account1("Alice Smith", 1000.00, 1001);
    BankAccount account2("Bob Jones", 2500.50, 1002);
    
    account1.displayInfo();
    account2.displayInfo();

    // ------------------------------------------------------
    // PART 2: Using pointers to objects
    // ------------------------------------------------------
    std::cout << "\n=== Using Pointers to Objects ===\n";
    
    // Create a pointer to an existing object
    BankAccount* ptrToAccount1 = &account1;
    
    // Access members via the pointer using the arrow operator (->)
    std::cout << "  Accessing via pointer:\n";
    std::cout << "  Owner (via pointer): " << ptrToAccount1->getOwner() << "\n";
    std::cout << "  Balance (via pointer): $" << ptrToAccount1->getBalance() << "\n";
    
    // Create a new BankAccount on the heap using 'new'
    // NOTE: Remember to 'delete' this to avoid memory leaks!
    BankAccount* heapAccount = new BankAccount("Charlie Brown", 500.00, 1003);
    std::cout << "\n  Created account on HEAP:\n";
    std::cout << "  Owner: " << heapAccount->getOwner() << "\n";
    std::cout << "  Initial Balance: $" << heapAccount->getBalance() << "\n";

    // ------------------------------------------------------
    // PART 3: Using object methods (deposit/withdraw)
    // ------------------------------------------------------
    std::cout << "\n=== Transactions ===\n";
    
    std::cout << "  Account #" << account1.getAccountNumber() << " transactions:\n";
    account1.deposit(250.00);
    account1.withdraw(100.00);
    account1.displayInfo();
    
    // Try invalid withdrawal
    std::cout << "\n  Attempting overdraft:\n";
    account1.withdraw(5000.00);
    
    // ------------------------------------------------------
    // PART 4: String manipulation
    // ------------------------------------------------------
    demonstrateStringManipulation(account2.getOwner());

    // ------------------------------------------------------
    // PART 5: Cleanup (free heap-allocated memory)
    // ------------------------------------------------------
    std::cout << "\n=== Cleanup ===\n";
    delete heapAccount;  // Free the memory we allocated with 'new'
    std::cout << "  Deleted heapAccount. Memory freed.\n";

    std::cout << "\n========================================\n";
    std::cout << "  Program completed successfully!\n";
    std::cout << "========================================\n";

    return 0;
}
