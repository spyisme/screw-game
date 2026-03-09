#include "../include/MainMenu.h"

MainMenu::MainMenu(sf::Font& font)
    : font(font),
    title(font, "Skrew", 48),
    startText(font, "Start Game", 28),
    exitText(font, "Exit", 28)
{
    // 1. Title Styling & Positioning
    title.setFillColor(sf::Color::White);
    sf::FloatRect titleBounds = title.getLocalBounds();
    title.setOrigin({
        titleBounds.position.x + titleBounds.size.x / 2.f,
        titleBounds.position.y + titleBounds.size.y / 2.f
        });
    title.setPosition({ 550.f, 120.f });

    // 2. Button Setup
    startButton.setSize({ 250.f, 70.f });
    startButton.setFillColor(sf::Color(100, 100, 250));
    startButton.setPosition({ 425.f, 250.f });

    exitButton.setSize({ 250.f, 70.f });
    exitButton.setFillColor(sf::Color(180, 70, 70));
    exitButton.setPosition({ 425.f, 360.f });

    // 3. Text Positioning (Now that buttons have positions)
    startText.setFillColor(sf::Color::White);
    sf::FloatRect startBounds = startText.getLocalBounds();
    startText.setOrigin({
        startBounds.position.x + startBounds.size.x / 2.f,
        startBounds.position.y + startBounds.size.y / 2.f
        });
    startText.setPosition({
        startButton.getPosition().x + startButton.getSize().x / 2.f,
        startButton.getPosition().y + startButton.getSize().y / 2.f
        });

    exitText.setFillColor(sf::Color::White);
    sf::FloatRect exitBounds = exitText.getLocalBounds();
    exitText.setOrigin({
        exitBounds.position.x + exitBounds.size.x / 2.f,
        exitBounds.position.y + exitBounds.size.y / 2.f
        });
    exitText.setPosition({
        exitButton.getPosition().x + exitButton.getSize().x / 2.f,
        exitButton.getPosition().y + exitButton.getSize().y / 2.f
        });
}
MenuResult MainMenu::handleEvent(const sf::Event& event, const sf::RenderWindow& window)
{
    if (event.is<sf::Event::MouseButtonPressed>())
    {
        sf::Vector2i mousePixel = sf::Mouse::getPosition(window);
        sf::Vector2f mouse(
            static_cast<float>(mousePixel.x),
            static_cast<float>(mousePixel.y)
        );

        if (startButton.getGlobalBounds().contains(mouse))
            return MenuResult::StartGame;

        if (exitButton.getGlobalBounds().contains(mouse))
            return MenuResult::Exit;
    }

    return MenuResult::None;
}

void MainMenu::draw(sf::RenderWindow& window)
{
    window.draw(title);
    window.draw(startButton);
    window.draw(startText);
    window.draw(exitButton);
    window.draw(exitText);
}