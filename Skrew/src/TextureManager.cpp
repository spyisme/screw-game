#include "../include/TextureManager.h"

#include <filesystem>
#include <iostream>

sf::Texture* TextureManager::getTexture(const std::string& fileName)
{
    auto found = textures.find(fileName);
    if (found != textures.end())
    {
        return &found->second;
    }

    sf::Texture texture;
    std::string path = "dlls/assets/cards/" + fileName + ".png";
    if (!std::filesystem::exists(path))
    {
        path = "assets/cards/" + fileName + ".png";
    }

    if (!texture.loadFromFile(path))
    {
        std::cout << "Failed to load texture: " << path << "\n";
        return nullptr;
    }

    texture.setSmooth(true);
    auto inserted = textures.emplace(fileName, std::move(texture));
    return &inserted.first->second;
}
