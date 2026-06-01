// =============================================================================
// cpp-practice-2026-04-24.cpp
// Topic: Enums, Switch Statements, and Structs in C++
//
// This program demonstrates:
//   - Enumerations (enum) for named constant values
//   - Switch statements for multi-way branching
//   - Structs for grouping related data
//   - How these C++ features work together
//
// Inspired by: C switch statement concepts from Oppie2Cme lessons,
//              adapted for C++ with enums and structs
// =============================================================================

#include <iostream>
#include <string>
#include <iomanip>  // For setw() formatting

using namespace std;

// =============================================================================
// ENUMERATION - Defines a set of named integer constants
// =============================================================================
// An enum creates a new type where each name maps to an integer value.
// By default, counting starts at 0: WARRIOR=0, MAGE=1, ROGUE=2, CLERIC=3
// This makes code more readable than using "magic numbers" directly.

enum CharacterClass {
    WARRIOR,    // 0 - Masters of sword and shield
    MAGE,       // 1 - Wields arcane magic
    ROGUE,      // 2 - Expert in stealth and daggers
    CLERIC      // 3 - Healer and divine magic user
};

// Another enum - weapon types with explicit values
enum WeaponType {
    SWORD = 1,      // Value 1
    STAFF = 2,      // Value 2  
    DAGGER = 3,     // Value 3
    MACE = 4        // Value 4
};

// =============================================================================
// STRUCT - Groups related data without functions (lighter than class)
// =============================================================================
// A struct is a simple way to group variables together.
// Unlike classes, structs don't have member functions by default.
// Great for plain data containers.

struct Character {
    string name;           // Character's name
    CharacterClass job;    // Their class (uses our enum type)
    int level;             // Current level
    int health;            // Current health points
    int maxHealth;         // Maximum health
    int mana;              // Magic points
    int maxMana;           // Maximum mana
    WeaponType weapon;     // Their equipped weapon
};

// =============================================================================
// FUNCTION PROTOTYPES
// =============================================================================

void displayCharacterInfo(Character& ch);        // Show all character stats
void performClassAction(Character& ch);          // Switch demo: class-specific action
void attackWithWeapon(Character& ch, WeaponType w);// Switch demo: weapon-based attack
string getClassName(CharacterClass c);           // Convert enum to readable string
string getWeaponName(WeaponType w);              // Convert weapon enum to string

// =============================================================================
// MAIN - Creates characters and demonstrates enums + switch
// =============================================================================

int main() {
    cout << "\n========================================================" << endl;
    cout << "   C++ Practice: Enums, Switch Statements, and Structs" << endl;
    cout << "========================================================" << endl;
    
    // -------------------------------------------------------------------------
    // Creating characters using our struct and enum
    // -------------------------------------------------------------------------
    
    // Character 1: A Warrior
    Character warrior;
    warrior.name = "Ironforge";
    warrior.job = WARRIOR;        // Using enum value
    warrior.level = 15;
    warrior.health = 150;
    warrior.maxHealth = 150;
    warrior.mana = 20;
    warrior.maxMana = 20;
    warrior.weapon = SWORD;
    
    // Character 2: A Mage
    Character mage;
    mage.name = "Eldric";
    mage.job = MAGE;
    mage.level = 12;
    mage.health = 80;
    mage.maxHealth = 80;
    mage.mana = 200;
    mage.maxMana = 200;
    mage.weapon = STAFF;
    
    // Character 3: A Rogue
    Character rogue;
    rogue.name = "Shadowstep";
    rogue.job = ROGUE;
    rogue.level = 18;
    rogue.health = 100;
    rogue.maxHealth = 100;
    rogue.mana = 50;
    rogue.maxMana = 50;
    rogue.weapon = DAGGER;
    
    // Character 4: A Cleric
    Character cleric;
    cleric.name = "Aurora";
    cleric.job = CLERIC;
    cleric.level = 14;
    cleric.health = 120;
    cleric.maxHealth = 120;
    cleric.mana = 150;
    cleric.maxMana = 150;
    cleric.weapon = MACE;
    
    // -------------------------------------------------------------------------
    // Display all characters
    // -------------------------------------------------------------------------
    cout << "\n--- CHARACTER ROSTER ---" << endl;
    displayCharacterInfo(warrior);
    displayCharacterInfo(mage);
    displayCharacterInfo(rogue);
    displayCharacterInfo(cleric);
    
    // -------------------------------------------------------------------------
    // Demonstrate SWITCH with enums - Class-specific abilities
    // -------------------------------------------------------------------------
    cout << "\n--- CLASS ABILITIES (Switch Statement Demo) ---" << endl;
    cout << "Each character performs their class-specific action:\n" << endl;
    
    performClassAction(warrior);
    performClassAction(mage);
    performClassAction(rogue);
    performClassAction(cleric);
    
    // -------------------------------------------------------------------------
    // Demonstrate SWITCH with enums - Weapon-based attacks
    // -------------------------------------------------------------------------
    cout << "\n--- WEAPON ATTACKS (Switch Statement Demo) ---" << endl;
    cout << "Each character attacks using their weapon:\n" << endl;
    
    attackWithWeapon(warrior, warrior.weapon);
    attackWithWeapon(mage, mage.weapon);
    attackWithWeapon(rogue, rogue.weapon);
    attackWithWeapon(cleric, cleric.weapon);
    
    // -------------------------------------------------------------------------
    // Bonus: Simulate combat and show how enums make code readable
    // -------------------------------------------------------------------------
    cout << "\n--- COMBAT SIMULATION ---" << endl;
    
    // Warrior attacks Rogue
    cout << warrior.name << " (" << getClassName(warrior.job) << ") attacks " << rogue.name << "!" << endl;
    rogue.health -= 35;
    cout << "  -> " << rogue.name << " takes 35 damage! Health: " << rogue.health << endl;
    
    // Rogue counterattacks
    cout << rogue.name << " (" << getClassName(rogue.job) << ") counterattacks!" << endl;
    warrior.health -= 25;
    cout << "  -> " << warrior.name << " takes 25 damage! Health: " << warrior.health << endl;
    
    // Mage casts a spell
    cout << mage.name << " (" << getClassName(mage.job) << ") casts magic missile!" << endl;
    rogue.health -= 50;
    cout << "  -> " << rogue.name << " takes 50 damage! Health: " << rogue.health << endl;
    
    // Cleric heals
    cout << cleric.name << " (" << getClassName(cleric.job) << ") heals " << warrior.name << "!" << endl;
    warrior.health += 40;
    if (warrior.health > warrior.maxHealth) warrior.health = warrior.maxHealth;
    cout << "  -> " << warrior.name << " recovered 40 HP! Health: " << warrior.health << endl;
    
    // -------------------------------------------------------------------------
    // Final status
    // -------------------------------------------------------------------------
    cout << "\n--- FINAL STATUS ---" << endl;
    displayCharacterInfo(warrior);
    displayCharacterInfo(mage);
    displayCharacterInfo(rogue);
    displayCharacterInfo(cleric);
    
    cout << "\n========================================================" << endl;
    cout << "   Practice Complete: Enums, Switch, and Structs!" << endl;
    cout << "========================================================" << endl;
    
    return 0;
}

// =============================================================================
// FUNCTION IMPLEMENTATIONS
// =============================================================================

// Display all information about a character
void displayCharacterInfo(Character& ch) {
    cout << "--------------------------------------------------------" << endl;
    cout << " Name:     " << ch.name << endl;
    cout << " Class:    " << getClassName(ch.job) << endl;
    cout << " Level:    " << ch.level << endl;
    cout << " Health:   " << ch.health << "/" << ch.maxHealth << " HP" << endl;
    cout << " Mana:     " << ch.mana << "/" << ch.maxMana << " MP" << endl;
    cout << " Weapon:   " << getWeaponName(ch.weapon) << endl;
    cout << "--------------------------------------------------------" << endl;
}

// Demonstrate SWITCH statement with CharacterClass enum
// This shows how switch replaces long if-else chains for equality checks
void performClassAction(Character& ch) {
    cout << ch.name << " uses ability: ";
    
    // Switch on the enum value - checks which class the character has
    switch (ch.job) {
        case WARRIOR:
            cout << "Power Strike! (Slam attack with " << getWeaponName(ch.weapon) << ")" << endl;
            break;  // Important! Without break, execution falls through to next case
            
        case MAGE:
            cout << "Arcane Blast! (Magic damage spell)" << endl;
            break;
            
        case ROGUE:
            cout << "Stealth Attack! (Backstab for extra damage)" << endl;
            break;
            
        case CLERIC:
            cout << "Divine Heal! (Restore health to self or ally)" << endl;
            break;
            
        default:
            // This runs if none of the cases matched
            cout << "Unknown ability!" << endl;
            break;
    }
}

// Demonstrate SWITCH statement with WeaponType enum
// Shows switch handling different weapon types
void attackWithWeapon(Character& ch, WeaponType w) {
    cout << ch.name << " attacks with " << getWeaponName(w) << ": ";
    
    // Switch on weapon type to determine attack style
    switch (w) {
        case SWORD:
            cout << "Slash! Deals heavy physical damage." << endl;
            break;
            
        case STAFF:
            cout << "Magic Blast! Deals arcane damage." << endl;
            break;
            
        case DAGGER:
            cout << "Quick Thrust! Fast but light damage." << endl;
            break;
            
        case MACE:
            cout << "Crushing Blow! High damage, slows enemy." << endl;
            break;
            
        default:
            cout << "Basic attack!" << endl;
            break;
    }
}

// Convert CharacterClass enum to readable string
// This is a common pattern when you need to display enum values
string getClassName(CharacterClass c) {
    string result;
    
    switch (c) {
        case WARRIOR:  result = "Warrior";   break;
        case MAGE:     result = "Mage";      break;
        case ROGUE:    result = "Rogue";     break;
        case CLERIC:   result = "Cleric";    break;
        default:       result = "Unknown";   break;
    }
    
    return result;
}

// Convert WeaponType enum to readable string
string getWeaponName(WeaponType w) {
    string result;
    
    switch (w) {
        case SWORD:  result = "Sword";  break;
        case STAFF:  result = "Staff";  break;
        case DAGGER: result = "Dagger"; break;
        case MACE:   result = "Mace";   break;
        default:     result = "Unknown"; break;
    }
    
    return result;
}