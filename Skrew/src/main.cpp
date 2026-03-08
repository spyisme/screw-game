#include <iostream>
#include <string>
#include <vector>
#include <SFML/Graphics.hpp>
#include "../include/Deck.h"

std::string cardToFileName(const Card& card)
{
    if (card.type == CardType::Number)
        return std::to_string(card.value);
    if (card.type == CardType::Basra)
        return "Basra";
    if (card.type == CardType::GiveAndTake)
        return "5odwHat";
    if (card.type == CardType::SeeAndSwab)
        return "Seeandswab";

    return "Unknown";
}

int main()
{
    Deck deck;
    std::vector<Card> cards;

    auto drawNewCards = [&]() {
        deck = Deck();
        cards.clear();
        for (int i = 0; i < 4; i++)
            cards.push_back(deck.drawCard());
        };

    drawNewCards();

    sf::RenderWindow window(sf::VideoMode({ 1000, 600 }), "Card Game - Image Mode");

    sf::Font font;
    if (!font.openFromFile("assets/fonts/arial.ttf")) return 1;

    // Vectors to hold our UI
    std::vector<sf::RectangleShape> cardBoxes;
    std::vector<sf::Texture> cardTextures;
    cardTextures.resize(4); // We need 4 texture slots

    // Function to update textures on the boxes
    auto updateCardTextures = [&]() {
        for (int i = 0; i < 4; i++) {
            std::string filename = "assets/cards/" + cardToFileName(cards[i]) + ".png";
            if (!cardTextures[i].loadFromFile(filename)) {
                std::cout << "Failed to load: " << filename << "\n";
                // Fallback: make the box red if image is missing
                cardBoxes[i].setFillColor(sf::Color::Red);
            }
            else {
                cardBoxes[i].setTexture(&cardTextures[i]);
                cardBoxes[i].setFillColor(sf::Color::White); // Reset color to show texture properly
            }
        }
        };

    // Initialize Card Boxes
    for (int i = 0; i < 4; i++)
    {
        sf::RectangleShape box({ 180.f, 250.f });
        box.setPosition({ 80.f + i * 220.f, 100.f });
        box.setOutlineThickness(3.f);
        box.setOutlineColor(sf::Color::White);
        cardBoxes.push_back(box);
    }

    // Load initial textures
    updateCardTextures();

    // Reroll Button
    sf::RectangleShape button({ 160.f, 60.f });
    button.setFillColor(sf::Color(100, 100, 250));
    button.setPosition({ 420.f, 480.f });

    sf::Text buttonText(font, "Reroll", 24);
    buttonText.setFillColor(sf::Color::White);

    // Center button text (SFML 3.0 style)
    sf::FloatRect btb = buttonText.getLocalBounds();
    buttonText.setOrigin({ btb.position.x + btb.size.x / 2.0f, btb.position.y + btb.size.y / 2.0f });
    buttonText.setPosition({ button.getPosition().x + 80.f, button.getPosition().y + 30.f });

    while (window.isOpen())
    {
        while (const std::optional event = window.pollEvent())
        {
            if (event->is<sf::Event::Closed>())
                window.close();

            if (event->is<sf::Event::MouseButtonPressed>())
            {
                sf::Vector2i mousePixel = sf::Mouse::getPosition(window);
                sf::Vector2f mouse(static_cast<float>(mousePixel.x), static_cast<float>(mousePixel.y));

                if (button.getGlobalBounds().contains(mouse))
                {
                    drawNewCards();
                    updateCardTextures();
                }
            }
        }

        window.clear(sf::Color(40, 40, 40));

        for (const auto& box : cardBoxes) window.draw(box);
        window.draw(button);
        window.draw(buttonText);

        window.display();
    }

    return 0;
}