#include <iostream>
#include<string>
using namespace std;


//Access Specifier: "Compartments" in class that provide access (or no access) to activities/objects in main
//"public:" -> anything object in main can access whatever is in this compartment.
//"private:"-> These members (variables here) can only be accessed from functions within the class it comes
//from. Main cannot just "call" the variable (here welcomeToCodeRocks). Will get error.

//Create a class 
//Code:
class AdamClass {
//Code - Create a public access specifier:
public:
    //Create a setter function (setName()) that takes a string parameter x. Then set that value x and set it equal to a variable that will 
    //store information from main. Make it descriptive like "welcomeStatement". It should be a void that can be called on, 
    //does action outside main, then returns control to main.
    //Code:
    void setName(string x) {
        welcomeStatement = x;
    }

    //In main you will see something that "matches" this function to be filled in
    //Here we take that value that is established in main and set it equal to a variable
        //This variable acts as local storage for a sentence/strings. Here we will store the value in
        //"private". Which is just a place that keeps the information "safe" in that it can only be 
        //accessed within the class. main() cannot access this directly. I must work through another
        //function and "get" the information from private using the string getName() function we made
        //below. Programmers typically do it this way to keep an orderly flow of information. If another
        //programmer on team is working with the code in main it just controls their access to the private
        //variables. Makes code flow more orderly since the variables(attributes) may be used in other 
        //parts of a given program and you dont want to change the actual value as it may cause other 
        //issues elsewhere. 

    //Now create a getName() string function that will return a value (here whatever is stored in "welcomeStatement" 
    //back to main when called.
    //Code:
    string getName() {
        return welcomeStatement;
    }

        //here we "get" return sentence stored in "private" to main() -> cout
        //to screen from adamsObject1 -> 'welcomeToCodeRocks!' This function purpose is to access the
        //information/here a sentence that is in private and to send it back to caller/main()
        //Important multiple objects can use this variable. When you make an object in main it is an
        //blank/empty form/box for each object -> see adamsObject2 example after adamsObject1
        //So this sentence can change accordingly

//Create a private access specifier.
//Code:
private:


    //This can only be accessed within this class "AdamClass". So main "calls" the getName() function
    //Which then runs within the class to get whatever sentence is currently stored in the variable
    //below.  Think of a variable as storage made in each object that it can use whenever but only for
    //each specific object. each object is a unique instance of the class. Again, the object is just
    //an empty form/template which contains data, functions/actions it can use. 

    //Create the string variable ("welcomeStatement") that will store sentence/string from main.
    //Code:
    string welcomeStatement;
};

int main() {

    //We start with AdamClass to tell computer what template the object is being built from
    //AdamsClass -> adamsObject. adamsObject is built from the schematic/architects drawings from AdamsClass
    //So here our first empty box/form is created. It can draw from "AdamClass"

    //Create an object(object1) of the AdamClass.
    //Code:
    AdamClass object1;

    //Now call on that object a welcome statement/sentence/string using the setName() function with argument  which will store in the 
    //welcomeStatement variable.
    //Code:
    object1.setName("Welcome to CodeRocks!");

    //Now use a cout statement and the object1 you created to retrieve the sentence stored in the string via the getName() function.
    //calls the getName() function and show to screen.
    //Code:
    cout << object1.getName() << endl;

    //This just shows that we can create as many objects from "AdamsClass" templates as we want.
    //Can think of it like a apartment complex with each apartment being built from same footprint
    //Create a second object (object2) and display another statement/sentences/string to screen.
    //Code:
    AdamClass object2;
    object2.setName("Welcome to the jungle, Kanopi!");
    cout << object2.getName() << endl;

}
