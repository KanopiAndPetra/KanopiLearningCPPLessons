/*
 * cpp-practice-2026-05-23.cpp
 * 
 * Topic: Classes + Object-Oriented Programming (OOP)
 * 
 * Repos studied:
 *   - Oppie1/1.1Oppie1CPP - Classes lesson (TBD)
 *   - Oppie1/1.1Oppie1CPP - Constructors lesson (TBD)
 * 
 * Concepts:
 *   - Class = blueprint for creating objects
 *   - Object = instance of a class
 *   - Public vs Private members
 *   - Constructor: special function called when object is created
 *   - Member functions (methods): functions inside a class
 *   - Getters/setters: access private data safely
 * 
 * What this program does:
 *   - Demonstrates a BankAccount class with deposit, withdraw, transfer
 *   - Shows multiple constructors (overloaded)
 *   - Demonstrates encapsulation (private data accessed via methods)
 *   - Shows how objects interact in a simple banking simulation
 */

#include <iostream>
#include <string>
#include <vector>
#include <cmath>
using namespace std;

// =============================================================================
// BANK ACCOUNT CLASS
// =============================================================================
// A class bundles DATA (account holder, balance) with FUNCTIONS (deposit,
// withdraw, display). The data is marked private so external code can't
// modify it directly — they MUST use the member functions.

class BankAccount {
private:
    // Private data — can't be accessed directly from outside the class
    string accountHolder;
    int accountNumber;
    double balance;
    static int nextAccountNumber;  // shared across all accounts

public:
    // -------------------------------------------------------------------------
    // CONSTRUCTORS
    // -------------------------------------------------------------------------
    // A constructor runs automatically when you create an object.
    // Multiple constructors = overloading! The compiler picks the right one.

    // Constructor 1: Full initialization
    BankAccount(string holder, double initialBalance) {
        accountHolder = holder;
        accountNumber = nextAccountNumber++;
        balance = initialBalance;
        cout << "  [ACCOUNT CREATED] #" << accountNumber 
             << " for " << accountHolder 
             << " with balance $" << balance << endl;
    }

    // Constructor 2: Default constructor (no arguments) — balance starts at 0
    BankAccount() {
        accountHolder = "Unnamed";
        accountNumber = nextAccountNumber++;
        balance = 0.0;
        cout << "  [ACCOUNT CREATED] #" << accountNumber 
             << " (default, balance $0)" << endl;
    }

    // Constructor 3: Just holder name, balance defaults to 0
    BankAccount(string holder) {
        accountHolder = holder;
        accountNumber = nextAccountNumber++;
        balance = 0.0;
        cout << "  [ACCOUNT CREATED] #" << accountNumber 
             << " for " << accountHolder << " (balance $0)" << endl;
    }

    // -------------------------------------------------------------------------
    // DESTRUCTOR
    // -------------------------------------------------------------------------
    // Runs when object is destroyed (goes out of scope, delete called, etc.)
    ~BankAccount() {
        cout << "  [ACCOUNT CLOSED] #" << accountNumber 
             << " (" << accountHolder << ")" << endl;
    }

    // -------------------------------------------------------------------------
    // MEMBER FUNCTIONS (Methods)
    // -------------------------------------------------------------------------

    // Deposit money — adds to balance
    void deposit(double amount) {
        if (amount > 0) {
            balance += amount;
            cout << "  Deposited $" << amount << " -> New balance: $" << balance << endl;
        } else {
            cout << "  ERROR: Deposit amount must be positive!" << endl;
        }
    }

    // Withdraw money — subtracts from balance (if sufficient funds)
    void withdraw(double amount) {
        if (amount <= 0) {
            cout << "  ERROR: Withdrawal amount must be positive!" << endl;
        } else if (amount > balance) {
            cout << "  ERROR: Insufficient funds! Balance: $" << balance << endl;
        } else {
            balance -= amount;
            cout << "  Withdrew $" << amount << " -> New balance: $" << balance << endl;
        }
    }

    // Transfer money to another account
    void transfer(double amount, BankAccount &recipient) {
        if (amount <= 0) {
            cout << "  ERROR: Transfer amount must be positive!" << endl;
        } else if (amount > balance) {
            cout << "  ERROR: Insufficient funds for transfer! Balance: $" << balance << endl;
        } else {
            balance -= amount;
            recipient.balance += amount;
            cout << "  Transferred $" << amount << " from #" << accountNumber
                 << " to #" << recipient.accountNumber << endl;
            cout << "  Your new balance: $" << balance << endl;
        }
    }

    // Get current balance (getter)
    double getBalance() const {
        return balance;
    }

    // Get account number (getter)
    int getAccountNumber() const {
        return accountNumber;
    }

    // Get holder name (getter)
    string getHolderName() const {
        return accountHolder;
    }

    // Display account info
    void display() const {
        cout << "  Account #" << accountNumber << " | " << accountHolder
             << " | Balance: $" << balance << endl;
    }
};

// Initialize static member (shared across all BankAccount objects)
int BankAccount::nextAccountNumber = 1000;

// =============================================================================
// STUDENT CLASS - Simple grade tracker
// =============================================================================
class Student {
private:
    string name;
    vector<double> grades;

public:
    // Constructor
    Student(string studentName) {
        name = studentName;
        cout << "  [STUDENT CREATED] " << name << endl;
    }

    // Add a grade
    void addGrade(double grade) {
        if (grade >= 0 && grade <= 100) {
            grades.push_back(grade);
            cout << "  Added grade: " << grade << "% for " << name << endl;
        } else {
            cout << "  ERROR: Grade must be 0-100!" << endl;
        }
    }

    // Calculate average grade
    double getAverage() const {
        if (grades.empty()) {
            return 0.0;
        }
        double sum = 0;
        for (double g : grades) {
            sum += g;
        }
        return sum / grades.size();
    }

    // Get letter grade
    char getLetterGrade() const {
        double avg = getAverage();
        if (avg >= 90) return 'A';
        else if (avg >= 80) return 'B';
        else if (avg >= 70) return 'C';
        else if (avg >= 60) return 'D';
        else return 'F';
    }

    // Display student summary
    void display() const {
        cout << "  Student: " << name << " | Grades: " << grades.size()
             << " | Avg: " << getAverage() << "% | Letter: " << getLetterGrade() << endl;
    }
};

// =============================================================================
// MAIN PROGRAM
// =============================================================================
int main() {

    cout << "======================================================" << endl;
    cout << "  C++ PRACTICE: Classes + Object-Oriented Programming" << endl;
    cout << "======================================================" << endl;

    // -------------------------------------------------------------------------
    // DEMO 1: BankAccount — creating objects and using constructors
    // -------------------------------------------------------------------------
    cout << "\n[DEMO 1] Creating BankAccount Objects" << endl;
    cout << "------------------------------------------------" << endl;

    // Three different ways to create objects (uses different constructors)
    BankAccount acct1("Alice Johnson", 1000.00);   // Full constructor
    BankAccount acct2("Bob Smith");                 // Name-only constructor
    BankAccount acct3;                             // Default constructor

    // -------------------------------------------------------------------------
    // DEMO 2: Using member functions (deposit, withdraw)
    // -------------------------------------------------------------------------
    cout << "\n[DEMO 2] deposit() and withdraw() methods" << endl;
    cout << "------------------------------------------------" << endl;

    cout << "\n-- Alice's account --" << endl;
    acct1.display();
    acct1.deposit(500.00);
    acct1.withdraw(200.00);
    acct1.withdraw(2000.00);  // Should fail — insufficient funds
    acct1.display();

    cout << "\n-- Bob's account --" << endl;
    acct2.display();
    acct2.deposit(250.00);
    acct2.withdraw(50.00);
    acct2.display();

    // -------------------------------------------------------------------------
    // DEMO 3: Transfer between accounts
    // -------------------------------------------------------------------------
    cout << "\n[DEMO 3] transfer() between accounts" << endl;
    cout << "------------------------------------------------" << endl;

    cout << "Before transfer:" << endl;
    acct1.display();
    acct2.display();

    // Alice transfers $300 to Bob
    acct1.transfer(300.00, acct2);

    cout << "After transfer ($300 from Alice to Bob):" << endl;
    acct1.display();
    acct2.display();

    // -------------------------------------------------------------------------
    // DEMO 4: Student class — grades and averages
    // -------------------------------------------------------------------------
    cout << "\n[DEMO 4] Student Class — grades + getAverage() + letter grade" << endl;
    cout << "------------------------------------------------" << endl;

    Student student1("Charlie");
    student1.addGrade(85.0);
    student1.addGrade(90.0);
    student1.addGrade(78.0);
    student1.addGrade(92.0);

    Student student2("Diana");
    student2.addGrade(55.0);
    student2.addGrade(70.0);
    student2.addGrade(65.0);

    Student student3("Edward");
    // No grades yet — average will be 0

    cout << "\n-- Student summaries --" << endl;
    student1.display();
    student2.display();
    student3.display();

    // -------------------------------------------------------------------------
    // DEMO 5: Vector of objects
    // -------------------------------------------------------------------------
    cout << "\n[DEMO 5] Vector of BankAccount objects" << endl;
    cout << "------------------------------------------------" << endl;

    vector<BankAccount> accountList;
    accountList.push_back(BankAccount("Frank", 500.00));
    accountList.push_back(BankAccount("Grace", 750.00));
    accountList.push_back(BankAccount("Henry", 1000.00));

    cout << "\nAll accounts:" << endl;
    for (int i = 0; i < accountList.size(); i++) {
        accountList[i].display();
    }

    // Find account with highest balance using getBalance()
    cout << "\n-- Finding highest balance --" << endl;
    BankAccount *highest = &accountList[0];
    for (int i = 1; i < accountList.size(); i++) {
        if (accountList[i].getBalance() > highest->getBalance()) {
            highest = &accountList[i];
        }
    }
    cout << "Highest balance: ";
    highest->display();

    // -------------------------------------------------------------------------
    // DEMO 6: Getter methods — accessing private data safely
    // -------------------------------------------------------------------------
    cout << "\n[DEMO 6] Getters — safely accessing private data" << endl;
    cout << "------------------------------------------------" << endl;

    cout << "acct1 info via getters:" << endl;
    cout << "  Account #: " << acct1.getAccountNumber() << endl;
    cout << "  Holder: " << acct1.getHolderName() << endl;
    cout << "  Balance: $" << acct1.getBalance() << endl;

    // -------------------------------------------------------------------------
    // DEMO 7: const member functions
    // -------------------------------------------------------------------------
    cout << "\n[DEMO 7] const correctness — display() doesn't modify object" << endl;
    cout << "------------------------------------------------" << endl;

    const BankAccount constAcct("Test User", 123.45);
    // constAcct.deposit(50.00);  // ERROR! Can't call non-const on const object
    constAcct.display();  // OK — display() is const qualified

    // -------------------------------------------------------------------------
    // SUMMARY
    // -------------------------------------------------------------------------
    cout << "\n[SUMMARY] What We Learned" << endl;
    cout << "======================================================" << endl;
    cout << "Class: Blueprint that bundles data + functions together" << endl;
    cout << "Object: Instance of a class (created with constructor)" << endl;
    cout << "Private: Data hidden from outside — must use methods" << endl;
    cout << "Public: Methods others can call" << endl;
    cout << "Constructor: Auto-runs when object is created" << endl;
    cout << "Overloaded constructors: Different ways to create an object" << endl;
    cout << "Getters: Safe read access to private data" << endl;
    cout << "const methods: Promise not to modify the object" << endl;
    cout << "Destructor: Auto-runs when object is destroyed" << endl;

    cout << "\n[NOTE] Destructors will fire as objects go out of scope" << endl;
    cout << "       (see [ACCOUNT CLOSED] messages above)" << endl;

    cout << "\nDone!" << endl;
    return 0;
}