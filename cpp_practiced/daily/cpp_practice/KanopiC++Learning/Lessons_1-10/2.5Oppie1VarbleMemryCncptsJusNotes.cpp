#include<iostream>
using namespace std;


int main()
{
	//Here we set the int variable tuna to 99 (declaration/initialization).
	//Declaration - Names the variable & sets its type (int tuna)
	//Initialization - Gives the variables first value (=99)
	//Code:
	int tuna = 99;
	//This value is stored in memory.

	//Make a statement that shows to screen showing what the variable's value is to the screen.
	//Code:
	cout << "tuna = " << tuna << endl;
	

	//That variable can be overwritten like this. We also dont need to put data type
	//in front of variable as it has already been declared an int previously in program.
	//if you were to put the data type in front again (like this int tuna) you would get an error
	//(redeclaration/multiple initialization) and program will not run/execute
	//Assign that variable to a new value '76' to demonstrate the assignment to screen.
	//Code:
	tuna = 76;
	
	
	//Make a cout statement that displays to screen that explains that we can take the initialized value (here 99)
	//and change is through assignment and how that works (int not needed just variable = 76)
	//Code:
	cout << "tuna was initialized to 99, but now it's been reassigned to: " << tuna << endl;

	return 0;
}
