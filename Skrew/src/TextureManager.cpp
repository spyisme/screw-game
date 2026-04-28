#include "../include/TextureManager.h"

#include <iostream>

sf::Texture* TextureManager::getTexture(const std::string& fileName)
{
    auto found = textures.find(fileName);
    if (found != textures.end())
    {
        return &found->second;
    }

    sf::Texture texture;
    std::string path = "assets/cards/" + fileName + ".png";

    if (!texture.loadFromFile(path))
    {
        std::cout << "Failed to load texture: " << path << "\n";
        return nullptr;
    }

    texture.setSmooth(true);
    auto inserted = textures.emplace(fileName, std::move(texture));
    return &inserted.first->second;
}
