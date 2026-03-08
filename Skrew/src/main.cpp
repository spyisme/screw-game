#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <SFML/Graphics.hpp>
#include "../include/Deck.h"

// Helper function to wrap text within a specific width (SFML 3.0 Compatible)
void wrapText(sf::Text& text, float maxWidth) {
    std::string originalString = text.getString().toAnsiString();
    std::string wrapped;
    std::string word;
    std::string currentLine;

    std::istringstream words(originalString);
    while (words >> word) {
        std::string testLine = currentLine + (currentLine.empty() ? "" : " ") + word;
        text.setString(testLine);

        // Check if adding this word exceeds the box width
        if (text.getLocalBounds().size.x > maxWidth) {
            wrapped += currentLine + "\n";
            currentLine = word;
        }
        else {
            currentLine = testLine;
        }
    }
    wrapped += currentLine;
    text.setString(wrapped);
}

// Helper to center text origin for SFML 3.0
void centerText(sf::Text& text, const sf::RectangleShape& box) {
    sf::FloatRect textBounds = text.getLocalBounds();

    // Set origin to the center of the local text bounds
    text.setOrigin({
        textBounds.position.x + textBounds.size.x / 2.0f,
        textBounds.position.y + textBounds.size.y / 2.0f
        });

    // Position the text at the center of the provided box
    text.setPosition({
        box.getPosition().x + box.getSize().x / 2.0f,
        box.getPosition().y + box.getSize().y / 2.0f
        });
}

std::string cardToString(const Card& card)
{
    if (card.type == CardType::Number)
        return std::to_string(card.value);
    if (card.type == CardType::Basra)
        return "Basra";
    if (card.type == CardType::GiveAndTake)
        return "5od w hat";
    if (card.type == CardType::GiveOnly)
        return "5od";
    if (card.type == CardType::SeeAndSwab)
        return "See and swab";

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

    // 1. Bigger Window for SFML 3
    sf::RenderWindow window(sf::VideoMode({ 1000, 600 }), "Card Game - Wrapping Text");

    sf::Font font;
    if (!font.openFromFile("assets/fonts/arial.ttf"))
    {
        std::cout << "Font failed to load\n";
        return 1;
    }

    std::vector<sf::Text> texts;
    std::vector<sf::RectangleShape> cardBoxes;

    // Initialize UI Elements
    for (int i = 0; i < 4; i++)
    {
        // Setup Green Box
        sf::RectangleShape box({ 180.f, 250.f });
        box.setFillColor(sf::Color::Green);
        box.setOutlineThickness(3.f);
        box.setOutlineColor(sf::Color::White);
        box.setPosition({ 80.f + i * 220.f, 100.f });
        cardBoxes.push_back(box);

        // Setup Text
        texts.emplace_back(font, cardToString(cards[i]), 28);
        texts[i].setFillColor(sf::Color::Black);

        // Wrap and Center
        wrapText(texts[i], box.getSize().x - 20.f); // 20px padding
        centerText(texts[i], box);
    }

    // Reroll Button
    sf::RectangleShape button({ 160.f, 60.f });
    button.setFillColor(sf::Color(100, 100, 250));
    button.setPosition({ 420.f, 480.f });

    sf::Text buttonText(font, "Reroll", 24);
    buttonText.setFillColor(sf::Color::White);

    // Center button text manually
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
                    for (int i = 0; i < 4; i++) {
                        texts[i].setString(cardToString(cards[i]));
                        wrapText(texts[i], cardBoxes[i].getSize().x - 20.f);
                        centerText(texts[i], cardBoxes[i]);
                    }
                }
            }
        }

        window.clear(sf::Color(40, 40, 40));

        for (const auto& box : cardBoxes) window.draw(box);
        for (const auto& text : texts) window.draw(text);

        window.draw(button);
        window.draw(buttonText);

        window.display();
    }

    return 0;
}