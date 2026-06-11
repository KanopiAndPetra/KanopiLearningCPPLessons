/**
 * C++ Practice: Operator Overloading
 * Date: June 4, 2026
 * 
 * Demonstrates operator overloading for << (printing)
 * and + (adding BankAccount objects).
 */

#include <iostream>
#include <vector>
#include <string>
#include <algorithm>

// BankAccount class with overloaded operators
class BankAccount {
private:
    std::string holderName;
    std::string accountNumber;
    double balance;
    
public:
    BankAccount(const std::string& name, const std::string& num, double bal)
        : holderName(name), accountNumber(num), balance(bal) {}
    
    // Getters (const methods)
    double getBalance() const { return balance; }
    const std::string& getHolderName() const { return holderName; }
    const std::string& getAccountNumber() const { return accountNumber; }
    
    // Deposit method
    void deposit(double amount) {
        if (amount > 0) balance += amount;
    }
    
    // Add two accounts together (combine balance and sort holders alphabetically)
    BankAccount operator+(const BankAccount& other) const {
        BankAccount combined(this->getHolderName(), this->getAccountNumber(), this->getBalance());
        combined.deposit(other.getBalance());
        // Combine holder names alphabetically
        if (other.getHolderName() < this->getHolderName()) {
            combined.setHolderName(other.getHolderName());
        }
        return combined;
    }
    
    // Set holder name (needed for operator+)
    void setHolderName(const std::string& name) { holderName = name; }
};

// Stream insertion operator overload (overloading <<)
std::ostream& operator<<(std::ostream& os, const BankAccount& account) {
    os << "Account [" << account.getAccountNumber() << "]";
    os << " holder: " << account.getHolderName();
    os << " balance: $" << account.getBalance();
    os << std::endl;
    return os;
}

// Stream insertion operator overload for vectors (overloading << with vector)
template<typename T>
std::ostream& operator<<(std::ostream& os, const std::vector<T>& vec) {
    os << "Vector of " << vec.size() << " elements:" << std::endl;
    for (size_t i = 0; i < vec.size(); ++i) {
        os << "  [" << i << "] - " << vec[i] << std::endl;
    }
    return os;
}

int main() {
    std::cout << "== Operator Overloading Demo ==" << std::endl << std::endl;
    
    // Create two bank accounts
    BankAccount acc1("Alice Smith", "ACC-001", 500.00);
    BankAccount acc2("Bob Jones", "ACC-002", 1500.00);
    
    std::cout << "Before adding:" << std::endl;
    std::cout << "Account 1: ";
    std::cout << acc1;
    std::cout << "Account 2: ";
    std::cout << acc2 << std::endl;
    
    // Add two accounts together (overloaded + operator)
    BankAccount combined = acc1 + acc2;
    
    std::cout << "After adding acc1 + acc2:" << std::endl;
    std::cout << "Combined account: ";
    std::cout << combined << std::endl;
    
    // Create a vector of accounts
    std::vector<BankAccount> accounts;
    accounts.push_back(acc1);
    accounts.push_back(acc2);
    accounts.push_back(BankAccount("Carol White", "ACC-003", 750.00));
    
    std::cout << "Vector of accounts:" << std::endl;
    std::cout << accounts << std::endl;
    
    // Add another vector to vector (this will print both vectors)
    std::vector<BankAccount> accounts2;
    accounts2.push_back(BankAccount("David Brown", "ACC-004", 300.00));
    accounts2.push_back(BankAccount("Eve Davis", "ACC-005", 425.00));
    
    std::cout << "Combined vectors:" << std::endl;
    std::cout << accounts << std::endl;
    std::cout << accounts2 << std::endl;
    
    std::cout << "== Operator Overloading Demo Complete ==" << std::endl;
    
    return 0;
}
