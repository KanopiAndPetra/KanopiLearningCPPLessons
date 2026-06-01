// cpp-practice-2026-04-18.cpp
// C++ Practice: Building a Deck of Cards with Vectors and Classes
// Combines concepts from: Vectors repo (1.74ClaudeExtraCredVectors) and Random Numbers (1.26Oppie1RandomNumGen)

#include <iostream>
#include <vector>
#include <cstdlib>   // For rand()
#include <ctime>     // For time() - seed for random
#include <string>
using namespace std;

// ============================================================
// Card Struct
// Represents a single playing card with a suit and rank.
// Using a struct here since it's simple data container.
// ============================================================
struct Card {
    string suit;
    string rank;
};

// ============================================================
// Deck Class
// Manages a collection of cards using std::vector.
// std::vector is ideal here because the deck size is known (52)
// but we can add/remove cards dynamically during dealing.
// ============================================================
class Deck {
private:
    vector<Card> cards;  // Vector to hold all cards

public:
    // Constructor: Creates a standard 52-card deck
    Deck() {
        // Arrays of suits and ranks - perfect for looping!
        string suits[] = {"Hearts", "Diamonds", "Clubs", "Spades"};
        string ranks[] = {"2", "3", "4", "5", "6", "7", "8", "9", "10", "Jack", "Queen", "King", "Ace"};

        // Nested loops: 4 suits × 13 ranks = 52 cards
        for (string suit : suits) {
            for (string rank : ranks) {
                Card c;
                c.suit = suit;
                c.rank = rank;
                cards.push_back(c);  // push_back() adds to end of vector
            }
        }
    }

    // Shuffle the deck using Fisher-Yates algorithm
    // This is the standard way to shuffle - unbiased and efficient
    void shuffle() {
        srand(time(0));  // Seed random with current time

        // Start from the last element and work backwards
        // This ensures every position has equal chance of getting any card
        for (int i = cards.size() - 1; i > 0; i--) {
            // Generate random index from 0 to i (inclusive)
            int j = rand() % (i + 1);
            // Swap cards[i] and cards[j]
            Card temp = cards[i];
            cards[i] = cards[j];
            cards[j] = temp;
        }
    }

    // Deal one card from the top of the deck
    // Returns an empty Card if deck is empty
    Card deal() {
        if (cards.empty()) {
            cout << "Deck is empty! Cannot deal." << endl;
            return {"", ""};
        }
        // Take the last card and remove it using pop_back()
        Card dealt = cards.back();
        cards.pop_back();  // Remove last element
        return dealt;
    }

    // Print all cards in the deck using RANGE-BASED FOR LOOP
    // This syntax (for type var : collection) iterates through all elements
    // The & makes it a REFERENCE so we can see actual values (not a copy)
    void printDeck(string message) {
        cout << "\n" << message << endl;
        cout << "Deck has " << cards.size() << " cards:" << endl;
        cout << "----------------------------" << endl;

        int count = 1;
        for (Card& c : cards) {  // Use & to avoid copying Card objects
            cout << count << ". " << c.rank << " of " << c.suit << endl;
            count++;
        }
    }

    // Get current deck size
    int size() {
        return cards.size();
    }
};

// ============================================================
// Main Function
// Demonstrates the Deck class with various vector operations
// ============================================================
int main() {
    cout << "========================================" << endl;
    cout << "  C++ Card Deck Simulator" << endl;
    cout << "  Concepts: Vectors + Classes + Random" << endl;
    cout << "========================================" << endl;

    // Create a new deck
    Deck myDeck;

    // Show initial deck size (should be 52)
    cout << "\nCreated a new deck with " << myDeck.size() << " cards." << endl;

    // Print first few cards before shuffling
    cout << "\n--- BEFORE SHUFFLING (first 5 cards) ---" << endl;
    for (int i = 0; i < 5; i++) {
        Card c = myDeck.deal();
        cout << i + 1 << ". " << c.rank << " of " << c.suit << endl;
    }

    // Recreate deck for full demo
    Deck fullDeck;

    // Shuffle the deck (this randomizes card order)
    fullDeck.shuffle();
    cout << "\n--- AFTER SHUFFLING (first 5 cards) ---" << endl;
    for (int i = 0; i < 5; i++) {
        Card c = fullDeck.deal();
        cout << i + 1 << ". " << c.rank << " of " << c.suit << endl;
    }

    // Demonstrate dealing a hand of 5 cards
    cout << "\n--- DEALING A 5-CARD HAND ---" << endl;
    Deck handDeck;
    handDeck.shuffle();
    vector<Card> hand;  // Vector to hold player's hand

    for (int i = 0; i < 5; i++) {
        hand.push_back(handDeck.deal());  // Add dealt card to hand vector
    }

    cout << "Your hand:" << endl;
    for (int i = 0; i < hand.size(); i++) {
        cout << i + 1 << ". " << hand[i].rank << " of " << hand[i].suit << endl;
    }

    cout << "\n--- REMOVING LAST CARD (pop_back demo) ---" << endl;
    cout << "Hand size before: " << hand.size() << endl;
    hand.pop_back();  // Remove last card
    cout << "Hand size after: " << hand.size() << endl;

    cout << "\n--- FINAL HAND ---" << endl;
    for (Card& c : hand) {  // Range-based loop with reference
        cout << c.rank << " of " << c.suit << endl;
    }

    cout << "\n========================================" << endl;
    cout << "  Program completed successfully!" << endl;
    cout << "========================================" << endl;

    return 0;
}