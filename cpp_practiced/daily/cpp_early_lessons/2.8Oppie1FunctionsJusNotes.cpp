#include<iostream>
using namespace std;



//This is a function prototype. It is necessary to place this at top of main as computers read from top down.  The definition/answer to this
//function is below the main function main(){ main program  }  Definition here (just below main). Create a void prototype function that
//prints something to the screen:
//Code:
void printToScreenFunction();


int main() {

	//This is a function call. It is just code that says execute the function and show result of whatever 
	//is in the code block of the function (here just a cout/print to screen statement.
	//Code:
	printToScreenFunction();

	return 0;
}

//This is the "definition" (answer) of the function. When computer see "printToScreenFunction()" in main it follows that 
//instruction and shows the codeblock (what's inside the brackets like code below) -> {    do this computer (here cout statement below)}
//Code:
void printToScreenFunction() {
	cout << "This message is printed from inside the function!" << endl;
}

//void just means not to return anything directly to main() (or other functions, their "caller" but we'll talk about that later :). 
// Main will "call" this function and the function will execute that function
//Again, do whatever is inside curly braces {     }. Think of functions like mini programs that main 
//can ask to perform an a

//Key point: Prototype -> Call -> Definition
