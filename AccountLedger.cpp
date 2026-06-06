// AccountLedger.cpp
// -----------------------------------------------------------------------------
// Definitions for AccountLedger. Lives in its own translation unit so the
// compiler can compile it in parallel with BankAccount.cpp — that's the
// payoff of separate compilation: incremental builds, parallel compiles,
// and smaller recompile cascades when a single .cpp changes.
// -----------------------------------------------------------------------------
#include "AccountLedger.h"

#include <stdexcept>

void AccountLedger::add(BankAccount acct) {
    accounts_.push_back(std::move(acct));
}

const BankAccount& AccountLedger::richest() const {
    if (accounts_.empty()) {
        throw std::logic_error("richest() called on empty ledger");
    }
    std::size_t best = 0;
    for (std::size_t i = 1; i < accounts_.size(); ++i) {
        if (accounts_[i].balance() > accounts_[best].balance()) {
            best = i;
        }
    }
    return accounts_[best];
}

double AccountLedger::totalDeposits() const {
    double sum = 0.0;
    for (const auto& a : accounts_) {
        sum += a.balance();
    }
    return sum;
}
