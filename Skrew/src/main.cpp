#include <iostream>
#include <vector>
#include <string>
#include <optional>
#include <SFML/Graphics.hpp>
#include "../include/Deck.h"
#include "../include/Player.h"
#include "../include/MainMenu.h" // Integrated Header

const sf::Vector2f CARD_SIZE = { 130.f, 180.f };

// Game States
enum class AppState {
    Menu,
    Naming,
    Playing
};

sf::Vector2f lerp(sf::Vector2f start, sf::Vector2f end, float t) {
    return start + t * (end - start);
}

std::string cardToFileName(const Card& card) {
    if (card.type == CardType::Number) return std::to_string(card.value);
    if (card.type == CardType::GiveAndTake) return "5odwHat";
    if (card.type == CardType::SeeAndSwab) return "Seeandswab";
    return "Unknown";
}

void setCardTexture(sf::RectangleShape& box, sf::Texture& tex, const std::string& name) {
    std::string path = "assets/cards/" + name + ".png";
    if (tex.loadFromFile(path)) {
        tex.setSmooth(true);
        box.setTexture(&tex, true);
        box.setFillColor(sf::Color::White);
    }
    else {
        box.setTexture(nullptr);
        box.setFillColor(sf::Color::Red);
    }
}

int main() {
    sf::RenderWindow window(sf::VideoMode({ 1100, 750 }), "Skrew");
    sf::Font font;
    if (!font.openFromFile("assets/fonts/arial.ttf")) return -1;

    // --- SYSTEMS ---
    MainMenu mainMenu(font);
    AppState state = AppState::Menu;

    // --- GAME STATE ---
    bool isFirstTurn = true;
    bool animating = false;
    bool drawnFromFloor = false;
    int animTargetIdx = -1;
    sf::Clock animClock;
    float animDuration = 0.4f;

    // --- ANIMATION OBJECTS ---
    sf::RectangleShape ghostToHand(CARD_SIZE);
    sf::RectangleShape ghostToFloor(CARD_SIZE);
    sf::Texture texToHand, texToFloor, backTex, drawnTex, floorTex;
    backTex.loadFromFile("assets/cards/back.png");

    // --- LOGIC ---
    Deck deck;
    Player player1("P1");
    std::optional<Card> currentDrawnCard;
    std::optional<Card> lastFloorCard;
    std::string p1Input = "";
    std::vector<sf::Texture> handTextures(4);

    // --- UI POSITIONS ---
    sf::Vector2f deckPos = { 850.f, 100.f };
    sf::Vector2f floorPos = { 600.f, 100.f };
    sf::Vector2f drawnSlotPos = { 350.f, 100.f };

    sf::Text nameDisplay(font, "Enter Name: _", 40);
    nameDisplay.setPosition({ 350.f, 320.f });
    sf::Text playerNameLabel(font, "", 28);
    playerNameLabel.setFillColor(sf::Color::Yellow);

    sf::RectangleShape floorBox(CARD_SIZE);
    floorBox.setPosition(floorPos);
    floorBox.setFillColor(sf::Color(50, 50, 50));

    sf::RectangleShape drawnBox(CARD_SIZE);
    drawnBox.setPosition(drawnSlotPos);
    drawnBox.setFillColor(sf::Color::Transparent);

    std::vector<sf::RectangleShape> handBoxes;
    for (int i = 0; i < 4; i++) {
        sf::RectangleShape b(CARD_SIZE);
        b.setPosition({ 230.f + i * 160.f, 500.f });
        handBoxes.push_back(b);
    }

    while (window.isOpen()) {
        while (const std::optional event = window.pollEvent()) {
            if (event->is<sf::Event::Closed>()) window.close();

            // 1. MAIN MENU STATE
            if (state == AppState::Menu) {
                MenuResult res = mainMenu.handleEvent(*event, window);
                if (res == MenuResult::StartGame) state = AppState::Naming;
                if (res == MenuResult::Exit) window.close();
            }

            // 2. NAMING STATE (Your previous logic)
            else if (state == AppState::Naming) {
                if (const auto* textEvent = event->getIf<sf::Event::TextEntered>()) {
                    if (textEvent->unicode == '\b' && !p1Input.empty()) p1Input.pop_back();
                    else if (textEvent->unicode == '\r' || textEvent->unicode == '\n') {
                        if (!p1Input.empty()) {
                            player1.setName(p1Input);
                            playerNameLabel.setString(player1.getName());
                            sf::FloatRect tb = playerNameLabel.getLocalBounds();
                            playerNameLabel.setOrigin({ tb.position.x + tb.size.x / 2.f, 0 });
                            playerNameLabel.setPosition({ 470.f, 700.f });
                            for (int i = 0; i < 4; i++) player1.addCard(deck.drawCard());
                            state = AppState::Playing;
                        }
                    }
                    else if (textEvent->unicode < 128) p1Input += static_cast<char>(textEvent->unicode);
                    nameDisplay.setString("Enter Name: " + p1Input + "_");
                }
            }

            // 3. PLAYING STATE
            else if (state == AppState::Playing && event->is<sf::Event::MouseButtonPressed>() && !animating) {
                sf::Vector2f mouse = window.mapPixelToCoords(sf::Mouse::getPosition(window));

                // Draw from pile
                if (sf::FloatRect(deckPos, CARD_SIZE).contains(mouse) && !currentDrawnCard) {
                    isFirstTurn = false;
                    currentDrawnCard = deck.drawCard();
                    drawnFromFloor = false;
                    setCardTexture(drawnBox, drawnTex, cardToFileName(*currentDrawnCard));
                }

                // Floor Interaction
                if (floorBox.getGlobalBounds().contains(mouse)) {
                    if (currentDrawnCard.has_value() && !drawnFromFloor) {
                        lastFloorCard = currentDrawnCard;
                        setCardTexture(floorBox, floorTex, cardToFileName(*lastFloorCard));
                        currentDrawnCard = std::nullopt;
                        drawnBox.setFillColor(sf::Color::Transparent);
                    }
                    else if (!currentDrawnCard.has_value() && lastFloorCard.has_value()) {
                        isFirstTurn = false;
                        currentDrawnCard = lastFloorCard;
                        drawnFromFloor = true;
                        lastFloorCard = std::nullopt;
                        floorBox.setTexture(nullptr);
                        floorBox.setFillColor(sf::Color(50, 50, 50));
                        setCardTexture(drawnBox, drawnTex, cardToFileName(*currentDrawnCard));
                    }
                }

                // Swap with hand
                if (currentDrawnCard.has_value()) {
                    for (int i = 0; i < 4; i++) {
                        if (handBoxes[i].getGlobalBounds().contains(mouse)) {
                            animating = true;
                            animTargetIdx = i;
                            animClock.restart();

                            setCardTexture(ghostToHand, texToHand, cardToFileName(*currentDrawnCard));
                            ghostToHand.setPosition(drawnSlotPos);
                            setCardTexture(ghostToFloor, texToFloor, cardToFileName(player1.getHand()[i]));
                            ghostToFloor.setPosition(handBoxes[i].getPosition());

                            Card oldHand = player1.getHand()[i];
                            // Const cast hack for quick testing; ideally Player should have a swapCard method
                            std::vector<Card>& handRef = const_cast<std::vector<Card>&>(player1.getHand());
                            handRef[i] = *currentDrawnCard;
                            lastFloorCard = oldHand;

                            currentDrawnCard = std::nullopt;
                            drawnBox.setFillColor(sf::Color::Transparent);
                            break;
                        }
                    }
                }
            }
        }

        // --- UPDATE LOGIC ---
        if (state == AppState::Playing && animating) {
            float t = animClock.getElapsedTime().asSeconds() / animDuration;
            if (t >= 1.0f) {
                animating = false;
                setCardTexture(floorBox, floorTex, cardToFileName(*lastFloorCard));
                animTargetIdx = -1;
            }
            else {
                ghostToHand.setPosition(lerp(drawnSlotPos, handBoxes[animTargetIdx].getPosition(), t));
                ghostToFloor.setPosition(lerp(handBoxes[animTargetIdx].getPosition(), floorPos, t));
            }
        }

        // --- DRAWING ---
        window.clear(sf::Color(30, 60, 30));

        if (state == AppState::Menu) {
            mainMenu.draw(window);
        }
        else if (state == AppState::Naming) {
            window.draw(nameDisplay);
        }
        else if (state == AppState::Playing) {
            for (int i = 0; i < 4; i++) {
                if (animating && i == animTargetIdx) continue;
                if (isFirstTurn && i < 2)
                    setCardTexture(handBoxes[i], handTextures[i], cardToFileName(player1.getHand()[i]));
                else
                    handBoxes[i].setTexture(&backTex);
                window.draw(handBoxes[i]);
            }

            window.draw(floorBox);
            window.draw(drawnBox);
            window.draw(playerNameLabel);
            if (animating) { window.draw(ghostToHand); window.draw(ghostToFloor); }

            for (int i = 0; i < 3; i++) {
                sf::RectangleShape s(CARD_SIZE);
                s.setTexture(&backTex);
                s.setPosition({ deckPos.x - (i * 4), deckPos.y - (i * 4) });
                window.draw(s);
            }
        }

        window.display();
    }
    return 0;
}