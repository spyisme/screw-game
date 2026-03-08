#include <iostream>
#include <vector>
#include <string>
#include <optional>
#include <SFML/Graphics.hpp>
#include "../include/Deck.h"
#include "../include/Player.h"

const sf::Vector2f CARD_SIZE = { 130.f, 180.f };

std::string cardToFileName(const Card& card) {
    if (card.type == CardType::Number) return std::to_string(card.value);
    if (card.type == CardType::Basra) return "Basra";
    if (card.type == CardType::GiveAndTake) return "5odwHat";
    if (card.type == CardType::SeeAndSwab) return "Seeandswab";
    return "Unknown";
}

void setCardTexture(sf::RectangleShape& box, sf::Texture& tex, const std::string& name) {
    std::string path = "assets/cards/" + name + ".png";
    if (tex.loadFromFile(path)) {
        box.setTexture(&tex);
        box.setFillColor(sf::Color::White);
    }
    else {
        box.setTexture(nullptr);
        box.setFillColor(sf::Color::Red);
    }
}

int main() {
    sf::RenderWindow window(sf::VideoMode({ 1100, 750 }), "Skrew - Initial Peek");
    sf::Font font;
    if (!font.openFromFile("assets/fonts/arial.ttf")) return -1;

    bool inMenu = true;
    std::string p1Input = "";

    sf::Texture backTex, drawnTex, floorTex;
    if (!backTex.loadFromFile("assets/cards/back.png")) return -1;
    std::vector<sf::Texture> handTextures(4);

    Deck deck;
    Player player1("P1");
    std::optional<Card> currentDrawnCard;
    std::optional<Card> lastFloorCard;
    bool drawnFromFloor = false;

    // UI Elements
    sf::Text menuText(font, "Enter Your Name:", 40);
    menuText.setPosition({ 350.f, 250.f });
    sf::Text nameDisplay(font, "_", 40);
    nameDisplay.setPosition({ 350.f, 320.f });
    nameDisplay.setFillColor(sf::Color::Yellow);

    sf::Text statusText(font, "", 22);
    statusText.setPosition({ 300.f, 380.f });
    statusText.setFillColor(sf::Color::White);

    std::vector<sf::RectangleShape> deckStack;
    for (int i = 0; i < 3; i++) {
        sf::RectangleShape s(CARD_SIZE);
        s.setTexture(&backTex);
        s.setPosition({ 850.f - (i * 4), 100.f - (i * 4) });
        deckStack.push_back(s);
    }

    sf::RectangleShape floorBox(CARD_SIZE);
    floorBox.setPosition({ 600.f, 100.f });
    floorBox.setOutlineThickness(2.f);
    floorBox.setOutlineColor(sf::Color(100, 100, 100));
    floorBox.setFillColor(sf::Color(50, 50, 50));

    sf::RectangleShape drawnBox(CARD_SIZE);
    drawnBox.setPosition({ 350.f, 100.f });
    drawnBox.setFillColor(sf::Color::Transparent);

    std::vector<sf::RectangleShape> handBoxes;
    for (int i = 0; i < 4; i++) {
        sf::RectangleShape b(CARD_SIZE);
        b.setPosition({ 230.f + i * 160.f, 500.f });
        b.setTexture(&backTex); // Default to face down
        handBoxes.push_back(b);
    }

    while (window.isOpen()) {
        while (const std::optional event = window.pollEvent()) {
            if (event->is<sf::Event::Closed>()) window.close();

            if (inMenu) {
                if (const auto* textEvent = event->getIf<sf::Event::TextEntered>()) {
                    if (textEvent->unicode == '\b' && !p1Input.empty()) p1Input.pop_back();
                    else if (textEvent->unicode == '\r' || textEvent->unicode == '\n') {
                        if (!p1Input.empty()) {
                            player1.setName(p1Input);

                            // DEALING LOGIC
                            for (int i = 0; i < 4; i++) {
                                player1.addCard(deck.drawCard());

                                // NEW: Make first 2 cards visible, last 2 hidden
                                if (i < 2) {
                                    setCardTexture(handBoxes[i], handTextures[i], cardToFileName(player1.getHand()[i]));
                                }
                                else {
                                    handBoxes[i].setTexture(&backTex);
                                }
                            }

                            inMenu = false;
                            statusText.setString("Cards 1 & 2 are revealed! Memorize them.");
                        }
                    }
                    else if (textEvent->unicode < 128) p1Input += static_cast<char>(textEvent->unicode);
                    nameDisplay.setString(p1Input + "_");
                }
            }
            else if (event->is<sf::Event::MouseButtonPressed>()) {
                sf::Vector2f mouse = window.mapPixelToCoords(sf::Mouse::getPosition(window));

                // 1. Draw from Pile
                if (!deckStack.empty() && deckStack.back().getGlobalBounds().contains(mouse) && !currentDrawnCard) {
                    currentDrawnCard = deck.drawCard();
                    drawnFromFloor = false;
                    setCardTexture(drawnBox, drawnTex, cardToFileName(*currentDrawnCard));
                    statusText.setString("Drawn from Pile. Swap or Discard.");
                }

                // 2. Take from Floor
                if (floorBox.getGlobalBounds().contains(mouse) && lastFloorCard.has_value() && !currentDrawnCard) {
                    currentDrawnCard = lastFloorCard;
                    drawnFromFloor = true;
                    lastFloorCard = std::nullopt;
                    floorBox.setTexture(nullptr);
                    floorBox.setFillColor(sf::Color(50, 50, 50));
                    setCardTexture(drawnBox, drawnTex, cardToFileName(*currentDrawnCard));
                    statusText.setString("Took from Floor. You MUST swap.");
                }

                // 3. Swap with Hand
                if (currentDrawnCard.has_value()) {
                    for (int i = 0; i < 4; i++) {
                        if (handBoxes[i].getGlobalBounds().contains(mouse)) {
                            Card oldHandCard = player1.getHand()[i];

                            std::vector<Card>& handRef = const_cast<std::vector<Card>&>(player1.getHand());
                            handRef[i] = *currentDrawnCard;

                            // Update visual: The new card is now visible in your hand
                            setCardTexture(handBoxes[i], handTextures[i], cardToFileName(handRef[i]));

                            // Move old card to floor
                            lastFloorCard = oldHandCard;
                            setCardTexture(floorBox, floorTex, cardToFileName(*lastFloorCard));

                            currentDrawnCard = std::nullopt;
                            drawnBox.setFillColor(sf::Color::Transparent);
                            statusText.setString("Swapped! New card is now visible.");
                        }
                    }

                    // 4. Discard newly drawn card
                    if (!drawnFromFloor && floorBox.getGlobalBounds().contains(mouse)) {
                        lastFloorCard = currentDrawnCard;
                        setCardTexture(floorBox, floorTex, cardToFileName(*lastFloorCard));
                        currentDrawnCard = std::nullopt;
                        drawnBox.setFillColor(sf::Color::Transparent);
                        statusText.setString("Discarded.");
                    }
                }
            }
        }

        window.clear(sf::Color(30, 60, 30));

        if (inMenu) {
            window.draw(menuText);
            window.draw(nameDisplay);
        }
        else {
            if (deck.size() > 0) {
                for (auto& s : deckStack) window.draw(s);
            }
            window.draw(floorBox);
            window.draw(drawnBox);
            for (auto& hb : handBoxes) window.draw(hb);
            window.draw(statusText);

            sf::Text deckCount(font, "Deck: " + std::to_string(deck.size()), 20);
            deckCount.setPosition({ 850, 300 });
            window.draw(deckCount);
        }

        window.display();
    }
    return 0;
}