// =============================================================================
// cpp-practice-2026-04-23.cpp
// Topic: Classes with Member Variables and Member Functions
//
// This file contains:
//   - The implementation of the Robot class declared in the header
//   - A main() function that creates Robot objects and calls their methods
//
// Inspired by: Header files concept from Oppie2CWithAI/Oppie2Cme C tutorials
// =============================================================================

#include <iostream>
#include <string>
#include <vector>
#include "cpp-practice-2026-04-23.h"  // Include our custom header

using namespace std;

// =============================================================================
// Robot Member Function Implementations
// =============================================================================

// Default constructor - creates a robot with default values
Robot::Robot() {
    name = "Unit";
    id = 0;
    energyLevel = 50;  // Start with half charge
    model = "Generic";
    cout << "[Robot system booting] Default constructor called." << endl;
}

// Parameterized constructor - creates a robot with custom values
Robot::Robot(string robotName, string robotModel) {
    name = robotName;
    id = rand() % 10000;  // Generate a random ID
    energyLevel = 100;    // New robots start fully charged
    model = robotModel;
    cout << "[Robot system booting] " << name << " (" << model << ") is online!" << endl;
}

// Robot introduces itself - uses member variables to display info
void Robot::introduce() {
    cout << "-----------------------------" << endl;
    cout << "Hello! I am " << name << "." << endl;
    cout << "My model designation: " << model << endl;
    cout << "My unit ID: #" << id << endl;
    cout << "-----------------------------" << endl;
}

// Display all robot status information
void Robot::displayStatus() {
    cout << "\n===== " << name << " Status =====" << endl;
    cout << "  Name:        " << name << endl;
    cout << "  Model:       " << model << endl;
    cout << "  ID:          #" << id << endl;
    cout << "  Energy:      " << energyLevel << "%" << endl;

    // Visual battery indicator using conditional logic
    cout << "  Battery:     [";
    int bars = energyLevel / 10;
    for (int i = 0; i < 10; i++) {
        if (i < bars) {
            cout << (energyLevel > 30 ? "█" : "▓");  // Full bar or low indicator
        } else {
            cout << "░";
        }
    }
    cout << "]" << endl;
    cout << "=============================" << endl;
}

// Charge the robot's battery
void Robot::charge(int amount) {
    cout << name << " is charging..." << endl;
    energyLevel += amount;
    if (energyLevel > 100) {
        energyLevel = 100;  // Can't exceed 100%
        cout << "Fully charged!" << endl;
    } else {
        cout << "Energy level now: " << energyLevel << "%" << endl;
    }
}

// Perform a task - uses energy, requires sufficient charge
void Robot::performTask(string taskName) {
    // Check if robot has enough energy to perform task (tasks cost 20 energy)
    if (energyLevel < 20) {
        cout << "ERROR: " << name << " cannot perform '" << taskName << "'" << endl;
        cout << "       Insufficient energy! Please charge." << endl;
        return;  // Early return - exit function without doing the task
    }

    // Perform the task
    cout << name << " is performing task: '" << taskName << "'..." << endl;
    energyLevel -= 20;
    cout << "Task complete! Energy remaining: " << energyLevel << "%" << endl;
}

// Getter function - returns the energy level (controlled access to private data)
int Robot::getEnergy() {
    return energyLevel;
}

// Getter function - returns the robot's name
string Robot::getName() {
    return name;
}


// =============================================================================
// Main Program - demonstrates the Robot class in action
// =============================================================================

int main() {
    cout << "\n*** C++ Class Practice: Robot Factory ***" << endl;
    cout << "Demonstrating member variables and member functions\n" << endl;

    // -------------------------------------------------------------------------
    // Create robots using different constructors
    // -------------------------------------------------------------------------
    cout << "\n--- Creating Robots ---" << endl;

    Robot defaultRobot;  // Uses default constructor
    Robot myRobot("TITAN-7", "Heavy-Duty Worker");  // Uses parameterized constructor

    // -------------------------------------------------------------------------
    // Call member functions on objects
    // -------------------------------------------------------------------------
    cout << "\n--- Robot Introductions ---" << endl;
    defaultRobot.introduce();
    myRobot.introduce();

    // -------------------------------------------------------------------------
    // Display status (shows all member variables)
    // -------------------------------------------------------------------------
    cout << "\n--- Initial Status Check ---" << endl;
    defaultRobot.displayStatus();
    myRobot.displayStatus();

    // -------------------------------------------------------------------------
    // Perform tasks (modifies member variables via internal logic)
    // -------------------------------------------------------------------------
    cout << "\n--- Task Execution ---" << endl;
    myRobot.performTask("Heavy lifting");
    myRobot.performTask("Assembly line work");
    myRobot.performTask("Quality inspection");

    // -------------------------------------------------------------------------
    // More status after tasks
    // -------------------------------------------------------------------------
    cout << "\n--- Status After Tasks ---" << endl;
    myRobot.displayStatus();

    // -------------------------------------------------------------------------
    // Show what happens when energy runs low
    // -------------------------------------------------------------------------
    cout << "\n--- Depleting Energy ---" << endl;
    myRobot.performTask("Welding");
    myRobot.performTask("Painting");
    myRobot.performTask("Packaging");
    myRobot.performTask("Moving");  // This one should fail due to low energy

    // -------------------------------------------------------------------------
    // Demonstrate getter functions
    // -------------------------------------------------------------------------
    cout << "\n--- Using Getter Functions ---" << endl;
    cout << "Using getName() and getEnergy() to check status..." << endl;
    cout << myRobot.getName() << " has " << myRobot.getEnergy() << "% energy remaining." << endl;

    // -------------------------------------------------------------------------
    // Recharge and continue working
    // -------------------------------------------------------------------------
    cout << "\n--- Recharging ---" << endl;
    myRobot.charge(60);  // Add 60% energy
    myRobot.displayStatus();

    // -------------------------------------------------------------------------
    // Final task to confirm recovery
    // -------------------------------------------------------------------------
    cout << "\n--- Final Task ---" << endl;
    myRobot.performTask("Final inspection");
    myRobot.displayStatus();

    cout << "\n*** Practice Complete: Member Variables & Functions Demonstrated ***" << endl;

    return 0;
}
