#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <vector>
#define NOMINMAX
#include <windows.h>
#include <SFML/Graphics.hpp>
#include "../include/GameClient.h"
#include "../include/MainMenu.h"
#include "../include/TextureManager.h"

const sf::Vector2f CARD_SIZE = { 96.f, 132.f };
const sf::Vector2f OPPONENT_CARD_SIZE = CARD_SIZE;
const float PLAYER_CARD_SPACING = 112.f;
const float OPPONENT_CARD_SPACING = PLAYER_CARD_SPACING;
const float OPPONENT_ROW_SPACING = 150.f;
const sf::Color TABLE_BACKGROUND = sf::Color(18, 72, 50);
const sf::Color TABLE_PANEL = sf::Color(24, 91, 66);
const sf::Color TABLE_PANEL_DARK = sf::Color(16, 55, 45);
const sf::Color TABLE_RAIL = sf::Color(76, 47, 34);
const sf::Color TEXT_PRIMARY = sf::Color(248, 244, 224);
const sf::Color TEXT_SECONDARY = sf::Color(202, 217, 201);
const sf::Color ACCENT_GREEN = sf::Color(44, 145, 95);
const sf::Color ACCENT_BLUE = sf::Color(63, 101, 167);
const sf::Color ACCENT_GOLD = sf::Color(232, 196, 95);
const sf::Color DANGER_TEXT = sf::Color(255, 190, 178);

enum class AppState
{
    Menu,
    Joining,
    RoomSelect,
    WaitingRoom,
    Playing
};

struct RoomInfo
{
    std::string joinId;
    std::string displayText;
};

struct AppConfig
{
    std::string host = "127.0.0.1";
    int port = 5000;
    unsigned int width = 1000;
    unsigned int height = 700;
};

std::vector<nlohmann::json> getPlayerHand(const nlohmann::json& state, const std::string& playerId);

sf::Vector2f lerp(sf::Vector2f start, sf::Vector2f end, float t)
{
    return start + t * (end - start);
}

float clamp01(float value)
{
    return std::max(0.f, std::min(1.f, value));
}

float easeInOut(float t)
{
    t = clamp01(t);
    return t * t * (3.f - 2.f * t);
}

std::string lowerText(const std::string& text)
{
    std::string result = text;
    for (char& letter : result)
    {
        letter = static_cast<char>(std::tolower(static_cast<unsigned char>(letter)));
    }
    return result;
}

std::string stripPngExtension(const std::string& fileName)
{
    if (fileName.size() > 4 && fileName.substr(fileName.size() - 4) == ".png")
    {
        return fileName.substr(0, fileName.size() - 4);
    }
    return fileName;
}

AppConfig loadAppConfig()
{
    AppConfig config;
    std::ifstream file("config.json");

    if (!file.is_open())
    {
        std::cout << "config.json not found. Using default settings.\n";
        return config;
    }

    try
    {
        nlohmann::json configJson = nlohmann::json::parse(file);

        if (configJson.contains("host") && configJson["host"].is_string())
        {
            config.host = configJson["host"].get<std::string>();
        }

        if (configJson.contains("port") && configJson["port"].is_number_integer())
        {
            config.port = configJson["port"].get<int>();
        }

        if (configJson.contains("width") && configJson["width"].is_number_integer())
        {
            int width = configJson["width"].get<int>();
            if (width > 0)
            {
                config.width = static_cast<unsigned int>(width);
            }
        }

        if (configJson.contains("height") && configJson["height"].is_number_integer())
        {
            int height = configJson["height"].get<int>();
            if (height > 0)
            {
                config.height = static_cast<unsigned int>(height);
            }
        }
    }
    catch (const nlohmann::json::exception&)
    {
        std::cout << "config.json is invalid. Using default settings.\n";
    }

    return config;
}

bool jsonBool(const nlohmann::json& data, const std::string& fieldName, bool defaultValue)
{
    if (data.is_object() && data.contains(fieldName) && data[fieldName].is_boolean())
    {
        return data[fieldName].get<bool>();
    }
    return defaultValue;
}

std::string jsonString(const nlohmann::json& data, const std::vector<std::string>& fieldNames)
{
    if (!data.is_object())
    {
        return "";
    }

    for (const std::string& fieldName : fieldNames)
    {
        if (data.contains(fieldName))
        {
            if (data[fieldName].is_string())
            {
                return data[fieldName].get<std::string>();
            }

            if (data[fieldName].is_number_integer())
            {
                return std::to_string(data[fieldName].get<int>());
            }
        }
    }

    return "";
}

std::vector<RoomInfo> readRoomsFromJson(const nlohmann::json& roomsJson)
{
    std::vector<RoomInfo> rooms;
    const nlohmann::json* roomArray = nullptr;

    if (roomsJson.is_array())
    {
        roomArray = &roomsJson;
    }
    else if (roomsJson.is_object() && roomsJson.contains("rooms") && roomsJson["rooms"].is_array())
    {
        roomArray = &roomsJson["rooms"];
    }
    else if (roomsJson.is_object() && roomsJson.contains("public_rooms") && roomsJson["public_rooms"].is_array())
    {
        roomArray = &roomsJson["public_rooms"];
    }

    if (roomArray == nullptr)
    {
        return rooms;
    }

    for (const auto& room : *roomArray)
    {
        std::string roomId = jsonString(room, { "room_id", "roomId", "id" });
        std::string roomCode = jsonString(room, { "room_code", "code" });
        std::string joinId = roomId.empty() ? roomCode : roomId;

        if (joinId.empty())
        {
            continue;
        }

        std::string playerCount = jsonString(room, { "player_count", "players_count", "current_players" });
        std::string maxPlayers = jsonString(room, { "max_players", "maxPlayers" });

        if (playerCount.empty() && room.contains("players") && room["players"].is_array())
        {
            playerCount = std::to_string(room["players"].size());
        }

        std::string displayName = roomCode.empty() ? roomId : roomCode;
        std::string displayText = "Room " + displayName;
        if (!playerCount.empty() || !maxPlayers.empty())
        {
            displayText += "    Players: " + (playerCount.empty() ? "?" : playerCount);
            displayText += "/" + (maxPlayers.empty() ? "?" : maxPlayers);
        }

        rooms.push_back({ joinId, displayText });
    }

    return rooms;
}

std::vector<std::string> readLobbyPlayerNames(const nlohmann::json& lobbyJson)
{
    std::vector<std::string> names;

    if (!lobbyJson.is_object() || !lobbyJson.contains("players") || !lobbyJson["players"].is_array())
    {
        return names;
    }

    for (const auto& player : lobbyJson["players"])
    {
        std::string name = jsonString(player, { "nickname", "name", "player_id", "id" });
        if (!name.empty())
        {
            names.push_back(name);
        }
    }

    return names;
}

std::string getLobbyPlayerCountText(const nlohmann::json& lobbyJson)
{
    int playerCount = 0;
    std::string maxPlayers = "4";

    if (lobbyJson.is_object())
    {
        if (lobbyJson.contains("players") && lobbyJson["players"].is_array())
        {
            playerCount = static_cast<int>(lobbyJson["players"].size());
        }

        std::string value = jsonString(lobbyJson, { "max_players", "maxPlayers" });
        if (!value.empty())
        {
            maxPlayers = value;
        }
    }

    return std::to_string(playerCount) + "/" + maxPlayers;
}

bool gameHasStarted(const nlohmann::json& state, const std::string& playerId)
{
    if (!state.is_object())
    {
        return false;
    }

    std::string status = lowerText(jsonString(state, { "status", "state", "phase" }));
    if (status == "playing" || status == "started" || status == "in_progress")
    {
        return true;
    }

    return !getPlayerHand(state, playerId).empty();
}

bool cardIsVisible(const nlohmann::json& card)
{
    if (card.is_null())
    {
        return false;
    }

    if (card.is_string())
    {
        std::string value = lowerText(card.get<std::string>());
        return value != "hidden" && value != "unknown" && value != "back";
    }

    if (!card.is_object())
    {
        return true;
    }

    if (card.contains("visible"))
    {
        return jsonBool(card, "visible", false);
    }

    if (card.contains("is_visible"))
    {
        return jsonBool(card, "is_visible", false);
    }

    if (card.contains("hidden"))
    {
        return !jsonBool(card, "hidden", true);
    }

    if (card.contains("face_down"))
    {
        return !jsonBool(card, "face_down", true);
    }

    return card.contains("value") || card.contains("type") || card.contains("name") ||
        card.contains("file") || card.contains("image_name") || card.contains("card");
}

std::string cardFileName(const nlohmann::json& card)
{
    if (!cardIsVisible(card))
    {
        return "back";
    }

    if (card.is_number_integer())
    {
        return std::to_string(card.get<int>());
    }

    if (card.is_string())
    {
        std::string value = stripPngExtension(card.get<std::string>());
        std::string lowered = lowerText(value);

        if (lowered == "giveandtake" || lowered == "give_and_take")
        {
            return "GiveAndTake";
        }

        if (lowered == "seeandswab" || lowered == "seeandswap" || lowered == "see_and_swab" || lowered == "see_and_swap")
        {
            return "Seeandswab";
        }

        return value;
    }

    if (!card.is_object())
    {
        return "back";
    }

    if (card.contains("card") && card["card"].is_object())
    {
        return cardFileName(card["card"]);
    }

    std::string fileName = jsonString(card, { "image_name", "file", "file_name", "image", "name" });
    if (!fileName.empty())
    {
        return cardFileName(fileName);
    }

    std::string type = lowerText(jsonString(card, { "type", "card_type" }));
    if (type == "giveandtake" || type == "give_and_take")
    {
        return "GiveAndTake";
    }

    if (type == "seeandswab" || type == "seeandswap" || type == "see_and_swab" || type == "see_and_swap")
    {
        return "Seeandswab";
    }

    std::string value = jsonString(card, { "value", "rank" });
    if (!value.empty())
    {
        return value;
    }

    return "back";
}

std::vector<nlohmann::json> getPlayerHand(const nlohmann::json& state, const std::string& playerId)
{
    std::vector<nlohmann::json> hand;

    const nlohmann::json* playerData = nullptr;
    if (state.is_object() && state.contains("player") && state["player"].is_object())
    {
        playerData = &state["player"];
    }

    if (playerData != nullptr)
    {
        if (playerData->contains("hand") && (*playerData)["hand"].is_array())
        {
            for (const auto& card : (*playerData)["hand"])
            {
                hand.push_back(card);
            }
            return hand;
        }

        if (playerData->contains("cards") && (*playerData)["cards"].is_array())
        {
            for (const auto& card : (*playerData)["cards"])
            {
                hand.push_back(card);
            }
            return hand;
        }
    }

    for (const std::string& fieldName : { "hand", "own_cards", "cards" })
    {
        if (state.is_object() && state.contains(fieldName) && state[fieldName].is_array())
        {
            for (const auto& card : state[fieldName])
            {
                hand.push_back(card);
            }
            return hand;
        }
    }

    if (state.is_object() && state.contains("players") && state["players"].is_array())
    {
        for (const auto& player : state["players"])
        {
            std::string id = jsonString(player, { "player_id", "playerId", "id" });
            if (id == playerId)
            {
                if (player.contains("hand") && player["hand"].is_array())
                {
                    for (const auto& card : player["hand"])
                    {
                        hand.push_back(card);
                    }
                }
                else if (player.contains("cards") && player["cards"].is_array())
                {
                    for (const auto& card : player["cards"])
                    {
                        hand.push_back(card);
                    }
                }
                return hand;
            }
        }
    }

    return hand;
}

std::optional<nlohmann::json> getFirstCardField(const nlohmann::json& state, const std::vector<std::string>& fieldNames)
{
    if (!state.is_object())
    {
        return std::nullopt;
    }

    for (const std::string& fieldName : fieldNames)
    {
        if (!state.contains(fieldName))
        {
            continue;
        }

        const nlohmann::json& value = state[fieldName];
        if (value.is_array() && !value.empty())
        {
            return value.back();
        }

        if (!value.is_array() && !value.is_null())
        {
            return value;
        }
    }

    return std::nullopt;
}

std::optional<nlohmann::json> getDrawnCard(const nlohmann::json& state)
{
    if (state.is_object() && state.contains("player") && state["player"].is_object())
    {
        auto card = getFirstCardField(state["player"], { "pending_drawn_card", "drawn_card", "current_drawn_card", "drawnCard" });
        if (card.has_value())
        {
            return card;
        }
    }

    return getFirstCardField(state, { "pending_drawn_card", "drawn_card", "current_drawn_card", "drawnCard" });
}

std::optional<nlohmann::json> getDiscardCard(const nlohmann::json& state)
{
    return getFirstCardField(state, { "top_discard_card", "top_discard", "discard_top", "discard_card", "floor_card", "discard", "floor_pile", "discard_pile" });
}

std::string getDiscardCardName(const nlohmann::json& state)
{
    std::optional<nlohmann::json> discardCard = getDiscardCard(state);
    return discardCard.has_value() ? cardFileName(*discardCard) : "";
}

std::optional<int> getIntegerField(const nlohmann::json& data, const std::vector<std::string>& fieldNames)
{
    if (!data.is_object())
    {
        return std::nullopt;
    }

    for (const std::string& fieldName : fieldNames)
    {
        if (!data.contains(fieldName))
        {
            continue;
        }

        if (data[fieldName].is_number_integer())
        {
            return data[fieldName].get<int>();
        }

        if (data[fieldName].is_string())
        {
            try
            {
                return std::stoi(data[fieldName].get<std::string>());
            }
            catch (const std::exception&)
            {
                return std::nullopt;
            }
        }
    }

    return std::nullopt;
}

std::optional<int> getDrawPileCount(const nlohmann::json& state)
{
    return getIntegerField(state, { "draw_pile_count", "drawPileCount", "deck_count", "deckCount", "cards_pile_count", "cardsPileCount" });
}

bool hasAllowedAction(const nlohmann::json& state, const std::string& action)
{
    if (!state.is_object() || !state.contains("allowed_actions") || !state["allowed_actions"].is_array())
    {
        return false;
    }

    for (const auto& allowedAction : state["allowed_actions"])
    {
        if (allowedAction.is_string() && allowedAction.get<std::string>() == action)
        {
            return true;
        }
    }

    return false;
}

bool isActionTargetPhase(const nlohmann::json& state)
{
    std::string phase = lowerText(jsonString(state, { "phase", "status" }));
    return phase == "waiting_for_action_target" ||
        hasAllowedAction(state, "resolve_give_and_take") ||
        hasAllowedAction(state, "reveal_see_and_swap_target") ||
        hasAllowedAction(state, "resolve_see_and_swap") ||
        hasAllowedAction(state, "cancel_see_and_swap");
}

std::string getPendingActionType(const nlohmann::json& state)
{
    if (state.is_object() && state.contains("pending_action") && state["pending_action"].is_object())
    {
        return lowerText(jsonString(state["pending_action"], { "type", "action_type" }));
    }

    return "";
}

sf::FloatRect getOpponentCardBounds(int playerIndex, int cardIndex)
{
    float rowY = 78.f + playerIndex * OPPONENT_ROW_SPACING;
    return sf::FloatRect({ 36.f + cardIndex * OPPONENT_CARD_SPACING, rowY }, OPPONENT_CARD_SIZE);
}

std::string getPlayerNameFromState(const nlohmann::json& state, const std::string& fallbackName)
{
    if (state.is_object() && state.contains("you") && state["you"].is_object())
    {
        std::string name = jsonString(state["you"], { "nickname", "name" });
        if (!name.empty())
        {
            return name;
        }
    }

    if (state.is_object() && state.contains("player") && state["player"].is_object())
    {
        std::string name = jsonString(state["player"], { "nickname", "name" });
        if (!name.empty())
        {
            return name;
        }
    }

    return fallbackName;
}

std::string getPlayerNicknameById(const nlohmann::json& state, const std::string& playerId)
{
    if (!state.is_object() || !state.contains("players") || !state["players"].is_array())
    {
        return "";
    }

    for (const auto& player : state["players"])
    {
        std::string id = jsonString(player, { "player_id", "playerId", "id" });
        if (id == playerId)
        {
            return jsonString(player, { "nickname", "name" });
        }
    }

    return "";
}

std::string getCurrentTurnText(const nlohmann::json& state)
{
    if (!state.is_object())
    {
        return "Current turn: waiting";
    }

    std::string nickname = jsonString(state, { "current_player_nickname" });
    if (!nickname.empty())
    {
        return "Current turn: " + nickname;
    }

    std::string playerId = jsonString(state, { "current_player_id" });
    if (!playerId.empty())
    {
        std::string nickname = getPlayerNicknameById(state, playerId);
        if (!nickname.empty())
        {
            return "Current turn: " + nickname;
        }

        return "Current turn: " + playerId;
    }

    return "Current turn: waiting";
}

std::string getCurrentPlayerId(const nlohmann::json& state)
{
    return jsonString(state, { "current_player_id", "currentPlayerId" });
}

std::string makeStateSignature(const nlohmann::json& state)
{
    std::string signature = jsonString(state, { "phase", "status" });
    signature += "|turn:" + getCurrentPlayerId(state);
    signature += "|draw:" + jsonString(state, { "draw_pile_count", "drawPileCount" });

    std::optional<nlohmann::json> discardCard = getDiscardCard(state);
    if (discardCard.has_value())
    {
        signature += "|discard:" + cardFileName(*discardCard);
    }

    std::optional<nlohmann::json> drawnCard = getDrawnCard(state);
    signature += drawnCard.has_value() ? "|pending:yes" : "|pending:no";

    return signature;
}

std::vector<nlohmann::json> getOtherPlayers(const nlohmann::json& state, const std::string& playerId)
{
    std::vector<nlohmann::json> otherPlayers;

    if (!state.is_object() || !state.contains("players") || !state["players"].is_array())
    {
        return otherPlayers;
    }

    for (const auto& player : state["players"])
    {
        std::string id = jsonString(player, { "player_id", "playerId", "id" });
        if (id != playerId)
        {
            otherPlayers.push_back(player);
        }
    }

    return otherPlayers;
}

std::vector<nlohmann::json> getCardsFromPlayer(const nlohmann::json& player)
{
    std::vector<nlohmann::json> cards;

    if (player.is_object() && player.contains("cards") && player["cards"].is_array())
    {
        for (const auto& card : player["cards"])
        {
            cards.push_back(card);
        }
    }
    else if (player.is_object() && player.contains("hand") && player["hand"].is_array())
    {
        for (const auto& card : player["hand"])
        {
            cards.push_back(card);
        }
    }

    return cards;
}

void setBoxTexture(sf::RectangleShape& box, TextureManager& textures, const std::string& fileName)
{
    sf::Texture* texture = textures.getTexture(fileName);
    if (texture != nullptr)
    {
        box.setTexture(texture, true);
        box.setFillColor(sf::Color::White);
    }
    else
    {
        box.setTexture(nullptr);
        box.setFillColor(sf::Color(160, 40, 40));
    }
}

void centerText(sf::Text& text, const sf::RectangleShape& box)
{
    sf::FloatRect bounds = text.getLocalBounds();
    text.setOrigin({
        bounds.position.x + bounds.size.x / 2.f,
        bounds.position.y + bounds.size.y / 2.f
        });
    text.setPosition({
        box.getPosition().x + box.getSize().x / 2.f,
        box.getPosition().y + box.getSize().y / 2.f
        });
}

void fitTextToWidth(sf::Text& text, float maxWidth, unsigned int minCharacterSize = 14)
{
    while (text.getCharacterSize() > minCharacterSize && text.getLocalBounds().size.x > maxWidth)
    {
        text.setCharacterSize(text.getCharacterSize() - 1);
    }
}

void fitTextToBox(sf::Text& text, sf::Vector2f maxSize, unsigned int minCharacterSize = 14)
{
    while (text.getCharacterSize() > minCharacterSize)
    {
        sf::FloatRect bounds = text.getLocalBounds();
        if (bounds.size.x <= maxSize.x && bounds.size.y <= maxSize.y)
        {
            break;
        }

        text.setCharacterSize(text.getCharacterSize() - 1);
    }
}

void setFittedText(sf::Text& text, const std::string& value, unsigned int characterSize, float maxWidth, unsigned int minCharacterSize = 14)
{
    text.setCharacterSize(characterSize);
    text.setString(value);
    fitTextToWidth(text, maxWidth, minCharacterSize);
}

void drawPanel(sf::RenderWindow& window, sf::Vector2f position, sf::Vector2f size, sf::Color fill, sf::Color outline)
{
    sf::RectangleShape panel(size);
    panel.setPosition(position);
    panel.setFillColor(fill);
    panel.setOutlineThickness(2.f);
    panel.setOutlineColor(outline);
    window.draw(panel);
}

void drawButton(sf::RenderWindow& window, sf::RectangleShape& button, sf::Text& label, bool hovered, sf::Color normal, sf::Color hover)
{
    button.setFillColor(hovered ? hover : normal);
    button.setOutlineThickness(2.f);
    button.setOutlineColor(hovered ? ACCENT_GOLD : sf::Color(118, 154, 126));
    fitTextToWidth(label, button.getSize().x - 28.f, 16);
    centerText(label, button);
    window.draw(button);
    window.draw(label);
}

void drawCardLabel(sf::RenderWindow& window, sf::Font& font, const std::string& label, sf::Vector2f cardPosition, sf::Color color = TEXT_SECONDARY)
{
    sf::Text text(font, label, 17);
    text.setFillColor(color);
    sf::FloatRect bounds = text.getLocalBounds();
    text.setOrigin({ bounds.position.x + bounds.size.x / 2.f, bounds.position.y });
    text.setPosition({ cardPosition.x + CARD_SIZE.x / 2.f, cardPosition.y + CARD_SIZE.y + 10.f });
    window.draw(text);
}

void styleCardSlot(sf::RectangleShape& box, bool hovered, bool actionable)
{
    box.setOutlineThickness(actionable ? 4.f : 2.f);
    if (hovered && actionable)
    {
        box.setOutlineColor(ACCENT_GOLD);
    }
    else if (actionable)
    {
        box.setOutlineColor(sf::Color(116, 217, 151));
    }
    else if (hovered)
    {
        box.setOutlineColor(sf::Color(170, 190, 176));
    }
    else
    {
        box.setOutlineColor(sf::Color(64, 104, 78));
    }
}

void drawStatusBar(sf::RenderWindow& window, sf::Font& font, const std::string& message, bool isError)
{
    if (message.empty())
    {
        return;
    }

    drawPanel(window, { 24.f, 626.f }, { 952.f, 46.f }, sf::Color(27, 48, 45), isError ? DANGER_TEXT : sf::Color(115, 167, 135));
    sf::Text text(font, message, 20);
    text.setFillColor(isError ? DANGER_TEXT : TEXT_PRIMARY);
    fitTextToWidth(text, 912.f, 14);
    text.setPosition({ 42.f, 638.f });
    window.draw(text);
}

std::filesystem::path getExecutableDirectory()
{
    std::vector<char> path(MAX_PATH);
    DWORD length = 0;

    while (true)
    {
        length = GetModuleFileNameA(nullptr, path.data(), static_cast<DWORD>(path.size()));
        if (length == 0)
        {
            return std::filesystem::current_path();
        }

        if (length < path.size() - 1)
        {
            break;
        }

        path.resize(path.size() * 2);
    }

    return std::filesystem::path(std::string(path.data(), length)).parent_path();
}

void showFatalError(const std::string& message)
{
    std::ofstream log("crash_log.txt", std::ios::app);
    log << "Skrew crashed:\n" << message << "\n\n";

    MessageBoxA(
        nullptr,
        (message + "\n\nDetails were written to crash_log.txt next to Skrew.exe.").c_str(),
        "Skrew startup error",
        MB_OK | MB_ICONERROR);
}

int runApp()
{
    std::filesystem::current_path(getExecutableDirectory());

    AppConfig appConfig = loadAppConfig();

    sf::RenderWindow window(sf::VideoMode({ appConfig.width, appConfig.height }), "Screw");
    window.setFramerateLimit(60);
    window.setView(sf::View(sf::FloatRect({ 0.f, 0.f }, { 1000.f, 700.f })));
    sf::Font font;
    if (!font.openFromFile("assets/fonts/arial.ttf"))
    {
        throw std::runtime_error("Failed to load assets/fonts/arial.ttf. Make sure the assets folder is next to Skrew.exe.");
    }

    std::cout << "Backend: " << appConfig.host << ":" << appConfig.port << "\n";
    std::cout << "Window: " << appConfig.width << "x" << appConfig.height << "\n";

    MainMenu mainMenu(font);
    GameClient client(appConfig.host, appConfig.port);
    TextureManager textures;

    AppState state = AppState::Menu;
    std::string nicknameInput;
    std::string nickname;
    std::string statusMessage;
    std::vector<RoomInfo> availableRooms;
    nlohmann::json latestLobby;
    bool isHost = false;
    bool waitingForDiscardSwap = false;
    int giveAndTakeOwnIndex = -1;
    std::string lastObservedStateSignature;
    std::string lastObservedCurrentPlayerId;
    std::string remoteDeckDrawPlayerId;

    bool animating = false;
    bool remoteAnimating = false;
    int animTargetIndex = -1;
    sf::Clock animClock;
    sf::Clock waitingRoomPollClock;
    sf::Clock lobbyPollClock;
    sf::Clock playingPollClock;
    sf::Clock remoteAnimClock;
    float animDuration = 0.42f;
    float remoteAnimDuration = 0.62f;
    sf::RectangleShape ghostToHand(CARD_SIZE);
    sf::RectangleShape ghostToDiscard(CARD_SIZE);
    sf::RectangleShape remoteGhost(OPPONENT_CARD_SIZE);
    sf::Vector2f remoteAnimStart = { 36.f, 78.f };

    sf::Vector2f deckPos = { 830.f, 112.f };
    sf::Vector2f discardPos = { 680.f, 112.f };
    sf::Vector2f remoteAnimEnd = discardPos;
    sf::Vector2f drawnPos = { 530.f, 112.f };

    sf::Text joiningTitle(font, "Choose your table name", 42);
    joiningTitle.setFillColor(TEXT_PRIMARY);
    joiningTitle.setPosition({ 262.f, 190.f });

    sf::Text joiningText(font, "Nickname: _", 34);
    joiningText.setFillColor(TEXT_PRIMARY);
    joiningText.setPosition({ 300.f, 310.f });

    sf::Text joiningHint(font, "Press Enter to continue", 20);
    joiningHint.setFillColor(TEXT_SECONDARY);
    joiningHint.setPosition({ 365.f, 365.f });

    sf::Text roomSelectTitle(font, "Choose Room", 42);
    roomSelectTitle.setFillColor(TEXT_PRIMARY);
    roomSelectTitle.setPosition({ 340.f, 70.f });

    sf::Text roomSelectInfo(font, "", 22);
    roomSelectInfo.setFillColor(TEXT_SECONDARY);
    roomSelectInfo.setPosition({ 220.f, 135.f });

    sf::RectangleShape createRoomButton({ 260.f, 58.f });
    createRoomButton.setPosition({ 240.f, 205.f });
    createRoomButton.setFillColor(ACCENT_GREEN);

    sf::Text createRoomText(font, "Create New Room", 26);
    createRoomText.setFillColor(sf::Color::White);
    centerText(createRoomText, createRoomButton);

    sf::RectangleShape refreshRoomsButton({ 220.f, 58.f });
    refreshRoomsButton.setPosition({ 525.f, 205.f });
    refreshRoomsButton.setFillColor(ACCENT_BLUE);

    sf::Text refreshRoomsText(font, "Refresh Rooms", 26);
    refreshRoomsText.setFillColor(sf::Color::White);
    centerText(refreshRoomsText, refreshRoomsButton);

    sf::Text waitingTitle(font, "Waiting Room", 42);
    waitingTitle.setFillColor(TEXT_PRIMARY);
    waitingTitle.setPosition({ 340.f, 95.f });

    sf::Text waitingInfo(font, "", 26);
    waitingInfo.setFillColor(TEXT_PRIMARY);
    waitingInfo.setPosition({ 280.f, 200.f });

    sf::RectangleShape startButton({ 220.f, 62.f });
    startButton.setPosition({ 390.f, 430.f });
    startButton.setFillColor(ACCENT_BLUE);

    sf::Text startButtonText(font, "Start", 28);
    startButtonText.setFillColor(sf::Color::White);
    centerText(startButtonText, startButton);

    sf::Text playerNameLabel(font, "", 24);
    playerNameLabel.setFillColor(TEXT_PRIMARY);
    playerNameLabel.setPosition({ 548.f, 502.f });

    sf::Text currentTurnLabel(font, "Current turn: waiting", 24);
    currentTurnLabel.setFillColor(ACCENT_GOLD);
    currentTurnLabel.setPosition({ 320.f, 27.f });

    sf::Text errorText(font, "", 20);
    errorText.setFillColor(DANGER_TEXT);
    errorText.setPosition({ 42.f, 638.f });

    sf::Text hintText(font, "", 19);
    hintText.setFillColor(TEXT_SECONDARY);
    hintText.setPosition({ 284.f, 475.f });

    sf::RectangleShape deckBox(CARD_SIZE);
    deckBox.setPosition(deckPos);
    setBoxTexture(deckBox, textures, "back");

    sf::RectangleShape discardBox(CARD_SIZE);
    discardBox.setPosition(discardPos);
    discardBox.setFillColor(sf::Color(50, 50, 50));

    sf::RectangleShape drawnBox(CARD_SIZE);
    drawnBox.setPosition(drawnPos);
    drawnBox.setFillColor(sf::Color(50, 50, 50));

    std::vector<sf::RectangleShape> handBoxes;
    for (int i = 0; i < 4; i++)
    {
        sf::RectangleShape box(CARD_SIZE);
        box.setPosition({ 284.f + i * PLAYER_CARD_SPACING, 526.f });
        handBoxes.push_back(box);
    }

    while (window.isOpen())
    {
        while (const std::optional event = window.pollEvent())
        {
            if (event->is<sf::Event::Closed>())
            {
                window.close();
            }

            if (state == AppState::Menu)
            {
                MenuResult result = mainMenu.handleEvent(*event, window);
                if (result == MenuResult::StartGame)
                {
                    state = AppState::Joining;
                    nicknameInput.clear();
                    nickname.clear();
                    availableRooms.clear();
                    latestLobby = nlohmann::json::object();
                    isHost = false;
                    lastObservedStateSignature.clear();
                    lastObservedCurrentPlayerId.clear();
                    remoteDeckDrawPlayerId.clear();
                    setFittedText(joiningText, "Nickname: _", 34, 410.f, 18);
                    statusMessage.clear();
                }
                else if (result == MenuResult::Exit)
                {
                    window.close();
                }
            }
            else if (state == AppState::Joining)
            {
                if (const auto* textEvent = event->getIf<sf::Event::TextEntered>())
                {
                    if (textEvent->unicode == '\b' && !nicknameInput.empty())
                    {
                        nicknameInput.pop_back();
                    }
                    else if (textEvent->unicode == '\r' || textEvent->unicode == '\n')
                    {
                        if (!nicknameInput.empty())
                        {
                            nickname = nicknameInput;
                            statusMessage = "Loading rooms...";

                            nlohmann::json roomsJson;
                            if (client.fetchRooms(roomsJson))
                            {
                                availableRooms = readRoomsFromJson(roomsJson);
                                state = AppState::RoomSelect;
                                statusMessage.clear();
                            }
                            else
                            {
                                statusMessage = client.getLastError();
                            }
                        }
                    }
                    else if (textEvent->unicode >= 32 && textEvent->unicode < 128 && nicknameInput.size() < 20)
                    {
                        nicknameInput += static_cast<char>(textEvent->unicode);
                    }

                    setFittedText(joiningText, "Nickname: " + nicknameInput + "_", 34, 410.f, 18);
                }
            }
            else if (state == AppState::RoomSelect)
            {
                if (event->is<sf::Event::MouseButtonPressed>())
                {
                    sf::Vector2f mouse = window.mapPixelToCoords(sf::Mouse::getPosition(window));

                    if (createRoomButton.getGlobalBounds().contains(mouse))
                    {
                        statusMessage = "Creating public room...";
                        if (client.createRoom(true, 4) && client.joinRoom(client.getRoomId(), nickname))
                        {
                            isHost = true;
                            client.fetchLobby(latestLobby);
                            state = AppState::WaitingRoom;
                            statusMessage = "Room created. Waiting for another player.";
                            waitingRoomPollClock.restart();
                            lobbyPollClock.restart();
                        }
                        else
                        {
                            statusMessage = client.getLastError();
                        }
                    }
                    else if (refreshRoomsButton.getGlobalBounds().contains(mouse))
                    {
                        nlohmann::json roomsJson;
                        statusMessage = "Refreshing rooms...";
                        if (client.fetchRooms(roomsJson))
                        {
                            availableRooms = readRoomsFromJson(roomsJson);
                            statusMessage.clear();
                        }
                        else
                        {
                            statusMessage = client.getLastError();
                        }
                    }
                    else
                    {
                        const float firstRoomY = 305.f;
                        const float roomHeight = 58.f;
                        const float roomX = 250.f;
                        const float roomWidth = 600.f;

                        for (int i = 0; i < static_cast<int>(availableRooms.size()); i++)
                        {
                            sf::FloatRect roomBounds({ roomX, firstRoomY + i * roomHeight }, { roomWidth, 46.f });
                            if (roomBounds.contains(mouse))
                            {
                                statusMessage = "Joining room...";
                                if (client.joinRoom(availableRooms[i].joinId, nickname))
                                {
                                    isHost = false;
                                    client.fetchLobby(latestLobby);
                                    state = AppState::WaitingRoom;
                                    statusMessage = "Joined room. Waiting for host to start.";
                                    waitingRoomPollClock.restart();
                                    lobbyPollClock.restart();
                                }
                                else
                                {
                                    statusMessage = client.getLastError();
                                }
                                break;
                            }
                        }
                    }
                }
            }
            else if (state == AppState::WaitingRoom)
            {
                bool startRequested = false;

                if (const auto* keyEvent = event->getIf<sf::Event::KeyPressed>())
                {
                    if (keyEvent->code == sf::Keyboard::Key::Enter)
                    {
                        startRequested = true;
                    }
                }

                if (event->is<sf::Event::MouseButtonPressed>())
                {
                    sf::Vector2f mouse = window.mapPixelToCoords(sf::Mouse::getPosition(window));
                    if (isHost && startButton.getGlobalBounds().contains(mouse))
                    {
                        startRequested = true;
                    }
                }

                if (startRequested && isHost)
                {
                    if (client.startGame())
                    {
                        state = AppState::Playing;
                        playingPollClock.restart();
                        lastObservedStateSignature = makeStateSignature(client.getState());
                        lastObservedCurrentPlayerId = getCurrentPlayerId(client.getState());
                        remoteDeckDrawPlayerId.clear();
                        statusMessage.clear();
                    }
                    else
                    {
                        statusMessage = client.getLastError();
                    }
                }
            }
            else if (state == AppState::Playing && event->is<sf::Event::KeyPressed>() && !animating && !remoteAnimating)
            {
                const auto* keyEvent = event->getIf<sf::Event::KeyPressed>();
                if (keyEvent != nullptr && keyEvent->code == sf::Keyboard::Key::Escape)
                {
                    const nlohmann::json& gameState = client.getState();
                    if (hasAllowedAction(gameState, "cancel_see_and_swap"))
                    {
                        if (client.cancelSeeAndSwap())
                        {
                            lastObservedStateSignature = makeStateSignature(client.getState());
                            lastObservedCurrentPlayerId = getCurrentPlayerId(client.getState());
                            remoteDeckDrawPlayerId.clear();
                            playingPollClock.restart();
                            giveAndTakeOwnIndex = -1;
                            statusMessage.clear();
                        }
                        else
                        {
                            statusMessage = client.getLastError();
                        }
                    }
                }
            }
            else if (state == AppState::Playing && event->is<sf::Event::MouseButtonPressed>() && !animating && !remoteAnimating)
            {
                sf::Vector2f mouse = window.mapPixelToCoords(sf::Mouse::getPosition(window));
                const nlohmann::json& gameState = client.getState();
                std::optional<nlohmann::json> drawnCard = getDrawnCard(gameState);
                std::vector<nlohmann::json> hand = getPlayerHand(gameState, client.getPlayerId());
                std::vector<nlohmann::json> otherPlayers = getOtherPlayers(gameState, client.getPlayerId());

                if (isActionTargetPhase(gameState))
                {
                    bool actionWorked = false;
                    bool handledActionClick = false;

                    if (hasAllowedAction(gameState, "cancel_see_and_swap") && discardBox.getGlobalBounds().contains(mouse))
                    {
                        handledActionClick = true;
                        actionWorked = client.cancelSeeAndSwap();
                    }
                    else if (hasAllowedAction(gameState, "resolve_give_and_take"))
                    {
                        for (int i = 0; i < static_cast<int>(handBoxes.size()) && i < static_cast<int>(hand.size()); i++)
                        {
                            if (handBoxes[i].getGlobalBounds().contains(mouse))
                            {
                                giveAndTakeOwnIndex = i;
                                statusMessage = "Choose an opponent card to take.";
                                handledActionClick = true;
                                break;
                            }
                        }

                        if (!handledActionClick)
                        {
                            for (int playerIndex = 0; playerIndex < static_cast<int>(otherPlayers.size()) && !handledActionClick; playerIndex++)
                            {
                                std::string targetPlayerId = jsonString(otherPlayers[playerIndex], { "player_id", "playerId", "id" });
                                std::vector<nlohmann::json> otherCards = getCardsFromPlayer(otherPlayers[playerIndex]);
                                int cardCount = std::min(otherCards.empty() ? 4 : static_cast<int>(otherCards.size()), 4);

                                for (int cardIndex = 0; cardIndex < cardCount; cardIndex++)
                                {
                                    if (getOpponentCardBounds(playerIndex, cardIndex).contains(mouse))
                                    {
                                        handledActionClick = true;
                                        if (giveAndTakeOwnIndex < 0)
                                        {
                                            statusMessage = "Choose one of your cards first.";
                                        }
                                        else if (!targetPlayerId.empty())
                                        {
                                            actionWorked = client.resolveGiveAndTake(giveAndTakeOwnIndex, targetPlayerId, cardIndex);
                                        }
                                        break;
                                    }
                                }
                            }
                        }
                    }
                    else if (hasAllowedAction(gameState, "reveal_see_and_swap_target"))
                    {
                        for (int i = 0; i < static_cast<int>(handBoxes.size()) && i < static_cast<int>(hand.size()); i++)
                        {
                            if (handBoxes[i].getGlobalBounds().contains(mouse))
                            {
                                handledActionClick = true;
                                actionWorked = client.revealSeeAndSwapTarget(client.getPlayerId(), i);
                                break;
                            }
                        }

                        for (int playerIndex = 0; playerIndex < static_cast<int>(otherPlayers.size()) && !handledActionClick; playerIndex++)
                        {
                            std::string targetPlayerId = jsonString(otherPlayers[playerIndex], { "player_id", "playerId", "id" });
                            std::vector<nlohmann::json> otherCards = getCardsFromPlayer(otherPlayers[playerIndex]);
                            int cardCount = std::min(otherCards.empty() ? 4 : static_cast<int>(otherCards.size()), 4);

                            for (int cardIndex = 0; cardIndex < cardCount; cardIndex++)
                            {
                                if (getOpponentCardBounds(playerIndex, cardIndex).contains(mouse))
                                {
                                    handledActionClick = true;
                                    if (!targetPlayerId.empty())
                                    {
                                        actionWorked = client.revealSeeAndSwapTarget(targetPlayerId, cardIndex);
                                    }
                                    break;
                                }
                            }
                        }
                    }
                    else if (hasAllowedAction(gameState, "resolve_see_and_swap"))
                    {
                        for (int i = 0; i < static_cast<int>(handBoxes.size()) && i < static_cast<int>(hand.size()); i++)
                        {
                            if (handBoxes[i].getGlobalBounds().contains(mouse))
                            {
                                handledActionClick = true;
                                actionWorked = client.resolveSeeAndSwap(i);
                                break;
                            }
                        }
                    }

                    if (actionWorked)
                    {
                        lastObservedStateSignature = makeStateSignature(client.getState());
                        lastObservedCurrentPlayerId = getCurrentPlayerId(client.getState());
                        remoteDeckDrawPlayerId.clear();
                        playingPollClock.restart();
                        waitingForDiscardSwap = false;
                        giveAndTakeOwnIndex = -1;
                        statusMessage.clear();
                    }
                    else if (handledActionClick && statusMessage.empty())
                    {
                        statusMessage = client.getLastError();
                    }
                    else if (!handledActionClick)
                    {
                        std::string actionType = getPendingActionType(gameState);
                        if (hasAllowedAction(gameState, "resolve_see_and_swap"))
                        {
                            statusMessage = "Choose one of your cards to swap, or click Discard to cancel.";
                        }
                        else if (actionType == "give_and_take" || hasAllowedAction(gameState, "resolve_give_and_take"))
                        {
                            statusMessage = giveAndTakeOwnIndex < 0 ? "Choose one of your cards first." : "Choose an opponent card to take.";
                        }
                        else
                        {
                            statusMessage = "Choose a card target for the action.";
                        }
                    }
                }
                else if (sf::FloatRect(deckPos, CARD_SIZE).contains(mouse) && !drawnCard.has_value())
                {
                    if (!hasAllowedAction(gameState, "draw_from_deck"))
                    {
                        statusMessage = "You cannot draw from deck right now.";
                    }
                    else if (client.drawFromDeck())
                    {
                        lastObservedStateSignature = makeStateSignature(client.getState());
                        lastObservedCurrentPlayerId = getCurrentPlayerId(client.getState());
                        remoteDeckDrawPlayerId.clear();
                        playingPollClock.restart();
                        statusMessage.clear();
                        waitingForDiscardSwap = false;
                        giveAndTakeOwnIndex = -1;
                    }
                    else
                    {
                        statusMessage = client.getLastError();
                    }
                }
                else if (discardBox.getGlobalBounds().contains(mouse))
                {
                    if (drawnCard.has_value())
                    {
                        if (!hasAllowedAction(gameState, "drop_drawn_card"))
                        {
                            statusMessage = "You cannot drop this card right now.";
                        }
                        else if (client.dropDrawnCard())
                        {
                            lastObservedStateSignature = makeStateSignature(client.getState());
                            lastObservedCurrentPlayerId = getCurrentPlayerId(client.getState());
                            remoteDeckDrawPlayerId.clear();
                            playingPollClock.restart();
                            statusMessage.clear();
                            waitingForDiscardSwap = false;
                            giveAndTakeOwnIndex = -1;
                        }
                        else
                        {
                            statusMessage = client.getLastError();
                        }
                    }
                    else
                    {
                        if (!hasAllowedAction(gameState, "take_discard_and_swap"))
                        {
                            statusMessage = "You cannot take discard right now.";
                        }
                        else
                        {
                            waitingForDiscardSwap = true;
                            statusMessage = "Click one of your cards to swap with discard.";
                        }
                    }
                }
                else
                {
                    for (int i = 0; i < static_cast<int>(handBoxes.size()); i++)
                    {
                        if (!handBoxes[i].getGlobalBounds().contains(mouse))
                        {
                            continue;
                        }

                        bool actionWorked = false;
                        if (drawnCard.has_value())
                        {
                            if (hasAllowedAction(gameState, "swap_drawn_with_own_card"))
                            {
                                actionWorked = client.swapDrawnWithOwnCard(i);
                            }
                            else
                            {
                                statusMessage = "You cannot swap the drawn card right now.";
                            }
                        }
                        else if (waitingForDiscardSwap)
                        {
                            if (hasAllowedAction(gameState, "take_discard_and_swap"))
                            {
                                actionWorked = client.takeDiscardAndSwap(i);
                            }
                            else
                            {
                                statusMessage = "You cannot take discard right now.";
                                waitingForDiscardSwap = false;
                            }
                        }

                        if (actionWorked)
                        {
                            lastObservedStateSignature = makeStateSignature(client.getState());
                            lastObservedCurrentPlayerId = getCurrentPlayerId(client.getState());
                            remoteDeckDrawPlayerId.clear();
                            playingPollClock.restart();
                            animating = true;
                            animTargetIndex = i;
                            animClock.restart();
                            setBoxTexture(ghostToHand, textures, drawnCard.has_value() ? cardFileName(*drawnCard) : "back");
                            ghostToHand.setPosition(drawnPos);
                            setBoxTexture(ghostToDiscard, textures, "back");
                            ghostToDiscard.setPosition(handBoxes[i].getPosition());
                            waitingForDiscardSwap = false;
                            giveAndTakeOwnIndex = -1;
                            statusMessage.clear();
                        }
                        else if (drawnCard.has_value() || waitingForDiscardSwap)
                        {
                            statusMessage = client.getLastError();
                        }

                        break;
                    }
                }
            }
        }

        if (state == AppState::WaitingRoom && !isHost && waitingRoomPollClock.getElapsedTime().asSeconds() >= 2.0f)
        {
            waitingRoomPollClock.restart();
            if (client.refreshState() && gameHasStarted(client.getState(), client.getPlayerId()))
            {
                state = AppState::Playing;
                playingPollClock.restart();
                lastObservedStateSignature = makeStateSignature(client.getState());
                lastObservedCurrentPlayerId = getCurrentPlayerId(client.getState());
                remoteDeckDrawPlayerId.clear();
                statusMessage.clear();
            }
        }

        if (state == AppState::WaitingRoom && lobbyPollClock.getElapsedTime().asSeconds() >= 2.0f)
        {
            lobbyPollClock.restart();
            nlohmann::json lobbyJson;
            if (client.fetchLobby(lobbyJson))
            {
                latestLobby = lobbyJson;
            }
        }

        if (state == AppState::Playing && playingPollClock.getElapsedTime().asSeconds() >= 2.0f)
        {
            nlohmann::json previousState = client.getState();
            std::string previousSignature = lastObservedStateSignature;
            std::string previousCurrentPlayerId = lastObservedCurrentPlayerId;

            playingPollClock.restart();
            if (client.refreshState())
            {
                const nlohmann::json& refreshedState = client.getState();
                std::string newSignature = makeStateSignature(refreshedState);
                std::string newCurrentPlayerId = getCurrentPlayerId(refreshedState);
                std::string previousDiscardName = getDiscardCardName(previousState);
                std::string newDiscardName = getDiscardCardName(refreshedState);
                bool discardChanged = !newDiscardName.empty() && previousDiscardName != newDiscardName;
                std::optional<int> previousDrawPileCount = getDrawPileCount(previousState);
                std::optional<int> newDrawPileCount = getDrawPileCount(refreshedState);
                bool drawPileChanged = previousDrawPileCount.has_value() &&
                    newDrawPileCount.has_value() &&
                    previousDrawPileCount.value() != newDrawPileCount.value();
                bool hadPendingDrawnCard = getDrawnCard(previousState).has_value();
                bool remoteStateChanged = !previousSignature.empty() &&
                    previousSignature != newSignature &&
                    previousCurrentPlayerId != client.getPlayerId();

                if (remoteStateChanged && drawPileChanged && !discardChanged)
                {
                    remoteDeckDrawPlayerId = previousCurrentPlayerId;
                }
                else if (!remoteDeckDrawPlayerId.empty() && newCurrentPlayerId != remoteDeckDrawPlayerId && previousCurrentPlayerId != remoteDeckDrawPlayerId)
                {
                    remoteDeckDrawPlayerId.clear();
                }

                if (remoteStateChanged && discardChanged && !animating)
                {
                    int opponentRow = 0;
                    std::vector<nlohmann::json> otherPlayers = getOtherPlayers(refreshedState, client.getPlayerId());
                    for (int i = 0; i < static_cast<int>(otherPlayers.size()); i++)
                    {
                        std::string otherId = jsonString(otherPlayers[i], { "player_id", "playerId", "id" });
                        if (otherId == previousCurrentPlayerId)
                        {
                            opponentRow = i;
                            break;
                        }
                    }

                    sf::FloatRect sourceCard = getOpponentCardBounds(opponentRow, 3);
                    if (drawPileChanged || remoteDeckDrawPlayerId == previousCurrentPlayerId)
                    {
                        remoteAnimStart = deckPos;
                    }
                    else if (hadPendingDrawnCard)
                    {
                        remoteAnimStart = drawnPos;
                    }
                    else
                    {
                        remoteAnimStart = sourceCard.position;
                    }
                    remoteAnimEnd = discardPos;
                    std::optional<nlohmann::json> remoteDiscardCard = getDiscardCard(refreshedState);
                    setBoxTexture(remoteGhost, textures, remoteDiscardCard.has_value() ? cardFileName(*remoteDiscardCard) : "back");
                    remoteGhost.setPosition(remoteAnimStart);
                    remoteGhost.setFillColor(sf::Color(255, 255, 255, 230));
                    remoteGhost.setOutlineThickness(3.f);
                    remoteGhost.setOutlineColor(ACCENT_GOLD);
                    remoteAnimating = true;
                    remoteAnimClock.restart();
                    remoteDeckDrawPlayerId.clear();
                }

                lastObservedStateSignature = newSignature;
                lastObservedCurrentPlayerId = newCurrentPlayerId;
                if (!isActionTargetPhase(refreshedState))
                {
                    giveAndTakeOwnIndex = -1;
                }
                statusMessage.clear();
            }
        }

        if (state == AppState::Playing && animating)
        {
            float t = animClock.getElapsedTime().asSeconds() / animDuration;
            if (t >= 1.0f)
            {
                animating = false;
                animTargetIndex = -1;
            }
            else if (animTargetIndex >= 0)
            {
                float easedT = easeInOut(t);
                ghostToHand.setPosition(lerp(drawnPos, handBoxes[animTargetIndex].getPosition(), easedT));
                ghostToDiscard.setPosition(lerp(handBoxes[animTargetIndex].getPosition(), discardPos, easedT));
            }
        }

        if (state == AppState::Playing && remoteAnimating)
        {
            float t = remoteAnimClock.getElapsedTime().asSeconds() / remoteAnimDuration;
            if (t >= 1.0f)
            {
                remoteAnimating = false;
            }
            else
            {
                float easedT = easeInOut(t);
                remoteGhost.setPosition(lerp(remoteAnimStart, remoteAnimEnd, easedT));
                int alpha = static_cast<int>(170.f + 85.f * (1.f - std::abs((easedT - 0.5f) * 2.f)));
                remoteGhost.setFillColor(sf::Color(255, 255, 255, static_cast<std::uint8_t>(alpha)));
            }
        }

        sf::Vector2f mouse = window.mapPixelToCoords(sf::Mouse::getPosition(window));
        window.clear(TABLE_BACKGROUND);

        sf::RectangleShape topRail({ 1000.f, 22.f });
        topRail.setPosition({ 0.f, 0.f });
        topRail.setFillColor(TABLE_RAIL);
        window.draw(topRail);

        sf::RectangleShape bottomRail({ 1000.f, 28.f });
        bottomRail.setPosition({ 0.f, 672.f });
        bottomRail.setFillColor(TABLE_RAIL);
        window.draw(bottomRail);

        if (state == AppState::Menu)
        {
            mainMenu.draw(window);
        }
        else if (state == AppState::Joining)
        {
            drawPanel(window, { 245.f, 160.f }, { 510.f, 260.f }, TABLE_PANEL_DARK, sf::Color(72, 126, 93));
            window.draw(joiningTitle);
            fitTextToWidth(joiningText, 410.f, 18);
            window.draw(joiningText);
            window.draw(joiningHint);
        }
        else if (state == AppState::RoomSelect)
        {
            roomSelectInfo.setCharacterSize(22);
            roomSelectInfo.setString("Player: " + nickname + "\nSelect a public room or create a new one.");
            fitTextToBox(roomSelectInfo, { 560.f, 58.f }, 15);

            drawPanel(window, { 180.f, 48.f }, { 640.f, 545.f }, TABLE_PANEL_DARK, sf::Color(72, 126, 93));
            window.draw(roomSelectTitle);
            window.draw(roomSelectInfo);
            drawButton(window, createRoomButton, createRoomText, createRoomButton.getGlobalBounds().contains(mouse), ACCENT_GREEN, sf::Color(57, 166, 111));
            drawButton(window, refreshRoomsButton, refreshRoomsText, refreshRoomsButton.getGlobalBounds().contains(mouse), ACCENT_BLUE, sf::Color(82, 121, 190));

            if (availableRooms.empty())
            {
                sf::Text noRoomsText(font, "No public rooms found.", 24);
                noRoomsText.setFillColor(TEXT_SECONDARY);
                noRoomsText.setPosition({ 420.f, 325.f });
                window.draw(noRoomsText);
            }
            else
            {
                const float firstRoomY = 305.f;
                const float roomHeight = 58.f;
                const float roomX = 250.f;
                const float roomWidth = 600.f;
                int roomsToShow = std::min(static_cast<int>(availableRooms.size()), 7);

                for (int i = 0; i < roomsToShow; i++)
                {
                    sf::RectangleShape roomButton({ roomWidth, 46.f });
                    roomButton.setPosition({ roomX, firstRoomY + i * roomHeight });
                    bool roomHovered = roomButton.getGlobalBounds().contains(mouse);
                    roomButton.setFillColor(roomHovered ? sf::Color(70, 118, 174) : sf::Color(46, 88, 137));
                    roomButton.setOutlineThickness(2.f);
                    roomButton.setOutlineColor(roomHovered ? ACCENT_GOLD : sf::Color(91, 134, 161));

                    sf::Text roomText(font, availableRooms[i].displayText, 22);
                    roomText.setFillColor(TEXT_PRIMARY);
                    fitTextToWidth(roomText, roomWidth - 36.f, 14);
                    roomText.setPosition({ roomX + 18.f, firstRoomY + i * roomHeight + 10.f });

                    window.draw(roomButton);
                    window.draw(roomText);
                }
            }
        }
        else if (state == AppState::WaitingRoom)
        {
            drawPanel(window, { 235.f, 70.f }, { 530.f, 500.f }, TABLE_PANEL_DARK, sf::Color(72, 126, 93));
            if (isHost)
            {
                std::string playerListText;
                std::vector<std::string> lobbyNames = readLobbyPlayerNames(latestLobby);
                for (const std::string& playerName : lobbyNames)
                {
                    playerListText += "\n- " + playerName;
                }

                waitingInfo.setString(
                    "Room code: " + client.getRoomCode() + "\n" +
                    "Players: " + getLobbyPlayerCountText(latestLobby) + playerListText + "\n\n" +
                    "Press Enter or click Start when ready."
                );
            }
            else
            {
                waitingInfo.setString(
                    "Room code: " + client.getRoomCode() + "\n" +
                    "Player: " + nickname + "\n\n" +
                    "Waiting for host to start..."
                );
            }
            waitingInfo.setCharacterSize(26);
            fitTextToBox(waitingInfo, { 455.f, 205.f }, 16);

            window.draw(waitingTitle);
            window.draw(waitingInfo);

            if (isHost)
            {
                drawButton(window, startButton, startButtonText, startButton.getGlobalBounds().contains(mouse), ACCENT_BLUE, sf::Color(82, 121, 190));
            }
        }
        else if (state == AppState::Playing)
        {
            const nlohmann::json& gameState = client.getState();
            std::vector<nlohmann::json> hand = getPlayerHand(gameState, client.getPlayerId());
            std::vector<nlohmann::json> otherPlayers = getOtherPlayers(gameState, client.getPlayerId());
            std::optional<nlohmann::json> discardCard = getDiscardCard(gameState);
            std::optional<nlohmann::json> drawnCard = getDrawnCard(gameState);

            drawPanel(window, { 16.f, 52.f }, { 468.f, 466.f }, TABLE_PANEL_DARK, sf::Color(56, 112, 79));
            drawPanel(window, { 506.f, 76.f }, { 458.f, 230.f }, TABLE_PANEL, sf::Color(76, 138, 93));
            drawPanel(window, { 250.f, 490.f }, { 522.f, 174.f }, TABLE_PANEL_DARK, sf::Color(84, 141, 91));

            setFittedText(currentTurnLabel, getCurrentTurnText(gameState), 24, 360.f, 16);
            drawPanel(window, { 300.f, 16.f }, { 400.f, 45.f }, sf::Color(22, 53, 48), sf::Color(89, 132, 83));
            window.draw(currentTurnLabel);

            bool actionTargetPhase = isActionTargetPhase(gameState);
            bool canGiveAndTake = hasAllowedAction(gameState, "resolve_give_and_take");
            bool canRevealSeeAndSwap = hasAllowedAction(gameState, "reveal_see_and_swap_target");
            bool canResolveSeeAndSwap = hasAllowedAction(gameState, "resolve_see_and_swap");

            for (int playerIndex = 0; playerIndex < static_cast<int>(otherPlayers.size()); playerIndex++)
            {
                const nlohmann::json& otherPlayer = otherPlayers[playerIndex];
                std::string otherName = jsonString(otherPlayer, { "nickname", "name", "player_id", "id" });
                if (otherName.empty())
                {
                    otherName = "Player";
                }

                float rowY = getOpponentCardBounds(playerIndex, 0).position.y;
                sf::Text otherNameText(font, otherName, 18);
                otherNameText.setFillColor(TEXT_SECONDARY);
                fitTextToWidth(otherNameText, 420.f, 13);
                otherNameText.setPosition({ 36.f, rowY - 24.f });
                window.draw(otherNameText);

                std::vector<nlohmann::json> otherCards = getCardsFromPlayer(otherPlayer);
                int cardCount = otherCards.empty() ? 4 : static_cast<int>(otherCards.size());
                cardCount = std::min(cardCount, 4);

                for (int cardIndex = 0; cardIndex < cardCount; cardIndex++)
                {
                    sf::RectangleShape otherCardBox(OPPONENT_CARD_SIZE);
                    otherCardBox.setPosition(getOpponentCardBounds(playerIndex, cardIndex).position);

                    if (cardIndex < static_cast<int>(otherCards.size()))
                    {
                        setBoxTexture(otherCardBox, textures, cardFileName(otherCards[cardIndex]));
                    }
                    else
                    {
                        setBoxTexture(otherCardBox, textures, "back");
                    }

                    bool actionableOpponent = canRevealSeeAndSwap || (canGiveAndTake && giveAndTakeOwnIndex >= 0);
                    styleCardSlot(otherCardBox, otherCardBox.getGlobalBounds().contains(mouse), actionableOpponent);
                    window.draw(otherCardBox);
                }
            }

            bool canSwapDrawn = drawnCard.has_value() && hasAllowedAction(gameState, "swap_drawn_with_own_card");
            bool canTakeDiscard = waitingForDiscardSwap && hasAllowedAction(gameState, "take_discard_and_swap");
            for (int i = 0; i < static_cast<int>(handBoxes.size()); i++)
            {
                if (animating && i == animTargetIndex)
                {
                    continue;
                }

                if (i < static_cast<int>(hand.size()))
                {
                    setBoxTexture(handBoxes[i], textures, cardFileName(hand[i]));
                }
                else
                {
                    handBoxes[i].setTexture(nullptr);
                    handBoxes[i].setFillColor(sf::Color(50, 50, 50));
                }

                bool ownCardActionable = canSwapDrawn || canTakeDiscard || canGiveAndTake || canRevealSeeAndSwap || canResolveSeeAndSwap;
                styleCardSlot(handBoxes[i], handBoxes[i].getGlobalBounds().contains(mouse), ownCardActionable);
                if (canGiveAndTake && i == giveAndTakeOwnIndex)
                {
                    handBoxes[i].setOutlineThickness(5.f);
                    handBoxes[i].setOutlineColor(ACCENT_GOLD);
                }
                window.draw(handBoxes[i]);
            }

            if (discardCard.has_value())
            {
                setBoxTexture(discardBox, textures, cardFileName(*discardCard));
            }
            else
            {
                discardBox.setTexture(nullptr);
                discardBox.setFillColor(sf::Color(50, 50, 50));
            }
            styleCardSlot(discardBox, discardBox.getGlobalBounds().contains(mouse), drawnCard.has_value() || hasAllowedAction(gameState, "take_discard_and_swap") || hasAllowedAction(gameState, "cancel_see_and_swap"));

            if (drawnCard.has_value())
            {
                setBoxTexture(drawnBox, textures, cardFileName(*drawnCard));
            }
            else
            {
                drawnBox.setTexture(nullptr);
                drawnBox.setFillColor(sf::Color(50, 50, 50));
            }
            styleCardSlot(drawnBox, drawnBox.getGlobalBounds().contains(mouse), drawnCard.has_value());

            window.draw(discardBox);
            window.draw(drawnBox);
            drawCardLabel(window, font, "Discard", discardPos, TEXT_SECONDARY);
            drawCardLabel(window, font, drawnCard.has_value() ? "Drawn card" : "Drawn slot", drawnPos, drawnCard.has_value() ? ACCENT_GOLD : TEXT_SECONDARY);

            for (int i = 0; i < 3; i++)
            {
                sf::RectangleShape deckLayer(CARD_SIZE);
                deckLayer.setTexture(deckBox.getTexture(), true);
                deckLayer.setFillColor(sf::Color::White);
                deckLayer.setPosition({ deckPos.x - (i * 4.f), deckPos.y - (i * 4.f) });
                styleCardSlot(deckLayer, i == 0 && sf::FloatRect(deckPos, CARD_SIZE).contains(mouse), i == 0 && !drawnCard.has_value() && hasAllowedAction(gameState, "draw_from_deck"));
                window.draw(deckLayer);
            }
            drawCardLabel(window, font, "Deck", deckPos, TEXT_SECONDARY);

            setFittedText(playerNameLabel, getPlayerNameFromState(gameState, nickname), 24, 205.f, 15);
            window.draw(playerNameLabel);

            if (actionTargetPhase)
            {
                if (canGiveAndTake)
                {
                    setFittedText(hintText, giveAndTakeOwnIndex < 0 ? "Give And Take: choose one of your cards" : "Give And Take: choose an opponent card", 19, 438.f, 13);
                }
                else if (canRevealSeeAndSwap)
                {
                    setFittedText(hintText, "See And Swap: choose any target card to reveal", 19, 438.f, 13);
                }
                else if (canResolveSeeAndSwap)
                {
                    setFittedText(hintText, "See And Swap: choose one of your cards, or click Discard to cancel", 19, 438.f, 13);
                }
                else
                {
                    setFittedText(hintText, "Choose a target for the action card", 19, 438.f, 13);
                }
            }
            else
            {
                setFittedText(
                    hintText,
                    waitingForDiscardSwap ? "Choose one of your cards to swap with the discard pile" : "Deck draws. Discard drops drawn cards or starts a swap.",
                    19,
                    438.f,
                    13);
            }
            window.draw(hintText);
            sf::Text handLabel(font, "Your cards", 20);
            handLabel.setFillColor(ACCENT_GOLD);
            handLabel.setPosition({ 284.f, 502.f });
            window.draw(handLabel);

            if (animating)
            {
                window.draw(ghostToHand);
                window.draw(ghostToDiscard);
            }

            if (remoteAnimating)
            {
                sf::RectangleShape remoteShadow(CARD_SIZE);
                remoteShadow.setPosition(remoteGhost.getPosition() + sf::Vector2f(8.f, 10.f));
                remoteShadow.setFillColor(sf::Color(0, 0, 0, 80));
                remoteShadow.setOutlineThickness(0.f);
                window.draw(remoteShadow);
                window.draw(remoteGhost);
            }
        }

        if (!statusMessage.empty())
        {
            drawStatusBar(window, font, statusMessage, true);
        }
        else if (!client.getLastError().empty())
        {
            drawStatusBar(window, font, client.getLastError(), true);
        }

        window.display();
    }

    return 0;
}

int main()
{
    try
    {
        return runApp();
    }
    catch (const std::exception& ex)
    {
        showFatalError(ex.what());
    }
    catch (...)
    {
        showFatalError("Unknown fatal error.");
    }

    return -1;
}
