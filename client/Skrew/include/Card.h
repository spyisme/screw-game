#pragma once
// Type of card
enum class CardType {
    Number,
    GiveAndTake,
    SeeAndSwab
};



// Represents a single card
struct Card {
    CardType type;
    int value;  // Used only for number cards

    // Constructor for number cards
    Card(int v) : type(CardType::Number), value(v) {}

    // Constructor for special cards
    Card(CardType t) : type(t), value(0) {}
};