#include <iostream>
using namespace std;


//char -> char holds a single character ('a', 'B', '7') not text.
// string -> Deals with "stringing" characters together
// int x; -> Deals with integer values
//double -> Deals with using decimals e.g. 3.1415926535...


	//This class is like a template/form for each object.  An object is like a blank
	//piece of paper/box/square etc. with nothing in it or on it. "object1" (below in main()) is an
	//instance of the class. We then use the class with a dot "." separator to class tell the computer what
	//to pull from the/what to do. Here we are using a function from the class (outside of main). "void"
	//is just stating that main is calling/telling the function "helloObject" to run and that's it. It 
	//It "returns" nothing to main. The action happens outside the main() program in a the function.
	//It runs outside of main() then control is returned to main() to do the next thing/step in the program.
	//Here we use a function but another common thing to pull/call from classes are variables. 
	//eg. int (integer, string (characters), double (decimal), char (single character) values.

//Code - Make a class
class AdamClass {
//Code - Make class access specifier
public:
    //Code - Make function that prints a statement to the screen and returns nothing to main. Main "calls" it. The function runs outside of main()
    //then control is returned to main.
    void helloObject() {
        cout << "Hello, welcome to Computer Science Class/Objects basics." << endl;
    }

    //Code - Create a variable within the class that will hold a name (supply your own name)
    string name;
};



	//Functions deal with actions to be performed. Here: helloObject();
	//Variables are attributes within class. Here: name
	//Must use " text here  " when seeing (cout) text to screen.
	//Order of does not matter. More for organizational/readability of code.  Here in main() we create
	//the object that runs the function (void helloObject()) -> see main() below and so I choose to put
	//it first. Feel free to put "string = yourName" where ever you like.


	

	//Here we bring in a name name. Please go ahead and type your name here and run
	//the program to see what happens.
	//Create a variable within the class that will hold a name (supply your own name)

	//Code:


int main() {

	//Here we create an object (object1) from the AdamClass
	//Code:
	AdamClass object1;

	//We are telling the computer to use object to "tell the function outside the
	//main() what to do. Here we use a "." dot separator. Think of it like a "+" sign. 
	//object1(blank form) + helloObject(); - " Hello, welcome to Computer..."
	
	//Then we use that object to call the function we created outside of main
	//Code:
	object1.helloObject();

	//Create an object (object2) from AdamClass
	//Code:
	AdamClass object2;

	//Now use that object to call the attribute (variable) "name" into main() from
	//Code:
	object2.name = "Adam";
	cout << "My name is " << object2.name << "." << endl;

	//The output should be one clean sentence -> "Hello, welcome to computer science class/objects basics. My name is Adam." 

	cout << "\n-----------Bonus----------\n" << endl;
	cout << "You can create an object that  does two things at once:" << endl;

	//An object (here object3) can hold two pieces of information that can be used. Write code that creates an object,
	//Uses that SAME object to call the helloObject() function and shows the name to the screen.
	//Code:
	AdamClass object3;
	object3.name = "Kanopi";
	object3.helloObject();
	cout << "My name is " << object3.name << "." << endl;

	return 0;

}

//Key point: Think of objects as boxes/forms you are creating within main() that each do something
//different in your main() program. Programmers often have to think modularly while building programs
//like stacking different shape rocks/blocks/boxes.
