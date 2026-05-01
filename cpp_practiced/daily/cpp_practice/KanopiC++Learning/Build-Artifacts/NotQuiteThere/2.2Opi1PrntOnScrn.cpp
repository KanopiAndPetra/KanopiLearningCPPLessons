#include <iostream>
using namespace std;

//Demonstration another attribute of \n (new line character).
//When you use the \n throughout sentence it puts text on new line.

//Key point: Go ahead and work with the spacings and "\n" in the sentence.
//e.g. Delete the space in front of "My" and then the sentences on the new lines will
//no longer line up. Also, try deleting the "\n" character and see what happens.

int main(){
    //VERSION 1: With \n and proper spacing - everything lines up nicely
    cout << "My name is Arlo" << endl;
    cout << "I live on a Mac Mini" << endl;
    cout << "I love prime numbers" << endl;
    
    cout << endl;
    
    //VERSION 2: What happens if we remove the \n - everything runs together on ONE line!
    cout << "My name is Arlo";
    cout << "I live on a Mac Mini";
    cout << "I love prime numbers";
    
    cout << endl << endl;
    
    //VERSION 3: With LEADING SPACES - shows indentation/alignment
    cout << "  My name is Arlo" << endl;
    cout << "  I live on a Mac Mini" << endl;
    cout << "  I love prime numbers" << endl;
    
    cout << endl;
    
    //VERSION 4: NO leading spaces - no alignment
    cout << "My name is Arlo" << endl;
    cout << "I live on a Mac Mini" << endl;
    cout << "I love prime numbers" << endl;
    
    return 0;
}
