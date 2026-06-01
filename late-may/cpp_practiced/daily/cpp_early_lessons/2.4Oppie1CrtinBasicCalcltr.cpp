#include <iostream>
using namespace std;


//Demonstrates building a basic calculator

int main()

{//Brace at start of program (main function)

	//Declare int (integer) variables with no value
	//These variables (a,b and sum) are local memory storage. They are "local" in that
	//they are only accessible within main(){     Only accessible between main braces        }. So as user enters values 
	//those values are stored in these variables and can be used later (here to total the sum) 
	//When those two values are added together (like below expression) that answer is stored
	//in "int sum" which will be shown to screen later.

	//Declaration of 2 variables with no value ("garbage"/nothing value). 
	
	//Code:
	int a;

	//Code:
	int b;

	//Declare sum variable as an int data type. 
	
	//Code:
	int sum;
	

	//Make statement that notifies of a basic calculator. 
	//Code:
	cout << "---This is a basic calculator!---" << endl;

		//Ask for a (a) number. 
		// Code:
	cout << "Enter a number: " << endl;

	//Code that assigns a value to 'a' (cin)
	//Code:
	cin >> a;

	//Ask for another number. 
	// Code:
	cout << "Please enter a another number to the screen: " << endl;
	
	//See information into program (b). This assigns a value to b (int b). 
	// Code:
	cin >> b;

	//User enters another value which is assigned to int 'b' variable

	//We assign the result of the expression a + b to sum. 
	//Code:

	sum = a + b;
	
	//Since we already declared "sum" to be an "int" we do not need to put "int" in front here (assignment) 

	//Create a statement that explains operation (cout) shows the sum to the screen. 
	// Code:
	cout << "The sum of a and b is: " << sum << endl;
	//Here we get the value that is stored in int "sum" and out put it to the screen using cout.

	return 0;

}//Brace at end of program (main function)

//Key point: "Garbage" is simply whatever bits (1s and 0s) were left behind from that previous use. Could be anything from
//part of an old calculation, leftover data from another program or even random numbers like 48291 or -872634
