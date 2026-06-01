#include <iostream>
using namespace std;



//Here we have the function definition above the main() program. Typically (like last tutorial) you want to prototype 
// (just the function -> void printFavoriteNumber(int x) for organizational purpose but I am doing it this way just to 
//demonstrate that this can be done as well.

//Here (int x) just means that the answer/return to the function must be an integer (int). Make void function that prints
//your favorite number to the screen. Then returns control to main.
//Code:
void printFavoriteNumber(int x) {
	cout << "Kanopi's favorite number is: " << x << endl;
}


int main() {

	//So here is call from main. It has an argument "()". It just means that we pass (int) to the prototype
   //A prototype is like a blank form that asks for a specific data type (here int). You can think  of it like this:
   //printFavoriteNumber(). So when when we "call" the function definition outside of main we wll get
   //cout << "Kanopi's favorite number is: " << ? << endl;
	//Code:
	printFavoriteNumber(7);

	return 0;
}
