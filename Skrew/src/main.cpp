#include <iostream>
#include <string>
#include <vector>
#include <SFML/Graphics.hpp>
#include "../include/Deck.h"

std::string cardToString(const Card& card)
{
    if (card.type == CardType::Number)
        return std::to_string(card.value);
    if (card.type == CardType::Basra)
        return "Basra";
    if (card.type == CardType::GiveAndTake)
        return "5od w hat";
    return "Unknown";
}

int main()
{
    Deck deck;

    std::vector<Card> cards;

    auto drawNewCards = [&]() {
        deck = Deck(); // new shuffled deck
        cards.clear();
        for (int i = 0; i < 4; i++)
            cards.push_back(deck.drawCard());
        };

    drawNewCards();

    sf::RenderWindow window(sf::VideoMode({ 800, 400 }), "Cards");

    sf::Font font;
    if (!font.openFromFile("assets/fonts/arial.ttf"))
    {
        std::cout << "Font failed to load\n";
        return 1;
    }

    std::vector<sf::Text> texts;
    texts.reserve(4); // Optional: prep space for performance

    for (int i = 0; i < 4; i++)
    {
        // Construct the text object directly inside the vector
        texts.emplace_back(font, cardToString(cards[i]), 40);
        texts[i].setFillColor(sf::Color::White);
        texts[i].setPosition({ 80.f + i * 180.f, 160.f });
    }

    // Button
    sf::RectangleShape button({ 150.f, 50.f });
    button.setFillColor(sf::Color(100, 100, 250));
    button.setPosition({ 325.f, 300.f });

    sf::Text buttonText(font, "Reroll", 24);
    buttonText.setFillColor(sf::Color::White);
    buttonText.setPosition({ 360.f, 310.f });

    while (window.isOpen())
    {
        while (const std::optional event = window.pollEvent())
        {
            if (event->is<sf::Event::MouseButtonPressed>())
            {
                sf::Vector2i mousePixel = sf::Mouse::getPosition(window);

                sf::Vector2f mouse(
                    static_cast<float>(mousePixel.x),
                    static_cast<float>(mousePixel.y)
                );

                if (button.getGlobalBounds().contains(mouse))
                {
                    drawNewCards();

                    for (int i = 0; i < 4; i++)
                        texts[i].setString(cardToString(cards[i]));
                }
            }
        }

        window.clear();

        for (auto& t : texts)
            window.draw(t);

        window.draw(button);
        window.draw(buttonText);

        window.display();
    }

    return 0;
}