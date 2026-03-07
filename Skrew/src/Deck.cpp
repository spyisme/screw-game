#include "../include/Deck.h"
#include <algorithm>
#include <random>

Deck::Deck()
{
    createDeck();
    shuffle();
}

void Deck::createDeck()
{
    deckArray.clear();

    // 0–10 (4 copies each)
    for (int value = 0; value <= 10; value++)
    {
        for (int i = 0; i < 4; i++)
        {
            deckArray.push_back(Card(value));
        }
    }

    // 20 (8 copies)
    for (int i = 0; i < 8; i++)
        deckArray.push_back(Card(20));

    // 25 (8 copies)
    for (int i = 0; i < 8; i++)
        deckArray.push_back(Card(25));

    // Basra (3 copies)
    for (int i = 0; i < 3; i++)
        deckArray.push_back(Card(CardType::Basra));

    // GiveAndTake (3 copies)
    for (int i = 0; i < 3; i++)
        deckArray.push_back(Card(CardType::GiveAndTake));
}

void Deck::shuffle()
{
    std::random_device rd;
    std::mt19937 rng(rd());
    std::shuffle(deckArray.begin(), deckArray.end(), rng);
}

Card Deck::drawCard()
{
    Card card = deckArray.back();
    deckArray.pop_back();
    return card;
}