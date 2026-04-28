#pragma once

#include <SFML/Graphics.hpp>
#include <string>
#include <unordered_map>

class TextureManager
{
private:
    std::unordered_map<std::string, sf::Texture> textures;

public:
    sf::Texture* getTexture(const std::string& fileName);
};
