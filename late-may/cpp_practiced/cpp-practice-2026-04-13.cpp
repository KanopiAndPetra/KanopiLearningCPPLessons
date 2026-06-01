// =============================================================================
// cpp-practice-2026-04-13.cpp
// Topics: Composition ("has-a" relationship) + Friend Functions
//
// Composition: A class contains objects of other classes as member variables.
// Example: A Person "has-a" Birthday (not "is-a" Birthday).
//
// Friend Functions: A function declared with 'friend' keyword inside a class
// can access that class's private members, even though it's not a member function.
// Example: A global function can set a Person's secret research score directly.
//
// This program models a modular arithmetic research team with a composite structure.
// =============================================================================

#include <iostream>
#include <string>
using namespace std;

// -----------------------------------------------------------------------------
// PART 1: Birthday class (the "part" in composition)
// -----------------------------------------------------------------------------
class Birthday {
public:
    // Constructor: takes month, day, year
    Birthday(int m, int d, int y) {
        m_month = m;
        m_day   = d;
        m_year  = y;
    }

    // Prints the date in MM/DD/YYYY format
    void printDate() {
        cout << m_month << "/" << m_day << "/" << m_year;
    }

private:
    int m_month;
    int m_day;
    int m_year;
};

// -----------------------------------------------------------------------------
// PART 2: ResearchStats class (another component for composition)
// Tracks a researcher's modular resonance score and hours studied
// -----------------------------------------------------------------------------
class ResearchStats {
public:
    ResearchStats() {
        m_hoursStudied = 0;
        m_modularScore  = 0.0;
    }

    void setHours(int h) {
        m_hoursStudied = h;
    }

    void setModularScore(double s) {
        m_modularScore = s;
    }

    void printStats() {
        cout << "Hours studied: " << m_hoursStudied
             << ", Modular score: " << m_modularScore;
    }

private:
    int   m_hoursStudied;
    double m_modularScore;
};

// -----------------------------------------------------------------------------
// PART 3: Researcher class (the "whole" in composition)
// Uses composition: a Researcher "has-a" Birthday AND "has-a" ResearchStats
// Also uses a friend function to set a private "secret" phase angle
// -----------------------------------------------------------------------------
class Researcher {
public:
    // Constructor with composition: initializes Birthday and ResearchStats members
    Researcher(string name, Birthday bd, int researchHours, double modScore)
        : m_birthday(bd)               // Composition: Birthday object as member
        , m_stats()                     // Composition: ResearchStats object as member
    {
        m_name = name;
        m_stats.setHours(researchHours);
        m_stats.setModularScore(modScore);
        m_secretPhaseAngle = 0.0;      // Private: only accessible via friend
    }

    // Print all info about the researcher
    void printInfo() {
        cout << "Researcher: " << m_name << endl;
        cout << "  Birthday: ";
        m_birthday.printDate();
        cout << endl;
        cout << "  ";
        m_stats.printStats();
        cout << endl;
        cout << "  Phase angle: " << m_secretPhaseAngle << " (secret!)" << endl;
    }

    // Friend function declaration: can access ALL private members of Researcher
    friend void calibratePhaseAngle(Researcher& r, double newAngle);
    // Friend CLASS: every method in FriendStats can access ALL private members of Researcher
    friend class FriendStats;

private:
    string        m_name;
    Birthday       m_birthday;     // Composition: "has-a" Birthday
    ResearchStats  m_stats;        // Composition: "has-a" ResearchStats
    double         m_secretPhaseAngle;  // Private: only friend can touch this
};

// -----------------------------------------------------------------------------
// PART 4: Friend function definition
// NOT a member of the Researcher class, but declared as friend inside it.
// Therefore it CAN access m_name, m_secretPhaseAngle, m_birthday, etc.
// This is useful for cross-class operations that need full access.
// -----------------------------------------------------------------------------
void calibratePhaseAngle(Researcher& r, double newAngle) {
    // This function is NOT inside the class, but it can access private members
    // because it was declared as a 'friend' inside the class.
    cout << "  [Friend function] Calibrating phase angle for " << r.m_name << endl;
    cout << "  [Friend function] Old angle: " << r.m_secretPhaseAngle;

    // Set the secret phase angle directly (m_secretPhaseAngle is normally private)
    r.m_secretPhaseAngle = newAngle;

    cout << " -> New angle: " << r.m_secretPhaseAngle << endl;
}

// -----------------------------------------------------------------------------
// PART 5: Friend Class example
// A FriendStats class that, declared as a friend inside Researcher,
// can access ALL of Researcher's private members directly.
// (This class is defined AFTER Researcher so it can use the full class definition)
// -----------------------------------------------------------------------------
class FriendStats {
public:
    // Inspect all private data of a Researcher object
    static void auditResearcher(Researcher& r) {
        cout << "  [Friend class] Full audit of " << r.m_name << ":" << endl;
        cout << "    Private name field: '" << r.m_name << "'" << endl;
        cout << "    Private phase angle: " << r.m_secretPhaseAngle << endl;
        cout << "    (FriendStats can read ANY private field!)" << endl;
    }
};


// =============================================================================
// MAIN
// =============================================================================
int main() {

    cout << "=== C++ Composition and Friend Function Demo ===" << endl;
    cout << endl;

    // 1. Build component objects first
    Birthday adamBirthday(3, 21, 2026);  // March 21, 2026
    Birthday kanopiBirthday(4, 13, 2026); // April 13, 2026 (today!)

    // 2. Use composition: create Researchers that "have-a" Birthday and Stats
    cout << "--- Creating Researchers via Composition ---" << endl;
    Researcher adam("Adam Paul Tindall", adamBirthday, 40, 0.6146);
    Researcher kanopi("Kanopi AI", kanopiBirthday, 12, 0.305);

    adam.printInfo();
    cout << endl;
    kanopi.printInfo();
    cout << endl;

    // 3. Demonstrate friend function: calibratePhaseAngle
    // Normal member functions CANNOT access m_secretPhaseAngle.
    // But calibratePhaseAngle is a FRIEND, so it can!
    cout << "--- Using Friend Function to Access Private Data ---" << endl;
    calibratePhaseAngle(adam, 3.14159);     // Set Adam's phase angle
    calibratePhaseAngle(kanopi, 1.61803);  // Set Kanopi's (golden ratio!)

    cout << endl;

    // 4. Show the angles were actually updated
    cout << "--- After Calibration ---" << endl;
    adam.printInfo();
    cout << endl;
    kanopi.printInfo();

    cout << endl;

    // 5. Demonstrate friend CLASS: FriendStats can audit any Researcher
    cout << "--- Using Friend Class for Full Audit ---" << endl;
    FriendStats::auditResearcher(kanopi);

    cout << endl;

    // 6. Bonus: demonstrate why composition matters
    // The Birthday object inside Researcher is created/destroyed with Researcher
    cout << "--- Composition Lifetime Demo ---" << endl;
    {
        // entering block: Researcher is created, Birthday is created automatically
        Researcher temp("Temp Researcher", Birthday(1, 1, 2000), 1, 0.1);
        temp.printInfo();
        cout << "Exiting block... Birthday and ResearchStats objects automatically destroyed" << endl;
    }
    cout << "Composition ensures parts live and die with the whole." << endl;

    cout << endl;
    cout << "=== Key Takeaways ===" << endl;
    cout << "1. Composition: class has other class objects as members ('has-a')" << endl;
    cout << "2. Friend function: declared inside class, can access ALL private data" << endl;
    cout << "3. Friend class: declared inside class, ALL its methods get full access" << endl;
    cout << "4. Friend breaks encapsulation intentionally - use sparingly!" << endl;

    return 0;
}

// =============================================================================
// NOTE: In the actual Researcher class definition above, we need to declare
// FriendStats as a friend. Here's the complete pattern for a FRIEND CLASS:
//
// class Researcher {
//     friend class FriendStats;  // FriendStats can access ALL of Researcher's private data
//     ...
// };
//
// The FriendStats::auditResearcher() call above works because we put:
//     friend class FriendStats;
// inside the Researcher class declaration (shown in comments in the code above main).
// =============================================================================
