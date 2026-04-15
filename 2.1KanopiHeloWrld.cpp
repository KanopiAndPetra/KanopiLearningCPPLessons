#include<iostream>
using namespace std;

//Hello world program. Demonstrates printing to screen

int main(){
    //Standard way to print info to screen in C++. Use endl; to go to new line
    cout << "Hello World" << endl;
    
    //Can also use \n to go to new line
    cout << "Line 1\nLine 2\n";
    
    //Can also just keep things on one line like this
    cout << "Hello " << "World";
    
    cout << endl;
    
    //This means your program finished with "0" errors. Succesful program.
    return 0;
}


//If you want to test a codeblock inside your main program (which is like a all encompassing code block
//with smaller code blocks inside) you can "return 1" in the body of main() {test other codeblocks
// here with return 1}. Always use "return 0" at end of main - if the program runs then you know you have 
//"0" errors!
