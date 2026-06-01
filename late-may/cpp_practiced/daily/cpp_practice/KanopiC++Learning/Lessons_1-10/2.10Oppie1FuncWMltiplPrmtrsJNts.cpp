#include <iostream>
using namespace std;


//Here we are just taking it one step further. Rather than just having one parameter as an int we can have as many parameters
//as we want.  The argument just needs to have matching values and data types. Here we will use for parameters and then make an
//expression as the "answer/result" of the function.

//Create prototype addNumbers has 4 int variables (values) as its parameter
//Code:
int addNumbers(int a, int b, int c, int d);


int main() {

	//Make simple cout statment to say what the call returns to screen
	//Code:
	cout << "The sum of all four numbers is: ";
	
	//Create the function call out to screen with 4 integer arguments.
	//This "calls" addNumbers function with matching values as arguments (arguments are numbers in parenthesis)
	//Code:
	cout << addNumbers(10, 15, 28, 7) << endl;
	
	
	
}

//Create a function definition for addNumbers.
//Code:
int addNumbers(int a, int b, int c, int d) {

	// Here we create a new variable "int answer" which is a local variable that will store the result/sum of the expression
	//Code:
	int answer = a + b + c + d;
	
	//This answer is "returned" to the caller which here is the main program.
	//Code:
	return answer;
	
}

//Key takeaway: addNumbers(10,15,28,7) is replaced with the "answer" that is returned in the addNumbers() function outside of main
//so, addNumbers(10,15,28,7) = answer (return answer)
