#include "../include/MainMenu.h"

MainMenu::MainMenu(sf::Font& font)
    : font(font),
    title(font, "Screw", 64),
    subtitle(font, "Multiplayer card table", 22),
    startText(font, "Start Game", 28),
    exitText(font, "Exit", 28)
{
    title.setFillColor(sf::Color(252, 248, 229));
    sf::FloatRect titleBounds = title.getLocalBounds();
    title.setOrigin({
        titleBounds.position.x + titleBounds.size.x / 2.f,
        titleBounds.position.y + titleBounds.size.y / 2.f
        });
    title.setPosition({ 500.f, 126.f });

    subtitle.setFillColor(sf::Color(195, 210, 194));
    sf::FloatRect subtitleBounds = subtitle.getLocalBounds();
    subtitle.setOrigin({
        subtitleBounds.position.x + subtitleBounds.size.x / 2.f,
        subtitleBounds.position.y + subtitleBounds.size.y / 2.f
        });
    subtitle.setPosition({ 500.f, 180.f });

    startButton.setSize({ 280.f, 64.f });
    startButton.setFillColor(sf::Color(35, 122, 84));
    startButton.setOutlineThickness(2.f);
    startButton.setOutlineColor(sf::Color(121, 209, 164));
    startButton.setPosition({ 360.f, 275.f });

    exitButton.setSize({ 280.f, 58.f });
    exitButton.setFillColor(sf::Color(92, 62, 64));
    exitButton.setOutlineThickness(2.f);
    exitButton.setOutlineColor(sf::Color(153, 109, 111));
    exitButton.setPosition({ 360.f, 363.f });

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
    sf::Vector2f mouse = window.mapPixelToCoords(sf::Mouse::getPosition(window));
    bool hoveringStart = startButton.getGlobalBounds().contains(mouse);
    bool hoveringExit = exitButton.getGlobalBounds().contains(mouse);

    startButton.setFillColor(hoveringStart ? sf::Color(47, 151, 102) : sf::Color(35, 122, 84));
    exitButton.setFillColor(hoveringExit ? sf::Color(118, 71, 75) : sf::Color(92, 62, 64));

    sf::RectangleShape tableGlow({ 680.f, 420.f });
    tableGlow.setPosition({ 160.f, 90.f });
    tableGlow.setFillColor(sf::Color(21, 72, 58));
    tableGlow.setOutlineThickness(3.f);
    tableGlow.setOutlineColor(sf::Color(61, 129, 99));

    sf::RectangleShape topRail({ 760.f, 32.f });
    topRail.setPosition({ 120.f, 535.f });
    topRail.setFillColor(sf::Color(25, 44, 39));

    sf::RectangleShape bottomRail({ 640.f, 20.f });
    bottomRail.setPosition({ 180.f, 576.f });
    bottomRail.setFillColor(sf::Color(17, 29, 27));

    window.draw(tableGlow);
    window.draw(topRail);
    window.draw(bottomRail);
    window.draw(title);
    window.draw(subtitle);
    window.draw(startButton);
    window.draw(startText);
    window.draw(exitButton);
    window.draw(exitText);
}
