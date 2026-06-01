#include<iostream>
using namespace std;



int main() {

	//Here we are initializing variables though expressions. Remember whenever you want to initialize
	//a variable the variable you want to initialize comes first -> data type variable = value or expression

	//Demonstrates modulus (%) operator. Just remainder of division (25/7=3 with 4 remainder).
	//Code:
	int a = 25 % 7;

	//Regular division so 7 is output
	//Code:
	int b = 28 / 7;

	//Addition
	//Code:
	int c = 3 + 4;
	
	//Subtraction
	//Code:
	int d = 7 - 2;

	//Now demonstrate order of operations (PEDMAS) using two different expressions and their results.
	//Code:
	int e = 3 * (4 + 1);

	//Code:
	int f = 3 * 4 + 1;

	//Now explain and see all of the above variables out to screen using cout statements as well as the variables
	//you just declared and initialized to value.

	cout << "Modulus Operator:" << endl;
	cout << "The modulus '%' operator takes divisor divided by numerator and returns the remainder of that output." << endl;
	cout << "So the modulus '%' of 28/7 = " << a << "\n" << endl;

	cout << "Regular division" << endl;
	cout << "So in regular division: 28/7 = " << b << "\n" << endl;

	cout << "Addition" << endl;
	cout << "So in regular division: 3+4 = " << c << "\n" << endl;

	cout << "Subtraction" << endl;
	cout << "So in regular division: 7 + 2 = " << d << "\n" << endl;

	cout << "First example of order of operations" << endl;
	cout << "3 (4 + 1) = " << e << "\n" << endl;

	cout << "Second example of order of operations" << endl;
	cout << "3 * 4 + 1 = " << f << "\n" << endl;

	return 0;
}
