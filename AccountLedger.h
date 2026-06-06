// AccountLedger.h
// -----------------------------------------------------------------------------
// A second header that *uses* BankAccount. Demonstrates that one .h can
// include another, and that header guards (#pragma once here) prevent
// the same declarations from being pasted in twice if a third .cpp includes
// both this and BankAccount.h directly.
// -----------------------------------------------------------------------------
#pragma once

#include "BankAccount.h"
#include <vector>

class AccountLedger {
public:
    // Store accounts by value — vector takes care of memory. The class
    // demonstrates composition, not pointer ownership.
    void add(BankAccount acct);

    // Returns the account with the highest balance. Throws if the ledger
    // is empty. Defined in AccountLedger.cpp.
    const BankAccount& richest() const;

    // Total of every balance. Defined in AccountLedger.cpp.
    double totalDeposits() const;

    std::size_t size() const noexcept { return accounts_.size(); }

private:
    std::vector<BankAccount> accounts_;
};
