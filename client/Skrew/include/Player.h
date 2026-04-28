#pragma once

#include <string>
#include <vector>
#include "Card.h"

class Player
{
private:
    std::string name;
    std::vector<Card> hand;

public:
    Player(const std::string& playerName);

    std::string getName() const;


    void setName(const std::string& newName);

    void addCard(const Card& card);

    void clearHand();

    const std::vector<Card>& getHand() const;
};