/*
 * C++ Practice: Composition and Member Initializers
 * Date: 2026-04-29
 * 
 * This program demonstrates:
 * 1. Composition: A class that "has-a" object of another class
 * 2. Member Initializer Lists: Initializing members before constructor body
 * 3. Const Members: Why const members REQUIRE initializer lists
 */

#include <iostream>
#include <string>
#include <cmath>  // for round() to handle floating point precision

using namespace std;

// ============================================================
// MONEY CLASS
// Represents an amount of money in dollars and cents
// This will be a "component" used by BankAccount (composition)
// ============================================================
class Money {
private:
    int dollars;   // Whole dollar amount
    int cents;     // Cent amount (0-99)

public:
    // Default constructor
    Money() : dollars(0), cents(0) {
        cout << "  [Money object created with $0.00]" << endl;
    }

    // Constructor with parameters - uses MEMBER INITIALIZER LIST
    // The syntax ": dollars(d), cents(c)" initializes members BEFORE body runs
    Money(int d, int c) : dollars(d), cents(c) {
        cout << "  [Money object created with $" << dollars << "." 
             << (cents < 10 ? "0" : "") << cents << "]" << endl;
        
        // Normalize: if cents goes over 99, carry to dollars
        if (cents < 0) {
            dollars += (cents / 100) - 1;
            cents = 100 + (cents % 100);
        } else if (cents >= 100) {
            dollars += cents / 100;
            cents = cents % 100;
        }
    }

    // Copy constructor - creates a copy of a Money object
    Money(const Money& other) : dollars(other.dollars), cents(other.cents) {
        cout << "  [Money object COPIED]" << endl;
    }

    // Getters
    int getDollars() const { return dollars; }
    int getCents() const { return cents; }

    // Display the money amount
    void display() const {
        cout << "$" << dollars << "." << (cents < 10 ? "0" : "") << cents;
    }
};

// ============================================================
// BANK ACCOUNT CLASS  
// Demonstrates COMPOSITION - a BankAccount "has-a" Money balance
// Also demonstrates MEMBER INITIALIZER LISTS
// ============================================================
class BankAccount {
private:
    string accountHolder;  // Name of the account owner
    string accountNumber; // Account identifier
    
    // COMPOSITION: balance is a Money object that belongs to this account
    // When BankAccount is created, Money is created first
    // When BankAccount is destroyed, Money is destroyed automatically
    Money balance;
    
    // CONST MEMBER: This CANNOT be changed after initialization
    // Therefore it MUST use a member initializer list in the constructor
    const double interestRate;  

public:
    // PRIMARY CONSTRUCTOR with MEMBER INITIALIZER LIST
    // Syntax: ClassName() : member1(val1), member2(val2), constMember(val3)
    // This initializes members BEFORE the constructor body executes
    BankAccount(string holder, string acctNum, int initialDollars, int initialCents)
        : accountHolder(holder)
        , accountNumber(acctNum)
        , balance(initialDollars, initialCents)  // Composition: initialize Money object
        , interestRate(0.025)  // 2.5% interest rate - MUST use initializer for const
    {
        cout << "  [BankAccount for " << accountHolder << " opened]" << endl;
    }

    // Overloaded constructor with just holder and acct num (balance = $0)
    BankAccount(string holder, string acctNum)
        : accountHolder(holder)
        , accountNumber(acctNum)
        , balance()  // Default construct Money with $0.00
        , interestRate(0.025)
    {
        cout << "  [BankAccount for " << accountHolder << " opened with $0.00]" << endl;
    }

    // Copy constructor - notice how it uses initializer list to copy Money
    BankAccount(const BankAccount& other)
        : accountHolder(other.accountHolder)
        , accountNumber(other.accountNumber)
        , balance(other.balance)  // Copies the Money object (composition)
        , interestRate(other.interestRate)
    {
        cout << "  [BankAccount COPIED for " << accountHolder << "]" << endl;
    }

    // Deposit money INTO the account
    void deposit(int dollars, int cents) {
        Money depositMoney(dollars, cents);
        
        // Simple addition (could be more complex with borrowing)
        balance = Money(
            balance.getDollars() + depositMoney.getDollars(),
            balance.getCents() + depositMoney.getCents()
        );
        
        cout << "[Deposit completed: $";
        depositMoney.display();
        cout << " | New balance: ";
        balance.display();
        cout << "]" << endl;
    }

    // Withdraw money FROM the account
    void withdraw(int dollars, int cents) {
        Money withdrawMoney(dollars, cents);
        
        // Check if sufficient funds
        int totalCents = balance.getDollars() * 100 + balance.getCents();
        int withdrawCents = withdrawMoney.getDollars() * 100 + withdrawMoney.getCents();
        
        if (withdrawCents > totalCents) {
            cout << "[ERROR: Insufficient funds!]" << endl;
            return;
        }
        
        // Simple subtraction
        balance = Money(
            balance.getDollars() - withdrawMoney.getDollars(),
            balance.getCents() - withdrawMoney.getCents()
        );
        
        cout << "[Withdrawal completed: $";
        withdrawMoney.display();
        cout << " | New balance: ";
        balance.display();
        cout << "]" << endl;
    }

    // Calculate and add interest to the account
    void addInterest() {
        // Calculate interest on current balance
        double totalBalance = balance.getDollars() + (balance.getCents() / 100.0);
        double interest = totalBalance * interestRate;
        
        // Convert interest back to dollars and cents
        int interestDollars = static_cast<int>(interest);
        int interestCents = static_cast<int>((interest - interestDollars) * 100);
        interestCents = static_cast<int>(round(interestCents));  // Handle rounding
        
        if (interestCents >= 100) {
            interestDollars += interestCents / 100;
            interestCents = interestCents % 100;
        }
        
        if (interestDollars > 0 || interestCents > 0) {
            deposit(interestDollars, interestCents);
            cout << "[Interest added at " << (interestRate * 100) << "%]" << endl;
        }
    }

    // Display account information
    void displayAccount() const {
        cout << "\n========================================" << endl;
        cout << "Account Holder: " << accountHolder << endl;
        cout << "Account Number: " << accountNumber << endl;
        cout << "Current Balance: ";
        balance.display();
        cout << endl;
        cout << "Interest Rate: " << (interestRate * 100) << "%" << endl;
        cout << "========================================" << endl;
    }

    // Destructor - important for understanding object lifetime
    ~BankAccount() {
        cout << "[BankAccount for " << accountHolder << " CLOSED]" << endl;
        // Note: balance Money object will be destroyed automatically (composition)
    }
};

// ============================================================
// MAIN FUNCTION - Demonstrates all concepts
// ============================================================
int main() {
    cout << "=== C++ PRACTICE: Composition & Member Initializers ===" << endl;
    cout << endl;

    // ---------------------------------------------------------
    // DEMO 1: Creating a Bank Account with initial balance
    // ---------------------------------------------------------
    cout << "\n--- Creating account with initial deposit ---" << endl;
    BankAccount checking("Alice Johnson", "CHK-1234", 500, 75);
    
    checking.displayAccount();
    
    // ---------------------------------------------------------
    // DEMO 2: Deposit money
    // ---------------------------------------------------------
    cout << "\n--- Making a deposit ---" << endl;
    checking.deposit(200, 50);
    checking.displayAccount();
    
    // ---------------------------------------------------------
    // DEMO 3: Withdraw money
    // ---------------------------------------------------------
    cout << "\n--- Making a withdrawal ---" << endl;
    checking.withdraw(100, 25);
    checking.displayAccount();
    
    // ---------------------------------------------------------
    // DEMO 4: Add interest
    // ---------------------------------------------------------
    cout << "\n--- Adding interest ---" << endl;
    checking.addInterest();
    checking.displayAccount();
    
    // ---------------------------------------------------------
    // DEMO 5: Creating account with $0 balance
    // ---------------------------------------------------------
    cout << "\n--- Creating second account (no initial deposit) ---" << endl;
    BankAccount savings("Bob Smith", "SAV-5678");
    
    savings.deposit(1000, 00);
    savings.addInterest();
    savings.displayAccount();
    
    // ---------------------------------------------------------
    // DEMO 6: Copying an account (demonstrates composition copy)
    // ---------------------------------------------------------
    cout << "\n--- Copying an account (composition demo) ---" << endl;
    BankAccount aliceCopy(checking);  // Copy constructor called
    aliceCopy.displayAccount();
    
    // ---------------------------------------------------------
    // DEMO 7: Demonstrating insufficient funds
    // ---------------------------------------------------------
    cout << "\n--- Testing insufficient funds ---" << endl;
    savings.withdraw(5000, 00);  // More than balance
    
    cout << "\n=== END OF PROGRAM ===" << endl;
    
    return 0;
}

/*
 * KEY CONCEPTS DEMONSTRATED:
 * 
 * 1. MEMBER INITIALIZER LIST
 *    BankAccount(string holder, string acctNum, int d, int c)
 *        : accountHolder(holder)      // Initialize string
 *        , accountNumber(acctNum)      // Initialize string  
 *        , balance(d, c)               // Initialize Money object (composition)
 *        , interestRate(0.025)         // Initialize const member (REQUIRED!)
 *    { }
 * 
 * 2. COMPOSITION
 *    - BankAccount "has-a" Money balance
 *    - Money object is a member variable of BankAccount
 *    - Lifetime is managed automatically: created with account, destroyed with account
 *    - This is "has-a" relationship (composition)
 *    - vs "is-a" relationship (inheritance)
 * 
 * 3. CONST MEMBERS
 *    - interestRate is const - cannot be changed after creation
 *    - MUST use member initializer list to set it
 *    - Cannot use assignment in constructor body
 * 
 * 4. OBJECT LIFETIME (Composition)
 *    - When BankAccount is created, members are initialized first
 *    - Money balance is created as part of BankAccount construction
 *    - When BankAccount is destroyed, Money is automatically destroyed
 *    - No memory management needed - automatic cleanup
 */
