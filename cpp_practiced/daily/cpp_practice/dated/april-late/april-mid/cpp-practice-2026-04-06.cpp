// cpp-practice-2026-04-06.cpp
// Practice: File I/O with Polymorphism
// Builds on: Virtual functions (April 5), Classes/Pointers (April 1)
// New concept: saving/loading polymorphic objects via file streams

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <sstream>

using namespace std;

// ============================================================
// BASE CLASS: Shape — with virtual serialize/deserialize
// ============================================================
class Shape {
protected:
    string name;
    string color;

public:
    Shape(string n = "Shape", string c = "white") : name(n), color(c) {}
    virtual ~Shape() {}

    // PURE VIRTUAL — each derived class must implement
    virtual double getArea() const = 0;
    virtual double getPerimeter() const = 0;

    // SERIALIZE — write to file
    virtual void writeToFile(ofstream& out) const {
        out << type() << endl;
        out << name << endl;
        out << color << endl;
    }

    // DESERIALIZE — read from file (factory method)
    virtual void readFromFile(ifstream& in) {
        getline(in, name);
        getline(in, color);
    }

    // TYPE IDENTIFIER — needed to reconstruct correct derived type
    virtual string type() const = 0;

    // Describe the shape
    virtual void describe() const {
        cout << name << " (" << color << ")"
             << " — area: " << getArea()
             << ", perimeter: " << getPerimeter() << endl;
    }
};

// ============================================================
// DERIVED: Rectangle
// ============================================================
class Rectangle : public Shape {
private:
    double width;
    double height;

public:
    Rectangle(double w = 0, double h = 0, string c = "blue")
        : Shape("Rectangle", c), width(w), height(h) {}

    double getArea() const override { return width * height; }
    double getPerimeter() const override { return 2 * (width + height); }

    string type() const override { return "Rectangle"; }

    void writeToFile(ofstream& out) const override {
        out << type() << endl;
        out << name << endl;
        out << color << endl;
        out << width << endl;
        out << height << endl;
    }

    void readFromFile(ifstream& in) override {
        Shape::readFromFile(in);
        in >> width >> height;
        in.ignore();  // clear newline after height
    }
};

// ============================================================
// DERIVED: Circle
// ============================================================
class Circle : public Shape {
private:
    double radius;
    static constexpr double PI = 3.14159265359;

public:
    Circle(double r = 0, string c = "red")
        : Shape("Circle", c), radius(r) {}

    double getArea() const override { return PI * radius * radius; }
    double getPerimeter() const override { return 2 * PI * radius; }

    string type() const override { return "Circle"; }

    void writeToFile(ofstream& out) const override {
        out << type() << endl;
        out << name << endl;
        out << color << endl;
        out << radius << endl;
    }

    void readFromFile(ifstream& in) override {
        Shape::readFromFile(in);
        in >> radius;
        in.ignore();
    }
};

// ============================================================
// FACTORY: Create Shape from file type identifier
// ============================================================
Shape* createShapeFromFile(ifstream& in) {
    string type;
    getline(in, type);

    Shape* shape = nullptr;
    if (type == "Rectangle") {
        shape = new Rectangle();
    } else if (type == "Circle") {
        shape = new Circle();
    }

    if (shape) {
        shape->readFromFile(in);
    }
    return shape;
}

// ============================================================
// DEMONSTRATION: File operations
// ============================================================
void demonstrateFileIO() {
    cout << "=== File I/O with Polymorphism ===" << endl;

    const string filename = "shapes_data.txt";

    // Create a collection of shapes
    vector<Shape*> shapes;
    shapes.push_back(new Rectangle(5.0, 3.0, "blue"));
    shapes.push_back(new Circle(2.5, "red"));
    shapes.push_back(new Rectangle(4.0, 6.0, "green"));
    shapes.push_back(new Circle(1.0, "yellow"));

    // Write ALL shapes to file
    cout << "Writing " << shapes.size() << " shapes to " << filename << "..." << endl;
    {
        ofstream out(filename.c_str());
        if (!out) {
            cerr << "ERROR: Could not open file for writing!" << endl;
            return;
        }

        // Write count first (so we know how many to read)
        out << shapes.size() << endl;

        for (const Shape* s : shapes) {
            s->writeToFile(out);
        }
        out.close();
    }
    cout << "Write complete." << endl;

    // Read shapes back from file
    cout << "\nReading shapes from file..." << endl;
    vector<Shape*> loadedShapes;
    {
        ifstream in(filename.c_str());
        if (!in) {
            cerr << "ERROR: Could not open file for reading!" << endl;
            return;
        }

        size_t count;
        in >> count;
        in.ignore();  // clear newline

        for (size_t i = 0; i < count; i++) {
            Shape* s = createShapeFromFile(in);
            if (s) {
                loadedShapes.push_back(s);
            }
        }
        in.close();
    }
    cout << "Read " << loadedShapes.size() << " shapes." << endl;

    // Verify: describe all loaded shapes
    cout << "\n=== Loaded Shapes ===" << endl;
    double totalArea = 0;
    for (const Shape* s : loadedShapes) {
        s->describe();
        totalArea += s->getArea();
    }
    cout << "Total area: " << totalArea << endl;

    // Clean up
    for (Shape* s : shapes) delete s;
    for (Shape* s : loadedShapes) delete s;

    // Clean up file
    remove(filename.c_str());
    cout << "\nFile cleaned up." << endl;
}

// ============================================================
// DEMONSTRATION: String streams (in-memory file-like)
// ============================================================
void demonstrateStringStream() {
    cout << "\n=== String Stream I/O ===" << endl;

    // Same principle as file streams, but in a string
    stringstream ss;

    // Write to string stream
    Rectangle r(10.0, 5.0, "purple");
    ss << r.type() << endl;
    ss << r.getArea() << endl;
    ss << r.getPerimeter() << endl;

    // Read from string stream
    string type, area, perimeter;
    getline(ss, type);
    getline(ss, area);
    getline(ss, perimeter);

    cout << "Serialized Rectangle:" << endl;
    cout << "  Type: " << type << endl;
    cout << "  Area: " << area << endl;
    cout << "  Perimeter: " << perimeter << endl;
}

// ============================================================
// MAIN
// ============================================================
int main() {
    cout << "C++ Practice: File I/O + Polymorphism" << endl;
    cout << "=====================================" << endl;
    cout << endl;

    demonstrateFileIO();
    demonstrateStringStream();

    cout << "\n=====================================" << endl;
    cout << "Key concepts covered today:" << endl;
    cout << "  - ifstream / ofstream file operations" << endl;
    cout << "  - Polymorphic serialization (writeToFile)" << endl;
    cout << "  - Factory pattern for deserialization" << endl;
    cout << "  - Type identification for object reconstruction" << endl;
    cout << "  - stringstream for in-memory I/O" << endl;
    cout << "  - File cleanup (remove())" << endl;
    cout << "  - Why: persist complex object hierarchies" << endl;

    return 0;
}
