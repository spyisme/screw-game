#pragma once
#include <vector>
#include "Card.h"

class Deck
{
private:
    std::vector<Card> deckArray;

public:
    Deck();

    void createDeck();
    void shuffle();

    size_t size() const {
        return deckArray.size(); 
    }
    Card drawCard();
};