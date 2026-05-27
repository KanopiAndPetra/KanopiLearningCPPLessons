#include<iostream>
using namespace std;


int main() {

	//Relational operators are <  and >
	// "=" is an assignment operator
	//Relational or Comparison operational operators == (equals) and != (does not equal)
	//The whole if statement is called a "control flow statement"
	//The part inside() is called the "condition"
	//The part inside {} is called the  code block.
	//An if statement has a condition() and a code block in {}

	//Declare and initialize two int variables which will be used at the end of the program
	//Code:
	int a = 5;
	int b = 8;

	//Create if statement using the "<" less than operator with a cout statement in the code block if "if" statement is true.
	//Code:
	if (a < b) {
		cout << "a is less than b" << endl;
	}

	//Create if statement using the ">=" less than operator with a cout message in code block because statement is true.
	//Code:
	if (a >= b) {
		cout << "a is greater than or equal to b" << endl;
	}

	//Create an if statement that is false with message in code block that will not print out because if is not true.
	//definition will not show to screen. See bonus at bottom.
	//Code:
	if (a > b) {
		cout << "This will NOT appear because a is not greater than b" << endl;
	}

	//Create a this value "equals" that value (==) 
	//Code:
	if (a == b) {
		cout << "a equals b" << endl;
	}

	//Create an if condition using the != (not equal) operator with literal numbers.
	//Code:
	if (10 != 3) {
		cout << "10 does not equal 3" << endl;
	}

	//Create an if condition using the == (equals) operator with literal numbers.
	//Code:
	if (7 == 7) {
		cout << "7 equals 7" << endl;
	}

	//Use the variables from the top of the program (int a and int b -> so just a and b) which will be true and print them out
	//to the screen.
	//Code:
	if (a < b) {
		cout << "Using variables: a=" << a << " and b=" << b << " — a < b is true" << endl;
	}
}

//Once you have run the program to see which information is displayed adjust the if statements if(adjust condition here) and 
//rerun program how what happens to the output of each statement.
//Hint: The third if code block cout statement is missing! Lets see if you can make it appear in the output in your adjusted code :)
//Bonus: See if you can make #4 not appear in output to screen.
