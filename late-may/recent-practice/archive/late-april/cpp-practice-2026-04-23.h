// =============================================================================
// C++ Practice - 2026-04-23
// Topic: Classes with Member Variables and Member Functions
//
// Builds on: Header files concept from C tutorials (Oppie2CWithAI/Oppie2Cme repos)
// =============================================================================

#include <iostream>
#include <string>
#include <vector>

// =============================================================================
// Robot.h - Header file declaring the Robot class
//
// A header file declares WHAT a class can do (interface/declaration).
// The actual implementation goes in the .cpp file.
// =============================================================================

#ifndef ROBOT_H
#define ROBOT_H

// The Robot class - a blueprint for creating robot objects
class Robot {
private:
    // Member variables (attributes) - data each robot stores
    // These are private: only the class itself can access them directly
    std::string name;
    int id;
    int energyLevel;      // 0-100 scale
    std::string model;

public:
    // Constructor declarations - special functions called when creating a Robot
    Robot();                                      // Default constructor
    Robot(std::string robotName, std::string robotModel); // Parameterized constructor

    // Member function declarations - things a Robot can do
    void introduce();              // Robot speaks its name and model
    void displayStatus();         // Show all robot info
    void charge(int amount);      // Add energy (up to 100)
    void performTask(std::string taskName); // Do work,消耗 energy
    int getEnergy();              // Getter for energyLevel
    std::string getName();       // Getter for name
};

#endif // ROBOT_H
