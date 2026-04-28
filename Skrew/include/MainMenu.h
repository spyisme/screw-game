#pragma once

#include <SFML/Graphics.hpp>

enum class MenuResult
{
    None,
    StartGame,
    Exit
};

class MainMenu
{
private:
    sf::Font& font;

    sf::Text title;
    sf::Text subtitle;

    sf::RectangleShape startButton;
    sf::Text startText;

    sf::RectangleShape exitButton;
    sf::Text exitText;

public:
    MainMenu(sf::Font& font);

    MenuResult handleEvent(const sf::Event& event, const sf::RenderWindow& window);

    void draw(sf::RenderWindow& window);
};
