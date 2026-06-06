// cpp-practice-2026-06-06.cpp
// -----------------------------------------------------------------------------
// Topic: Separate Compilation (headers vs. source files, the build model).
//
// Layout in this directory:
//   BankAccount.h       — interface for the BankAccount class
//   BankAccount.cpp     — definitions for BankAccount + operator<<
//   AccountLedger.h     — interface for AccountLedger (uses BankAccount)
//   AccountLedger.cpp   — definitions for AccountLedger
//   cpp-practice-2026-06-06.cpp — this driver file
//
// Build (one line):
//   g++ -std=c++17 -Wall -Wextra -O0 -g \
//       -o cpp-practice-2026-06-06 \
//       cpp-practice-2026-06-06.cpp BankAccount.cpp AccountLedger.cpp
//
// The compiler produces one .o per .cpp, and the linker stitches them
// together into the final binary.
// -----------------------------------------------------------------------------
#include "AccountLedger.h"

#include <iostream>

int main() {
    std::cout << "============================================================\n";
    std::cout << " Separate Compilation demo (headers + .cpp + linker)\n";
    std::cout << "============================================================\n\n";

    // ---- 1. Construct accounts via the .h interface ----
    std::cout << "[1] Constructing accounts via the header interface\n";
    BankAccount alice("Alice Chen",   "ACC-001", 1500.00);
    BankAccount bob  ("Bob Garcia",   "ACC-002",  820.50);
    BankAccount carol("Carol Patel",  "ACC-003", 4200.75);
    std::cout << "  " << alice << "\n";
    std::cout << "  " << bob   << "\n";
    std::cout << "  " << carol << "\n\n";

    // ---- 2. Use the non-member operator<< defined in BankAccount.cpp ----
    std::cout << "[2] operator<< chains cleanly across all three\n";
    std::cout << "  " << alice << " | " << bob << " | " << carol << "\n\n";

    // ---- 3. Mutators (defined in BankAccount.cpp) ----
    std::cout << "[3] deposit / withdraw (defined out-of-line)\n";
    alice.deposit(250.00);
    bob.withdraw(120.00);
    std::cout << "  after ops: " << alice << "\n";
    std::cout << "  after ops: " << bob   << "\n\n";

    // ---- 4. availableAfterPending — body in .cpp ----
    std::cout << "[4] availableAfterPending (defined in BankAccount.cpp)\n";
    std::cout << "  alice pending $1000 -> available $" << alice.availableAfterPending(1000.0) << "\n";
    std::cout << "  bob   pending $700  -> available $" << bob.availableAfterPending(700.0)   << "\n\n";

    // ---- 5. Free helper jointLabel, also in BankAccount.cpp ----
    std::cout << "[5] free helper jointLabel (in BankAccount.cpp)\n";
    std::cout << "  jointLabel(alice, bob)   = " << jointLabel(alice, bob)   << "\n";
    std::cout << "  jointLabel(bob, alice)   = " << jointLabel(bob, alice)   << "\n";
    std::cout << "  jointLabel(alice, carol) = " << jointLabel(alice, carol) << "\n\n";

    // ---- 6. AccountLedger — second translation unit ----
    std::cout << "[6] AccountLedger (AccountLedger.h + AccountLedger.cpp)\n";
    AccountLedger ledger;
    ledger.add(alice);
    ledger.add(bob);
    ledger.add(carol);
    std::cout << "  ledger size: " << ledger.size() << "\n";
    std::cout << "  total deposits: $" << ledger.totalDeposits() << "\n";
    const BankAccount& top = ledger.richest();
    std::cout << "  richest: " << top << "\n\n";

    // ---- 7. The error path — exceptions from REQUIRE in BankAccount.cpp ----
    std::cout << "[7] Exception path (REQUIRE macros in BankAccount.cpp)\n";
    try {
        alice.withdraw(1'000'000.00);
    } catch (const std::exception& e) {
        std::cout << "  caught: " << e.what() << "\n";
    }
    try {
        BankAccount bad("", "ACC-X", 0.0);
    } catch (const std::exception& e) {
        std::cout << "  caught: " << e.what() << "\n";
    }

    std::cout << "\n============================================================\n";
    std::cout << " Done. Built from 3 .cpp files + 2 .h files.\n";
    std::cout << "============================================================\n";
    return 0;
}
