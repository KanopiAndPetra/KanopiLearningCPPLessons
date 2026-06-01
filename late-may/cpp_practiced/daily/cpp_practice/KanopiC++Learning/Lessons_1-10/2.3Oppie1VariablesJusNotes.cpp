#include <iostream>
using namespace std;


//Demonstration of variable initialization and basic arithmetic operations

int main() {

	//Initialization - variable is created AND given its first value
	//Must use "int" data type to set variable (here a) to an integer. Same for
	//int b setting it equal to 3. This is called initialization.
	//Also, create variables of same type named "sum" and "difference that will be used later in program

	//Initialization code:
	int a = 2;
	int b = 3;
	int sum;
	int difference;
	

	//Demonstration on of assignment "operator"
	//The data type "int" must match the variables (sum -> (+)) data type. 
	//Here we must do "sum" and "difference" since we are doing a different operation (-) to assign.

	//sum code
	//Here do sum operation:
	sum = a + b;
	
	//difference code
	//Here do a difference operation:
	difference = a - b;
	

	//Here we see the information (a, b and sum/difference) (2 lines of code) to the screen. Code:
	cout << "a = " << a << endl;
	cout << "b = " << b << endl;
	cout << "sum = a + b = " << sum << endl;
	cout << "difference = a - b = " << difference << endl;
	

	//Here we see (a, b and sum). Code:
	// (already shown above - this might be redundant output or separate example)
	

	//Explanatory Code to the screen:
	cout << endl;
	cout << "a and b were initialized to 2 and 3." << endl;
	cout << "sum and difference were assigned after the fact." << endl;
	

	//Assignment Code:
	a = 10;
	b = 4;
	

	//After assignment new variable assignment code -> expression code:
	sum = a + b;
	difference = a - b;
	

	//Here we see the information (a, b and sum/difference) to the screen (2 lines of code). Code:
	cout << endl;
	cout << "After reassignment:" << endl;
	cout << "a = " << a << endl;
	cout << "b = " << b << endl;
	cout << "sum = a + b = " << sum << endl;
	cout << "difference = a - b = " << difference << endl;

	return 0;

}

//Additional Points:
//int a; -> Declaration only (a exists but has a garbage value)
//a = 4; -> Assignment - giving a value to an EXISTING variable (here int a)
//Also, int b = 3; -> initialization.
//b = 10; -> Assignment - changing b's value
