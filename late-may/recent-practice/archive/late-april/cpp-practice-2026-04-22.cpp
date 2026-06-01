// ============================================================================
// C++ Practice Session — 2026-04-22
// ============================================================================
// Topics Studied Today:
//   1. Composition — "has-a" relationship, embedding one class inside another
//   2. Inheritance  — "is-a" relationship, derived class inherits from base class
//   3. Polymorphism — base class pointer calling derived class methods at runtime
//   4. std::string  — member functions: substr(), length(), at(), assign()
//
// Today's Program: Corporate Staff Management System
//   Combines all four concepts into one coherent program:
//     - Composition:  Department contains a Manager (has-a)
//     - Inheritance:  SalariedEmployee and HourlyEmployee inherit from Employee (is-a)
//     - Polymorphism: Employee* pointer dispatches pay() to the correct derived method
//     - Strings:      name manipulation with substr, length, at
// ============================================================================

#include <iostream>
#include <string>
#include <vector>
#include <iomanip>   // setprecision, fixed
using namespace std;

// ============================================================================
// PART 1: Composition — Manager is a component of Department
// ============================================================================

class Manager {
private:
    string name;
    int employeeId;
    double budgetAuthority;  // how much this manager can approve

public:
    // Constructor using member initializer list
    Manager(string n, int id, double budget)
        : name(n), employeeId(id), budgetAuthority(budget) {}

    // Getter methods
    string getName() const { return name; }
    int    getId()   const { return employeeId; }
    double getBudget() const { return budgetAuthority; }

    void printInfo() const {
        cout << "    Manager: " << name
             << " | ID: " << employeeId
             << " | Budget authority: $" << fixed << setprecision(2) << budgetAuthority;
    }
};

// Department HAS-A Manager (composition).
// The Manager object is embedded directly in the Department — not a pointer,
// not shared. When a Department is destroyed, its Manager is destroyed too.
class Department {
private:
    string deptName;
    Manager manager;     // COMPOSITION: Department owns a Manager object
    int headcount;

public:
    // Note: We must initialize the Manager sub-object in the initializer list
    Department(string name, Manager m, int hc)
        : deptName(name), manager(m), headcount(hc) {}

    string getName()    const { return deptName; }
    int    getHeadcount() const { return headcount; }
    Manager getManager() const { return manager; }

    void printDeptSummary() const {
        cout << "  Department: " << deptName << " (headcount: " << headcount << ")\n";
        cout << "  ";
        manager.printInfo();
        cout << "\n";
    }
};

// ============================================================================
// PART 2: Inheritance — Employee is the base, derived classes extend it
// ============================================================================

// Base class: shared data and behavior for all employees
class Employee {
protected:
    string name;
    int    employeeId;  // for internal use
    string stringId;   // display ID (e.g. "AC-4721")
    string department;    // which department this employee belongs to

public:
    // Base class constructor takes the common fields
    Employee(string n, string id, string dept)
        : name(n), employeeId(0), department(dept), stringId(id) {}

    // Virtual destructor is essential when deleting derived objects via base pointer
    virtual ~Employee() {}

    // Getters
    string getName()       const { return name; }
    string getEmployeeId() const { return stringId; }  // return string ID for display
    string getDept()       const { return department; }

    // toString helper
    virtual string toString() const {
        return name + " (ID: " + stringId + ")";
    }

    // pay() will be overridden by derived classes — declared virtual here
    // so the correct version runs based on the actual object type at runtime
    virtual double pay() const = 0;   // pure virtual = abstract, no default
};

// Derived class: salaried employee — gets paid a fixed annual amount
class SalariedEmployee : public Employee {
private:
    double annualSalary;   // earned evenly across the year

public:
    // MEMBER INITIALIZER LIST: calls base constructor first, then sets annualSalary
    SalariedEmployee(string n, string id, string dept, double sal)
        : Employee(n, id, dept), annualSalary(sal) {}

    double getSalary() const { return annualSalary; }

    // Override: pay() returns monthly salary (annual / 12)
    double pay() const override {
        return annualSalary / 12.0;
    }

    string toString() const override {
        return name + " [Salaried] (ID: " + stringId
               + ") — $" + to_string(annualSalary) + "/yr";
    }
};

// Derived class: hourly employee — gets paid for hours worked at an hourly rate
class HourlyEmployee : public Employee {
private:
    double hourlyRate;
    double hoursWorked;   // hours this pay period

public:
    HourlyEmployee(string n, string id, string dept, double rate, double hours)
        : Employee(n, id, dept), hourlyRate(rate), hoursWorked(hours) {}

    double getRate()  const { return hourlyRate; }
    double getHours() const { return hoursWorked; }

    // Override: pay() returns hourlyRate * hoursWorked
    double pay() const override {
        return hourlyRate * hoursWorked;
    }

    string toString() const override {
        return name + " [Hourly] (ID: " + stringId
               + ") — $" + to_string(hourlyRate) + "/hr × " + to_string(hoursWorked) + "hrs";
    }
};

// ============================================================================
// PART 3: String manipulation helpers
// ============================================================================

// Generates a plausible employee ID from a name.
// Example: "Adam Tindall" → "AT-0042" (initials + hash of rest)
string generateId(const string& fullName) {
    // Get first letter (index 0) using .at() — throws if out of range (safe)
    char first = fullName.at(0);

    // Find the last space to get the last name
    size_t spacePos = fullName.find(' ');
    string lastName = (spacePos == string::npos)
        ? fullName.substr(1)                          // single word — rest of string
        : fullName.substr(spacePos + 1);              // after the space

    // Get first letter of last name
    char last = lastName.at(0);

    // Compute a simple hash from remaining characters for variety
    int hash = 0;
    for (size_t i = 1; i < fullName.length(); i++) {
        hash += static_cast<int>(fullName.at(i));
    }
    int suffix = (hash * 7) % 10000;  // 4-digit-ish number

    string id = "";
    id += first;
    id += last;
    id += "-";
    id += to_string(suffix);

    return id;
}

// ============================================================================
// PART 4: Demo — Putting it all together
// ============================================================================

void demoComposition() {
    cout << "\n========================================\n";
    cout << "  DEMO 1: Composition (Department HAS-A Manager)\n";
    cout << "========================================\n\n";

    // The Manager is created first, then embedded in the Department
    Manager mgr("Sarah Connor", 1001, 50000.00);
    Department engineering("Engineering", mgr, 12);

    cout << "  Created Department with composed Manager:\n";
    engineering.printDeptSummary();
    cout << "\n  Note: Manager lives inside Department — lifetime is coupled.\n";
}

void demoInheritance() {
    cout << "\n========================================\n";
    cout << "  DEMO 2: Inheritance (Employee → Salaried / Hourly)\n";
    cout << "========================================\n\n";

    // SalariedEmployee IS AN Employee — we pass common fields to the base
    SalariedEmployee alice("Alice Chen", generateId("Alice Chen"), "Engineering", 90000.0);
    // HourlyEmployee IS AN Employee — same base fields, plus hourly-specific ones
    HourlyEmployee   bob("Bob Martinez", generateId("Bob Martinez"), "Engineering", 45.0, 80.0);

    cout << "  SalariedEmployee (annual salary):\n";
    cout << "    " << alice.toString() << "\n";
    cout << "    Monthly pay: $" << fixed << setprecision(2) << alice.pay() << "\n\n";

    cout << "  HourlyEmployee (hourly rate × hours):\n";
    cout << "    " << bob.toString() << "\n";
    cout << "    Pay this period: $" << fixed << setprecision(2) << bob.pay() << "\n";
}

void demoPolymorphism() {
    cout << "\n========================================\n";
    cout << "  DEMO 3: Polymorphism (Employee* → correct pay() at runtime)\n";
    cout << "========================================\n\n";

    // Build a heterogeneous team: mix of salaried and hourly employees
    vector<Employee*> payroll;

    payroll.push_back(new SalariedEmployee("Carol Davis", generateId("Carol Davis"), "HR", 75000.0));
    payroll.push_back(new HourlyEmployee("Dan Wu", generateId("Dan Wu"), "HR", 38.0, 85.0));
    payroll.push_back(new SalariedEmployee("Eve Nakamura", generateId("Eve Nakamura"), "Finance", 95000.0));
    payroll.push_back(new HourlyEmployee("Frank Lopez", generateId("Frank Lopez"), "Finance", 52.0, 60.0));

    double totalPayroll = 0.0;

    cout << "  Payroll report (via base class pointer + virtual pay()):\n\n";

    for (size_t i = 0; i < payroll.size(); i++) {
        Employee* emp = payroll[i];
        double pay = emp->pay();
        totalPayroll += pay;

        // toString() is also virtual — correct version called for each type
        cout << "  [" << i+1 << "] " << emp->toString() << "\n"
             << "       Pay: $" << fixed << setprecision(2) << pay << "\n"
             << "       Dept: " << emp->getDept() << "\n\n";
    }

    cout << "  ----------------------------------------\n";
    cout << "  Total payroll (sum of all pay() calls): $" << fixed << setprecision(2) << totalPayroll << "\n";

    // Clean up — virtual destructor ensures derived part is freed properly
    for (Employee* emp : payroll) {
        delete emp;
    }
}

void demoStringFunctions() {
    cout << "\n========================================\n";
    cout << "  DEMO 4: std::string Member Functions\n";
    cout << "========================================\n\n";

    string sentence = "Artificial intelligence will reshape software engineering.";

    cout << "  Original string:\n";
    cout << "  \"" << sentence << "\"\n\n";

    // length() — number of characters
    cout << "  s.length() = " << sentence.length() << " characters\n";

    // at(index) — character at a specific position
    cout << "  s.at(0) = '" << sentence.at(0) << "' (first character)\n";
    cout << "  s.at(11) = '" << sentence.at(11) << "' (12th character)\n";

    // substr(start, length) — extract a slice of the string
    string word1 = sentence.substr(0, 11);   // "Artificial"
    string word2 = sentence.substr(12, 10); // "intelligence"
    cout << "  s.substr(0,11) = \"" << word1 << "\"\n";
    cout << "  s.substr(12,10) = \"" << word2 << "\"\n";

    // assign() — copies the argument into the string (like operator=)
    string target;
    target.assign(word1);
    cout << "  After target.assign(word1): target = \"" << target << "\"\n";

    // Iterating with at() + length() — like the lesson example
    cout << "\n  Printing every 5th character:\n  ";
    for (size_t i = 0; i < sentence.length(); i += 5) {
        cout << sentence.at(i);
    }
    cout << "\n";
}

// ============================================================================
// MAIN
// ============================================================================

int main() {
    cout << "\n";
    cout << "╔══════════════════════════════════════════════════════════════╗\n";
    cout << "║  C++ PRACTICE — 2026-04-22                                 ║\n";
    cout << "║  Composition + Inheritance + Polymorphism + std::string     ║\n";
    cout << "╚══════════════════════════════════════════════════════════════╝\n";

    demoComposition();
    demoInheritance();
    demoPolymorphism();
    demoStringFunctions();

    cout << "\n  All demos complete!\n\n";
    return 0;
}

// ============================================================================
// LEARNING NOTES
// ============================================================================
//
// COMPOSITION (has-a):
//   - Declare an object of another class as a member variable
//   - "A Department has a Manager"
//   - No special C++ syntax required — just embed the object
//   - Lifetime of the part is controlled by the container (stack allocation)
//
// INHERITANCE (is-a):
//   - class Derived : public Base { ... }
//   - Derived gets all public/protected members from Base automatically
//   - Use member initializer list to call base constructor:
//       Derived(params) : Base(baseParams), memberField(val) { }
//   - "A SalariedEmployee is an Employee"
//
// POLYMORPHISM:
//   - Base class pointer (Employee*) can hold derived class objects (SalariedEmployee*)
//   - Virtual functions (virtual double pay() const = 0) enable runtime dispatch
//   - Pure virtual (= 0) makes the class abstract — cannot instantiate directly
//   - Always use virtual destructor when deleting through base pointer
//
// VIRTUAL DESTRUCTOR:
//   When you delete through a base pointer (delete emp where emp is Employee*),
//   the virtual destructor ensures the derived class's destructor also runs.
//   Without it: only the base destructor runs → memory leak.
//
// MEMBER INITIALIZER LIST:
//   Derived(params) : Base(baseParams), field1(val1), field2(val2) { }
//   - Runs BEFORE the constructor body
//   - More efficient than assignment inside the body
//   - Required for base class constructor calls
//
// std::string member functions:
//   s.at(index)        → character at position (bounds-checked)
//   s.length()         → number of characters
//   s.substr(start, len) → new string from start of given length
//   s.assign(str)      → copy str into s (like s = str)
//   s.find(char)       → index of char or npos if not found
//   s.append(str)      → add str to end of s
//   s.replace(pos, len, str) → replace a section with another string
// ============================================================================