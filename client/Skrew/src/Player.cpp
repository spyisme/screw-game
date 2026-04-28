#include "../include/Player.h"

Player::Player(const std::string& playerName)
{
    name = playerName;
}

std::string Player::getName() const
{
    return name;
}

// Fixed: Signature now matches the header exactly
void Player::setName(const std::string& newName)
{
    name = newName;
}

void Player::addCard(const Card& card)
{
    hand.push_back(card);
}

void Player::clearHand()
{
    hand.clear();
}

const std::vector<Card>& Player::getHand() const
{
    return hand;
}