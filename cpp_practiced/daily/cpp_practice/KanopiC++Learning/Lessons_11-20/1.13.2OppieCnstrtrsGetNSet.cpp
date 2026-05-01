#include <iostream>
#include <string>
using namespace std;


//This tutorial focuses on using getters and setters to initialize constructors and access data in the private access specifier (private:)
//We can only private: data within the class. Main can't just call a a value contained in a variable (local storage) that is in private.
//We must use getters and setters which is logic (not keywords) to first set the information from the user input from the arguments and
//use its matching constructor/function(with parameter) located in the OppieClass1 and "set"" what is in that private variable. Then we will use
// the "get" logic to access the private variable (here parameter sentence) and show it to screen. It also demonstrates that functions can
//call other functions within the class. In this example a constructor "calls" another function.

//"string" is a variable = sentence 

//Create a class that will hold the constructor, getter and setter member functions as well as a member variable to hold a sentence.
//CODE:
class OppieClass1 {
//CODE - Create a public access specifier:
public:
    //CODE - Create a constructor that will do two things at once when called. 1. Create an object and 
    OppieClass1(string z) {
        //We will use this constructor that matches the object. (In-> main(){OppieClass1 oppieObject1(argument)
        //This takes that information and calls a function within this class. Again, when we say "call" the computer
        //is asking the other function to execute. It takes the information gained in main(){ oppieObject1("Example of constructor...")
        //(z is just a placeholder for that sentence when it calls to the void function below. Its just the sentence we just got from main
        setParameterSentence(z);
    }

    //Setter -> The setter function assigns the value into the private member parameterSentence.
    //Create a void set function with a parameter that takes the string and then set that string to a member variable.
    //CODE:
    void setParameterSentence(string x) {
        //This executes because it was called from the constructor above. 
        //All this function does is take "x" (could be y, z, a ect.) (sentence from argument in main and store it in private: string parameterSentence0
        parameterSentence = x;
    }

    //Create a getter string function that's only function is to retrieve the member variable that holds the sentence.
    //CODE:
    string getParameterSentence() {
        //Its sole purpose is to return "parameterSentence" from the class (stored in local variable in private) to main call
        //via oppieObject1.getParameterSentence(); -> the "answer" to a function -> here getParameterSentence() IS the return value from the function
        //that is executed. //This function is directly called from main as it exists in the public: portion of this class (OppieClass1)
        return parameterSentence;
    }

//CODE - Create a private access specifier.
private:
    //CODE - Create a string member variable to hold the sentence from the constructor call in main.
    string parameterSentence;

    //Local private variable that can ONLY be accessed within the class it belongs to
};


int main()
{

    cout << "\n-----  getters and setters along with constructors------\n\n";

    //Create an object/constructor of/from the class you created that holds a sentence you create.
    //CODE:
    OppieClass1 oppieObject1("Constructor, setter, and getter working together!");

    //This "call" sends info to class constructor and stores in the private variable you created.

    //Now call on that object using the getter function that and show the sentence out to the screen.
    //CODE:
    cout << oppieObject1.getParameterSentence() << endl;

    //This "calls" the getParameterFunction() which executes and returns the sentence stored in the private:
    //access specifier. "return" parameterSentence = getParameter Sentence

    cout << "\n\n-------And the second constructor-------\n" << endl;

    //Now repeat using a second object and constructor with a different sentence.
    //CODE:
    OppieClass1 oppieObject2("Kanopi is crushing C++!");
    cout << oppieObject2.getParameterSentence() << endl;

}

//Key Point: Constructor accepts input -> setter stores it in private data -> getter returns private data back out.
