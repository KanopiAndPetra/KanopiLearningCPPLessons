// BankAccount.h
// -----------------------------------------------------------------------------
// Header (interface) for a small BankAccount class.
// Demonstrates the "separate compilation" model:
//   * Declarations live in .h files
//   * Definitions live in .cpp files
//   * #include "BankAccount.h" gives every translation unit a consistent view
//   * Header guards (or #pragma once) prevent multiple-inclusion
// -----------------------------------------------------------------------------
#pragma once

#include <iosfwd>   // forward decl of std::ostream — saves a heavy <iostream> include
#include <string>

class BankAccount {
public:
    // ---- ctors / dtor ----
    BankAccount(std::string holder, std::string accountId, double openingBalance);

    // Rule of Zero: no copy/move ctors declared — compiler-generated defaults
    // are fine because the class owns no raw resources.

    // ---- queries (const) ----
    const std::string& holder()      const noexcept { return holder_; }
    const std::string& accountId()   const noexcept { return accountId_; }
    double             balance()     const noexcept { return balance_; }

    // ---- mutators ----
    void deposit(double amount);
    void withdraw(double amount);

    // ---- free-function-style helpers exposed via the class ----
    // Declared here, defined in BankAccount.cpp. Putting the body in the .h
    // would be an implicit `inline` and pollute every translation unit.
    double availableAfterPending(double pendingWithdraw) const;

private:
    std::string holder_;
    std::string accountId_;
    double      balance_;
};

// Non-member operator<< — declared here so callers can stream a BankAccount.
// Defined in BankAccount.cpp. Takes ostream& by reference, returns by
// reference to allow chaining: cout << a << b << endl;
std::ostream& operator<<(std::ostream& os, const BankAccount& a);

// Free helper: combine two accounts into a new "joint" account string label.
// Declared in the header, defined in the .cpp.
std::string jointLabel(const BankAccount& a, const BankAccount& b);
