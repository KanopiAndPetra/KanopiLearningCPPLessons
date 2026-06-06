// BankAccount.cpp
// -----------------------------------------------------------------------------
// Definitions for the BankAccount interface declared in BankAccount.h.
// This translation unit compiles to BankAccount.o, which main.cpp links
// against to produce the final executable.
// -----------------------------------------------------------------------------
#include "BankAccount.h"

#include <iostream>
#include <stdexcept>
#include <utility>

// Helper macro for the precondition checks. Shows up only in this .cpp, so the
// exception type is an implementation detail — callers see the throws.
#define REQUIRE(cond, msg)                                                    \
    do {                                                                       \
        if (!(cond)) {                                                         \
            throw std::invalid_argument(msg);                                  \
        }                                                                      \
    } while (0)

// ---- ctor ----
BankAccount::BankAccount(std::string holder, std::string accountId, double openingBalance)
    : holder_(std::move(holder)),
      accountId_(std::move(accountId)),
      balance_(openingBalance) {
    REQUIRE(!holder_.empty(),    "holder name must not be empty");
    REQUIRE(!accountId_.empty(), "accountId must not be empty");
    REQUIRE(openingBalance >= 0.0, "opening balance must be non-negative");
}

// ---- mutators ----
void BankAccount::deposit(double amount) {
    REQUIRE(amount > 0.0, "deposit amount must be positive");
    balance_ += amount;
}

void BankAccount::withdraw(double amount) {
    REQUIRE(amount > 0.0, "withdraw amount must be positive");
    REQUIRE(amount <= balance_, "insufficient funds");
    balance_ -= amount;
}

// ---- helper ----
double BankAccount::availableAfterPending(double pendingWithdraw) const {
    REQUIRE(pendingWithdraw >= 0.0, "pending withdraw must be non-negative");
    return balance_ - pendingWithdraw;
}

// ---- non-member operator<< ----
std::ostream& operator<<(std::ostream& os, const BankAccount& a) {
    // We use a multi-field, fixed-width-ish style so it's easy to read when
    // several accounts are printed back-to-back.
    os << "[" << a.accountId() << "] " << a.holder()
       << "  balance=$" << a.balance();
    return os;
}

// ---- free helper ----
std::string jointLabel(const BankAccount& a, const BankAccount& b) {
    // Picks the lexicographically smaller holder name first so the joint
    // label is canonical regardless of argument order.
    return (a.holder() <= b.holder())
        ? (a.holder() + " & " + b.holder())
        : (b.holder() + " & " + a.holder());
}
