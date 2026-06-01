#include <iostream>
#include <string>
using namespace std;



//Below are two examples of using a constructor to initialize members with multiple pieces of data at once.
//This allows us to reduce code in that we dont have to list each objects attributes on another line like this:


//Create a class.
//Code:
class OppieClass1 {
//Create a public access specifier.
//Code:
public:
    //The following are local storage variables to store info from the object. We first get he information from user which stores here
    //Then we retrieve this information oppie1Object and "." dot separator. Basically what we are saying is that oppie1Object + name
    //When we say "local" variables we are saying local to the program. Its storage space within a given function/program/class.

    //Create a string variable that holds/stores a name.
    //Code:
    string name;

    //Create an int variable that will hold an age.
    //Code:
    int age;

    //Create a variable that will hold/store grade point average (decimal).
    //Code:
    double gpa;

    //This constructor "template" has matching information and structure within main()
        //e.g. OppieClass1 oppieObject1("Claude", 33, 3.7) -> That information goes from there into each variable in (      )
        //programmer puts the actual information in main(). 
    //Create a constructor with string, int and double as its parameter (a, b, c) which will set the values to variables
    //where that information will be stored.
    //Code:
    OppieClass1(string a, int b, double c) {
        name = a;
        age = b;
        gpa = c;
    }

        //That information is stored in each variable and made an attribute within the class (stored in local variables)
        //This is the body of the function. The functions sole purpose is to take the data and put it into each storage box/variable. 


    //This is just a standard function with a void type. So main() tells it to do something and it performs that action outside 
    //main function then the program goes back to main and goes to next step.

    //Create a void function that's purpose is just to show out to screen that Claude is studying when called from main.
    //Code:
    void study() {
        cout << name << " is studying!" << endl;
    }
};


//Another example. EXACT same concepts as above.
//You can have multiple classes outside of the main program. Here is another example.

//Create a car class
//Code:
class CarClass {
//Create a public access specifier
//Code:
public:
    //Create variables (attributes) that will store information from the constructor (age, make, model).
    //Code:
    int year;
    string make;
    string model;

    //Create a CarClass constructor with parameters and set those parameters equal to the variables you just made.
    //Code:
    CarClass(int a, string b, string c) {
        year = a;
        make = b;
        model = c;
    }

    //Create a void function that does something. Like prints out "Car hits the breaks" to the screen. 
    //Code:
    void brakes() {
        cout << "Car hits the brakes!" << endl;
    }
};


int main() {

    //This is the main program. Understand that it is connected to the classes outside the program. It is actually a function as well.
    //When I say it "calls" functions in the other classes just understand that the functions are like mini-programs the are being
    //directed to do thing from here in main.

    cout << "\nExample of using a 'public:' access specifier to demonstrate constructor which is doing two things at once:\n";
    cout << "1. Create box/blank form. 2. put some default info in it!\n\n";

    cout << "\n-----Example Output from constructors -> Creates object and default information. e.g. object(info in object)----" << endl;
    cout << "\nExample 1 (OppieClass oppieObject1(name, age, gpa):\n";
    //Below we are saying use OppieClass1 (blueprint) to build oppieObject1 -> (with info pre filled)-> oppieClass1(name, age, gpa)

    //Create a Oppie object of the OppieClass with parameters "Claude", 33, 3.7"
    //Code:
    OppieClass1 oppieObject1("Claude", 33, 3.7);

    //Use that object to show name, age and gpa to screen
    //Code:
    cout << "Name: " << oppieObject1.name << endl;

    //Code:
    cout << "Age: " << oppieObject1.age << endl;

    //Code:
    cout << "GPA: " << oppieObject1.gpa << endl;

    //Create an object and call the "study" function which will then print to screen.
    //Code:
    oppieObject1.study();




    //Make another Oppie object named oppieObject2 of the OppieClass and perform the same steps as the oppieObject above
    //Code:
    OppieClass1 oppieObject2("Kanopi", 1, 4.0);

    //Code:
    cout << "Name: " << oppieObject2.name << endl;

    //Code:
    cout << "Age: " << oppieObject2.age << endl;

    //Code:
    cout << "GPA: " << oppieObject2.gpa << endl;

    //Code:
    oppieObject2.study();


    //Another example just to show you can have main drawing from multiple classes. EXACT same concepts as above.
    //The only difference is that you will be making an object (perhaps named carObject) of the CarClass.

    //Code:
    CarClass carObject(2020, "Toyota", "Camry");

    //Code:
    cout << "Car Year: " << carObject.year << endl;

    //Code:
    cout << "Car Make: " << carObject.make << endl;

    //Code:
    cout << "Car Model: " << carObject.model << endl;

    //Code:
    carObject.brakes();


}
