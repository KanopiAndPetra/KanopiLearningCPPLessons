// ============================================================================
// C++ Practice: Character Profile Manager
// Date: 2026-04-14
// ============================================================================
// Topics Practiced:
//   - Classes with member variables and member functions
//   - String manipulation methods (length, append, insert, find, erase)
//   - Wide strings (wstring) for Unicode support
//   - Pointers and dynamic memory
//   - Constructors and destructors
// ============================================================================

#include <iostream>
#include <string>
#include <vector>

// ============================================================================
// CLASS: CharacterProfile
// Represents a game character with various string-based attributes
// ============================================================================
class CharacterProfile {
private:
    // Member variables (attributes of the character)
    std::string name;
    std::string characterClass;
    std::string email;         // Simulated email for username simulation
    int level;
    int experience;

public:
    // --------------------------------------------------------------------
    // Constructor - initializes default character
    // --------------------------------------------------------------------
    CharacterProfile() {
        name = "Unknown";
        characterClass = "Adventurer";
        email = "";
        level = 1;
        experience = 0;
        std::cout << "  [CharacterProfile created: " << name << "]\n";
    }

    // --------------------------------------------------------------------
    // Overloaded Constructor - creates character with a name
    // --------------------------------------------------------------------
    CharacterProfile(std::string startingName, std::string startingClass) {
        name = startingName;
        characterClass = startingClass;
        email = "";
        level = 1;
        experience = 0;
        std::cout << "  [CharacterProfile created: " << name << " the " << characterClass << "]\n";
    }

    // --------------------------------------------------------------------
    // Destructor - cleanup when object is destroyed
    // --------------------------------------------------------------------
    ~CharacterProfile() {
        std::cout << "  [CharacterProfile destroyed: " << name << "]\n";
    }

    // --------------------------------------------------------------------
    // SETTERS - Modify character attributes
    // --------------------------------------------------------------------
    void setName(std::string newName) {
        // Using length() to validate name length
        if (newName.length() > 20) {
            std::cout << "  Name too long! Maximum 20 characters.\n";
            return;
        }
        name = newName;
        std::cout << "  Name set to: " << name << "\n";
    }

    void setClass(std::string newClass) {
        characterClass = newClass;
        std::cout << "  Class changed to: " << characterClass << "\n";
    }

    void generateEmail() {
        // Demonstrates string manipulation: append(), insert(), at()
        if (name.empty()) {
            std::cout << "  Cannot generate email: name is empty!\n";
            return;
        }

        // Start with first 4 chars of name (or entire name if shorter)
        std::string baseEmail = name.substr(0, std::min(4, (int)name.length()));
        
        // Convert to lowercase by accessing each character with at()
        for (int i = 0; i < baseEmail.length(); i++) {
            baseEmail.at(i) = tolower(baseEmail.at(i));
        }
        
        // Append the domain using append()
        baseEmail.append("@gameworld.com");
        
        email = baseEmail;
        std::cout << "  Email generated: " << email << "\n";
    }

    void gainExperience(int xp) {
        experience += xp;
        std::cout << "  Gained " << xp << " XP! Total: " << experience << "\n";
        
        // Check for level up every 100 XP
        while (experience >= 100) {
            levelUp();
        }
    }

    // --------------------------------------------------------------------
    // GETTERS - Retrieve character attributes
    // --------------------------------------------------------------------
    std::string getName() { return name; }
    std::string getClass() { return characterClass; }
    int getLevel() { return level; }
    int getExperience() { return experience; }
    std::string getEmail() { return email; }

    // --------------------------------------------------------------------
    // Display character stats
    // --------------------------------------------------------------------
    void displayStats() {
        std::cout << "\n  ╔══════════════════════════════════════╗\n";
        std::cout << "  ║        CHARACTER STATS              ║\n";
        std::cout << "  ╠══════════════════════════════════════╣\n";
        std::cout << "  ║ Name:     " << name;
        // Padding for alignment
        for (int i = name.length(); i < 20; i++) std::cout << " ";
        std::cout << "║\n";
        std::cout << "  ║ Class:    " << characterClass;
        for (int i = characterClass.length(); i < 20; i++) std::cout << " ";
        std::cout << "║\n";
        std::cout << "  ║ Level:    " << level;
        std::cout << "                        ║\n";
        std::cout << "  ║ XP:       " << experience << "/100";
        std::cout << "                    ║\n";
        if (!email.empty()) {
            std::cout << "  ║ Email:    " << email;
            for (int i = email.length(); i < 20; i++) std::cout << " ";
            std::cout << "║\n";
        }
        std::cout << "  ╚══════════════════════════════════════╝\n";
    }

private:
    // --------------------------------------------------------------------
    // Private helper function - only accessible within the class
    // --------------------------------------------------------------------
    void levelUp() {
        experience -= 100;
        level++;
        std::cout << "  ★ LEVEL UP! Now level " << level << " ★\n";
    }
};

// ============================================================================
// FUNCTION: demonstrateWideStrings()
// Shows how wstring works with Unicode characters
// ============================================================================
void demonstrateWideStrings() {
    std::cout << "\n  === Wide String Demo (Unicode Support) ===\n";
    
    // Wide strings use wchar_t and support Unicode
    std::wstring wideName = L"Hattori Hanzo";
    std::wstring wideTitle = L"忍者";
    
    // Convert wide string to regular string for display
    std::cout << "  Wide character name (as bytes): ";
    for (wchar_t c : wideName) {
        std::cout << (char)c;
    }
    std::cout << "\n";
    
    std::cout << "  Wide character title (Japanese): ";
    for (wchar_t c : wideTitle) {
        // Simple output - actual display depends on terminal encoding
        std::cout << (char)(c & 0xFF);
    }
    std::cout << "\n";
    
    std::cout << "  Length of wideName: " << wideName.length() << "\n";
    std::cout << "  Wide strings are useful for international characters!\n";
}

// ============================================================================
// FUNCTION: demonstratePointerUsage()
// Shows how pointers work with our character objects
// ============================================================================
void demonstratePointerUsage() {
    std::cout << "\n  === Pointer Demo ===\n";
    
    // Create character on the stack
    CharacterProfile hero("Link", "Hero");
    
    // Create pointer to the character
    CharacterProfile* characterPtr = &hero;
    
    // Access members through pointer using ->
    std::cout << "  Pointer access - Name: " << characterPtr->getName() << "\n";
    std::cout << "  Pointer access - Class: " << characterPtr->getClass() << "\n";
    std::cout << "  Address of character: " << characterPtr << "\n";
    
    // Modify through pointer
    characterPtr->gainExperience(50);
    
    std::cout << "  After gaining XP via pointer:\n";
    characterPtr->displayStats();
}

// ============================================================================
// FUNCTION: demonstrateStringMethods()
// Shows various string manipulation methods
// ============================================================================
void demonstrateStringMethods() {
    std::cout << "\n  === String Methods Demo ===\n";
    
    std::string testStr = "Hello World";
    
    std::cout << "  Original string: \"" << testStr << "\"\n";
    std::cout << "  Length: " << testStr.length() << "\n";
    std::cout << "  At index 6: '" << testStr.at(6) << "'\n";
    
    // find() - find character position
    size_t spacePos = testStr.find(' ');
    std::cout << "  Find ' ': position " << spacePos << "\n";
    
    // erase() - remove characters
    std::string modStr = testStr;
    modStr.erase(5, 6);  // Erase 6 chars starting at position 5
    std::cout << "  After erase(5,6): \"" << modStr << "\"\n";
    
    // insert() - insert characters
    modStr = "Hello";
    modStr.insert(5, " World");
    std::cout << "  After insert: \"" << modStr << "\"\n";
    
    // append() - append to string
    modStr.append("!");
    std::cout << "  After append: \"" << modStr << "\"\n";
    
    // clear() - empty the string
    modStr.clear();
    std::cout << "  After clear, empty? " << (modStr.empty() ? "Yes" : "No") << "\n";
}

// ============================================================================
// MAIN PROGRAM
// ============================================================================
int main() {
    std::cout << "╔══════════════════════════════════════════════════════════════╗\n";
    std::cout << "║   C++ PRACTICE SESSION - 2026-04-14                        ║\n";
    std::cout << "║   Character Profile Manager + String Methods + Pointers    ║\n";
    std::cout << "╚══════════════════════════════════════════════════════════════╝\n";

    // -------------------------------------------------------------------------
    // Part 1: Create and manage character profiles using the class
    // -------------------------------------------------------------------------
    std::cout << "\n  === Creating Character Profiles ===\n";
    
    // Create default character
    CharacterProfile* player1 = new CharacterProfile();
    
    // Create named character
    CharacterProfile player2("Sora", "Keyblade Wielder");
    
    // Set properties on player1
    player1->setName("Kanopi");
    player1->setClass("AI Assistant");
    player1->generateEmail();
    
    // Gain some experience
    player1->gainExperience(75);
    player1->gainExperience(50);  // This triggers a level up!
    
    // -------------------------------------------------------------------------
    // Part 2: Display stats
    // -------------------------------------------------------------------------
    std::cout << "\n  === Character Stats ===\n";
    player1->displayStats();
    player2.displayStats();
    
    // -------------------------------------------------------------------------
    // Part 3: String method demonstrations
    // -------------------------------------------------------------------------
    demonstrateStringMethods();
    
    // -------------------------------------------------------------------------
    // Part 4: Pointer demonstrations
    // -------------------------------------------------------------------------
    demonstratePointerUsage();
    
    // -------------------------------------------------------------------------
    // Part 5: Wide string demonstration
    // -------------------------------------------------------------------------
    demonstrateWideStrings();
    
    // -------------------------------------------------------------------------
    // Cleanup
    // -------------------------------------------------------------------------
    std::cout << "\n  === Cleanup (delete player1) ===\n";
    delete player1;
    
    std::cout << "\n  === Main function ending ===\n";
    std::cout << "  Note: player2 will be destroyed automatically when it goes out of scope\n";
    
    return 0;
}

// ============================================================================
// LEARNING NOTES:
// ============================================================================
// 
// CLASSES:
// - Classes bundle data (member variables) and functions (member methods)
// - Constructors initialize objects; destructor cleans up
// - Public members are accessible from outside; private only within class
//
// STRING METHODS:
// - length() / size()    → returns character count
// - at(index)            → returns character at position (with bounds check)
// - empty()              → returns true if string is empty
// - clear()              → makes string empty
// - append(str)         → adds to end of string
// - insert(pos, str)     → inserts at position
// - erase(pos, count)    → removes characters
// - find(char)           → returns position of character
// - substr(pos, count)   → returns portion of string
//
// POINTERS:
// - * declares a pointer variable
// - & gets the address of a variable
// - -> accesses members through pointer
// - new allocates memory on heap (must delete)
// - Stack variables auto-cleanup; heap must be manually deleted
//
// WIDE STRINGS:
// - wstring stores wide characters (Unicode support)
// - Use L prefix for wide string literals
// - Useful for international characters and non-ASCII text
// ============================================================================