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
	deckArray.reserve(66);

    // -1–10 (4 copies each 11 card) 44 card
    for (int value = -1; value <= 10; value++)
    {
        for (int i = 0; i < 4; i++)
        {
            deckArray.push_back(Card(value));
        }
    }

    // 20 (6 copies)
    for (int i = 0; i < 6; i++)
        deckArray.push_back(Card(20));

    // 25 (6 copies)
    for (int i = 0; i < 4; i++)
        deckArray.push_back(Card(25));
     
	// 66 - 56 = 10 cards left for special cards 

    // Basra (4 copies)
    for (int i = 0; i < 4; i++)
        deckArray.push_back(Card(CardType::Basra));

    // GiveAndTake (4 copies)
    for (int i = 0; i < 4; i++)
        deckArray.push_back(Card(CardType::GiveAndTake));

    // SeeAndSwab (4 copies)
    for (int i = 0; i < 4; i++)
        deckArray.push_back(Card(CardType::SeeAndSwab));

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