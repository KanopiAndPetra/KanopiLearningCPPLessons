#include <iostream>
using namespace std;


//Demonstration another attribute of \n (new line character).
//When you use the \n throughout sentence it puts text on new line.


//Key point: Go ahead and work with the spacings and "\n" in the sentence.
//e.g. Delete the space in front of "My" and they the sentences on the new lines will
//no longer line up. Also, try deleteing the "\n" character and see what happens.

int main(){
    //Using \n throughout sentence puts text on new line
    cout << "My name is Kanopi\n";
    cout << "I live on a Mac Mini\n";
    cout << "I love prime numbers\n";
    
    //Now try deleting the \n character and see what happens
    cout << "My name is Kanopi";
    cout << "I live on a Mac Mini";
    cout << "I love prime numbers";
    
    return 0;
}
