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
#include <array>
#include <vector>
#define NOMINMAX
#include <windows.h>
#include <SFML/Graphics.hpp>
#include "../include/GameClient.h"
#include "../include/MainMenu.h"
#include "../include/TextureManager.h"

const sf::Vector2f CARD_SIZE = { 96.f, 132.f };
const sf::Vector2f OPPONENT_CARD_SIZE = { 68.f, 94.f };
const float PLAYER_CARD_SPACING = 112.f;
const float OPPONENT_CARD_SPACING = 78.f;
const float OPPONENT_ROW_SPACING = 76.f;
const sf::Color TABLE_BACKGROUND = sf::Color(24, 34, 31);
const sf::Color TABLE_PANEL = sf::Color(31, 46, 41);
const sf::Color TABLE_PANEL_DARK = sf::Color(20, 29, 27);
const sf::Color TABLE_RAIL = sf::Color(18, 27, 25);
const sf::Color PANEL_OUTLINE = sf::Color(58, 75, 67);
const sf::Color SLOT_EMPTY = sf::Color(38, 45, 42);
const sf::Color TEXT_PRIMARY = sf::Color(241, 238, 226);
const sf::Color TEXT_SECONDARY = sf::Color(180, 190, 182);
const sf::Color ACCENT_GREEN = sf::Color(85, 146, 109);
const sf::Color ACCENT_BLUE = sf::Color(80, 118, 111);
const sf::Color ACCENT_GOLD = sf::Color(210, 177, 93);
const sf::Color DANGER_TEXT = sf::Color(232, 154, 142);

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
    float menuPollIntervalSecs = 2.0f;
    float gamePollIntervalSecs = 2.0f;
};

struct LobbyPlayerInfo
{
    std::string id;
    std::string name;
    bool isHost = false;
};

struct RemoteCardMotion
{
    sf::RectangleShape card;
    sf::Vector2f start;
    sf::Vector2f end;
    sf::Vector2f startSize = CARD_SIZE;
    sf::Vector2f endSize = CARD_SIZE;
    sf::Vector2f laneOffset = { 0.f, 0.f };
    std::string sourcePlayerId;
    std::string destinationPlayerId;
    int sourceIndex = -1;
    int destinationIndex = -1;
    bool hideHandSlots = false;
    float startedAt = 0.f;
    float duration = 0.62f;
};

struct ClickPulse
{
    sf::Vector2f position;
    float startedAt = 0.f;
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

float approach(float current, float target, float speed, float dt)
{
    float step = speed * dt;
    if (current < target)
    {
        return std::min(current + step, target);
    }

    return std::max(current - step, target);
}

sf::Color withAlpha(sf::Color color, std::uint8_t alpha)
{
    color.a = alpha;
    return color;
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

float jsonPositiveFloat(const nlohmann::json& data, const std::string& fieldName, float defaultValue)
{
    if (data.is_object() && data.contains(fieldName) && data[fieldName].is_number())
    {
        float value = data[fieldName].get<float>();
        if (value > 0.f)
        {
            return value;
        }
    }

    return defaultValue;
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

        config.menuPollIntervalSecs = jsonPositiveFloat(configJson, "menu_poll_interval_secs", config.menuPollIntervalSecs);
        config.gamePollIntervalSecs = jsonPositiveFloat(configJson, "game_poll_interval_secs", config.gamePollIntervalSecs);
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

bool jsonBoolAny(const nlohmann::json& data, const std::vector<std::string>& fieldNames, bool defaultValue = false)
{
    if (!data.is_object())
    {
        return defaultValue;
    }

    for (const std::string& fieldName : fieldNames)
    {
        if (data.contains(fieldName) && data[fieldName].is_boolean())
        {
            return data[fieldName].get<bool>();
        }
    }

    return defaultValue;
}

std::string getLobbyHostPlayerId(const nlohmann::json& lobbyJson)
{
    std::string hostId = jsonString(lobbyJson, {
        "host_player_id", "hostPlayerId", "host_id", "hostId",
        "owner_player_id", "ownerPlayerId", "owner_id", "ownerId",
        "creator_player_id", "creatorPlayerId", "creator_id", "creatorId"
    });

    if (!hostId.empty() || !lobbyJson.is_object())
    {
        return hostId;
    }

    for (const std::string& fieldName : { "host", "host_player", "owner", "creator" })
    {
        if (lobbyJson.contains(fieldName) && lobbyJson[fieldName].is_object())
        {
            hostId = jsonString(lobbyJson[fieldName], { "player_id", "playerId", "id" });
            if (!hostId.empty())
            {
                return hostId;
            }
        }
    }

    if (lobbyJson.contains("room") && lobbyJson["room"].is_object())
    {
        hostId = jsonString(lobbyJson["room"], {
            "host_player_id", "hostPlayerId", "host_id", "hostId",
            "owner_player_id", "ownerPlayerId", "owner_id", "ownerId",
            "creator_player_id", "creatorPlayerId", "creator_id", "creatorId"
        });
    }

    return hostId;
}

std::string getLobbyHostPlayerName(const nlohmann::json& lobbyJson)
{
    std::string hostName = jsonString(lobbyJson, {
        "host_nickname", "hostNickname", "host_name", "hostName",
        "owner_nickname", "ownerNickname", "owner_name", "ownerName",
        "creator_nickname", "creatorNickname", "creator_name", "creatorName"
    });

    if (!hostName.empty() || !lobbyJson.is_object())
    {
        return hostName;
    }

    for (const std::string& fieldName : { "host", "host_player", "owner", "creator" })
    {
        if (lobbyJson.contains(fieldName) && lobbyJson[fieldName].is_object())
        {
            hostName = jsonString(lobbyJson[fieldName], { "nickname", "name", "player_name", "playerName" });
            if (!hostName.empty())
            {
                return hostName;
            }
        }
    }

    return hostName;
}

std::vector<LobbyPlayerInfo> readLobbyPlayers(const nlohmann::json& lobbyJson)
{
    std::vector<LobbyPlayerInfo> players;

    if (!lobbyJson.is_object() || !lobbyJson.contains("players") || !lobbyJson["players"].is_array())
    {
        return players;
    }

    std::string hostId = getLobbyHostPlayerId(lobbyJson);
    std::string hostName = getLobbyHostPlayerName(lobbyJson);
    bool explicitHostFound = false;

    for (const auto& player : lobbyJson["players"])
    {
        std::string id = jsonString(player, { "player_id", "playerId", "id" });
        std::string name = jsonString(player, { "nickname", "name", "player_name", "playerName", "player_id", "id" });
        if (!name.empty())
        {
            bool isHost = jsonBoolAny(player, { "is_host", "isHost", "host", "is_owner", "isOwner", "owner" });
            isHost = isHost || (!hostId.empty() && id == hostId) || (!hostName.empty() && name == hostName);
            explicitHostFound = explicitHostFound || isHost;
            players.push_back({ id, name, isHost });
        }
    }

    if (!explicitHostFound && !players.empty())
    {
        players.front().isHost = true;
    }

    return players;
}

std::vector<std::string> readLobbyPlayerLabels(const nlohmann::json& lobbyJson)
{
    std::vector<std::string> labels;
    for (const LobbyPlayerInfo& player : readLobbyPlayers(lobbyJson))
    {
        labels.push_back(player.name + (player.isHost ? " (host)" : ""));
    }
    return labels;
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
        hasAllowedAction(state, "reveal_own_card") ||
        hasAllowedAction(state, "reveal_opponent_card") ||
        hasAllowedAction(state, "resolve_give_and_take") ||
        hasAllowedAction(state, "reveal_see_and_swap_target") ||
        hasAllowedAction(state, "resolve_see_and_swap") ||
        hasAllowedAction(state, "cancel_see_and_swap");
}

bool isRoundFinished(const nlohmann::json& state)
{
    std::string phase = lowerText(jsonString(state, { "phase", "status" }));
    return phase == "round_finished" || phase == "finished" || phase == "round_complete";
}

bool canCallScrew(const nlohmann::json& state, const std::string& playerId)
{
    if (!state.is_object() || isRoundFinished(state) || isActionTargetPhase(state) || getDrawnCard(state).has_value())
    {
        return false;
    }

    std::string phase = lowerText(jsonString(state, { "phase", "status" }));
    if (phase == "final_turns")
    {
        return false;
    }

    if (hasAllowedAction(state, "call_screw"))
    {
        return true;
    }

    std::string currentPlayerId = jsonString(state, { "current_player_id", "currentPlayerId" });
    return phase == "playing" && !currentPlayerId.empty() && currentPlayerId == playerId;
}

std::string getPendingActionType(const nlohmann::json& state)
{
    if (state.is_object() && state.contains("pending_action") && state["pending_action"].is_object())
    {
        return lowerText(jsonString(state["pending_action"], { "type", "action_type" }));
    }

    return "";
}

std::optional<nlohmann::json> getActionRevealCard(const nlohmann::json& reveal)
{
    if (!reveal.is_object() || !reveal.contains("card") || reveal["card"].is_null())
    {
        return std::nullopt;
    }

    return reveal["card"];
}

std::string getActionRevealOwnerId(const nlohmann::json& reveal)
{
    return jsonString(reveal, { "card_owner_id", "cardOwnerId", "target_player_id", "targetPlayerId", "player_id", "playerId" });
}

std::optional<int> getActionRevealCardIndex(const nlohmann::json& reveal)
{
    return getIntegerField(reveal, { "card_index", "cardIndex", "target_card_index", "targetCardIndex", "own_card_index", "ownCardIndex", "index" });
}

float getActionRevealDuration(const nlohmann::json& reveal)
{
    return jsonPositiveFloat(reveal, "reveal_duration_seconds", 3.f);
}

bool actionRevealMatchesSlot(const nlohmann::json& reveal, const std::string& ownerId, int cardIndex)
{
    if (!reveal.is_object())
    {
        return false;
    }

    std::string revealOwnerId = getActionRevealOwnerId(reveal);
    std::optional<int> revealCardIndex = getActionRevealCardIndex(reveal);
    return !revealOwnerId.empty() && revealOwnerId == ownerId && revealCardIndex.has_value() && revealCardIndex.value() == cardIndex;
}

sf::FloatRect getOpponentCardBounds(int playerIndex, int cardIndex)
{
    float rowY = 112.f + playerIndex * OPPONENT_ROW_SPACING;
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

    if (state.is_object() && state.contains("players"))
    {
        signature += "|players:" + state["players"].dump();
    }

    if (state.is_object() && state.contains("pending_action"))
    {
        signature += "|action:" + state["pending_action"].dump();
    }

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

std::vector<nlohmann::json> getRoundResults(const nlohmann::json& state)
{
    std::vector<nlohmann::json> results;
    if (!state.is_object())
    {
        return results;
    }

    const nlohmann::json* resultArray = nullptr;
    if (state.contains("round_results") && state["round_results"].is_array())
    {
        resultArray = &state["round_results"];
    }
    else if (state.contains("results") && state["results"].is_array())
    {
        resultArray = &state["results"];
    }

    if (resultArray == nullptr)
    {
        return results;
    }

    for (const auto& result : *resultArray)
    {
        results.push_back(result);
    }

    return results;
}

std::vector<nlohmann::json> getDisplayHand(const nlohmann::json& state, const std::string& playerId)
{
    if (isRoundFinished(state))
    {
        for (const auto& result : getRoundResults(state))
        {
            std::string id = jsonString(result, { "player_id", "playerId", "id" });
            if (id == playerId)
            {
                return getCardsFromPlayer(result);
            }
        }
    }

    return getPlayerHand(state, playerId);
}

std::vector<nlohmann::json> getDisplayOtherPlayers(const nlohmann::json& state, const std::string& playerId)
{
    if (isRoundFinished(state))
    {
        std::vector<nlohmann::json> others;
        for (const auto& result : getRoundResults(state))
        {
            std::string id = jsonString(result, { "player_id", "playerId", "id" });
            if (id != playerId)
            {
                others.push_back(result);
            }
        }
        return others;
    }

    return getOtherPlayers(state, playerId);
}

const nlohmann::json& cardPayload(const nlohmann::json& slot)
{
    if (slot.is_object() && slot.contains("card") && slot["card"].is_object())
    {
        return slot["card"];
    }

    return slot;
}

std::string normalizeZoneName(const std::string& zone)
{
    std::string normalized = lowerText(zone);
    if (normalized == "deck" || normalized == "draw_pile")
    {
        return "pile";
    }

    if (normalized == "ground" || normalized == "floor" || normalized == "floor_pile" || normalized == "discard_pile")
    {
        return "discard";
    }

    if (normalized == "drawn" || normalized == "drawn_card" || normalized == "current_drawn_card")
    {
        return "pending_drawn_card";
    }

    return normalized;
}

std::string cardFieldString(const nlohmann::json& slot, const std::vector<std::string>& fieldNames)
{
    const nlohmann::json& card = cardPayload(slot);
    std::string value = jsonString(card, fieldNames);
    if (!value.empty())
    {
        return value;
    }

    if (&card != &slot)
    {
        return jsonString(slot, fieldNames);
    }

    return "";
}

const nlohmann::json* cardLocation(const nlohmann::json& slot, const std::string& fieldName)
{
    const nlohmann::json& card = cardPayload(slot);
    if (card.is_object() && card.contains(fieldName) && card[fieldName].is_object())
    {
        return &card[fieldName];
    }

    if (&card != &slot && slot.is_object() && slot.contains(fieldName) && slot[fieldName].is_object())
    {
        return &slot[fieldName];
    }

    return nullptr;
}

std::string locationZone(const nlohmann::json* location)
{
    if (location == nullptr)
    {
        return "";
    }

    return normalizeZoneName(jsonString(*location, { "zone" }));
}

std::string locationPlayerId(const nlohmann::json* location)
{
    if (location == nullptr)
    {
        return "";
    }

    return jsonString(*location, { "player_id", "playerId", "id" });
}

std::optional<int> locationIndex(const nlohmann::json* location)
{
    if (location == nullptr)
    {
        return std::nullopt;
    }

    return getIntegerField(*location, { "index", "card_index", "position" });
}

std::string locationKey(const nlohmann::json* location, const std::string& fallbackZone)
{
    std::string zone = locationZone(location);
    if (zone.empty())
    {
        zone = normalizeZoneName(fallbackZone);
    }

    std::string key = zone;
    std::string playerId = locationPlayerId(location);
    if (!playerId.empty())
    {
        key += ":" + playerId;
    }

    std::optional<int> index = locationIndex(location);
    if (index.has_value())
    {
        key += "#" + std::to_string(index.value());
    }

    return key;
}

std::string cardSource(const nlohmann::json& slot)
{
    std::string source = normalizeZoneName(cardFieldString(slot, { "source" }));
    if (!source.empty())
    {
        return source;
    }

    return locationZone(cardLocation(slot, "from"));
}

std::string cardDestination(const nlohmann::json& slot)
{
    std::string destination = normalizeZoneName(cardFieldString(slot, { "destination" }));
    if (!destination.empty())
    {
        return destination;
    }

    return locationZone(cardLocation(slot, "to"));
}

const nlohmann::json* cardSwapInfo(const nlohmann::json& slot)
{
    const nlohmann::json& card = cardPayload(slot);
    if (card.is_object() && card.contains("swap_info") && card["swap_info"].is_object())
    {
        return &card["swap_info"];
    }

    return nullptr;
}

int cardSlotIndex(const nlohmann::json& slot, int fallback)
{
    std::optional<int> index = getIntegerField(slot, { "index", "card_index", "position" });
    return index.has_value() ? index.value() : fallback;
}

std::string cardIdentity(const nlohmann::json& slot)
{
    std::string id = cardFieldString(slot, { "id", "card_id", "cardId" });
    if (!id.empty())
    {
        return id;
    }

    return cardFileName(slot) + "|" + cardFieldString(slot, { "type", "card_type" }) + "|" + cardFieldString(slot, { "value", "rank" });
}

std::string cardMovementKey(const nlohmann::json& slot)
{
    const nlohmann::json* from = cardLocation(slot, "from");
    const nlohmann::json* to = cardLocation(slot, "to");
    std::string source = cardSource(slot);
    std::string destination = cardDestination(slot);

    if (from == nullptr && to == nullptr && source.empty() && destination.empty())
    {
        const nlohmann::json* swapInfo = cardSwapInfo(slot);
        if (swapInfo != nullptr)
        {
            std::string fromPlayerId = jsonString(*swapInfo, { "from_player_id", "fromPlayerId" });
            std::string toPlayerId = jsonString(*swapInfo, { "to_player_id", "toPlayerId" });
            std::optional<int> fromIndex = getIntegerField(*swapInfo, { "from_index", "fromIndex" });
            std::optional<int> toIndex = getIntegerField(*swapInfo, { "to_index", "toIndex" });
            if (!fromPlayerId.empty() && !toPlayerId.empty() && fromIndex.has_value() && toIndex.has_value())
            {
                return cardIdentity(slot) + "|hand:" + fromPlayerId + "#" + std::to_string(fromIndex.value()) +
                    "->hand:" + toPlayerId + "#" + std::to_string(toIndex.value());
            }
        }

        return "";
    }

    return cardIdentity(slot) + "|" + locationKey(from, source) + "->" + locationKey(to, destination);
}

std::optional<nlohmann::json> findPlayerCardByIndex(const nlohmann::json& state, const std::string& playerId, int cardIndex)
{
    if (state.is_object() && state.contains("player") && state["player"].is_object())
    {
        const nlohmann::json& player = state["player"];
        std::string id = jsonString(player, { "player_id", "playerId", "id" });
        if (id.empty() || id == playerId)
        {
            std::vector<nlohmann::json> cards = getCardsFromPlayer(player);
            for (int i = 0; i < static_cast<int>(cards.size()); i++)
            {
                if (cardSlotIndex(cards[i], i) == cardIndex)
                {
                    return cards[i];
                }
            }
        }
    }

    if (state.is_object() && state.contains("players") && state["players"].is_array())
    {
        for (const auto& player : state["players"])
        {
            std::string id = jsonString(player, { "player_id", "playerId", "id" });
            if (id != playerId)
            {
                continue;
            }

            std::vector<nlohmann::json> cards = getCardsFromPlayer(player);
            for (int i = 0; i < static_cast<int>(cards.size()); i++)
            {
                if (cardSlotIndex(cards[i], i) == cardIndex)
                {
                    return cards[i];
                }
            }
        }
    }

    return std::nullopt;
}

std::optional<nlohmann::json> findCardAtLocation(const nlohmann::json& state, const nlohmann::json* location, const std::string& fallbackZone)
{
    std::string zone = locationZone(location);
    if (zone.empty())
    {
        zone = normalizeZoneName(fallbackZone);
    }

    if (zone == "hand")
    {
        std::string playerId = locationPlayerId(location);
        std::optional<int> index = locationIndex(location);
        if (!playerId.empty() && index.has_value())
        {
            return findPlayerCardByIndex(state, playerId, index.value());
        }
    }

    if (zone == "discard")
    {
        return getDiscardCard(state);
    }

    if (zone == "pending_drawn_card")
    {
        return getDrawnCard(state);
    }

    return std::nullopt;
}

std::optional<nlohmann::json> findCardAtMovementDestination(const nlohmann::json& state, const nlohmann::json& cardSlot)
{
    return findCardAtLocation(state, cardLocation(cardSlot, "to"), cardDestination(cardSlot));
}

void addMovedCardSlot(std::vector<nlohmann::json>& movedCards, const nlohmann::json& slot)
{
    if (!cardMovementKey(slot).empty())
    {
        movedCards.push_back(slot);
    }
}

std::vector<nlohmann::json> collectMovedCardSlots(const nlohmann::json& state, const std::string& localPlayerId)
{
    std::vector<nlohmann::json> movedCards;

    std::optional<nlohmann::json> discardCard = getDiscardCard(state);
    if (discardCard.has_value())
    {
        addMovedCardSlot(movedCards, *discardCard);
    }

    std::optional<nlohmann::json> drawnCard = getDrawnCard(state);
    if (drawnCard.has_value())
    {
        addMovedCardSlot(movedCards, *drawnCard);
    }

    for (const nlohmann::json& card : getPlayerHand(state, localPlayerId))
    {
        addMovedCardSlot(movedCards, card);
    }

    for (const nlohmann::json& otherPlayer : getOtherPlayers(state, localPlayerId))
    {
        for (const nlohmann::json& card : getCardsFromPlayer(otherPlayer))
        {
            addMovedCardSlot(movedCards, card);
        }
    }

    return movedCards;
}

int getOpponentRowForPlayer(const nlohmann::json& state, const std::string& localPlayerId, const std::string& playerId)
{
    std::vector<nlohmann::json> otherPlayers = getOtherPlayers(state, localPlayerId);
    for (int i = 0; i < static_cast<int>(otherPlayers.size()); i++)
    {
        std::string otherId = jsonString(otherPlayers[i], { "player_id", "playerId", "id" });
        if (otherId == playerId)
        {
            return i;
        }
    }

    return -1;
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
        box.setFillColor(sf::Color(122, 58, 52));
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
    sf::RectangleShape shadow(size);
    shadow.setPosition(position + sf::Vector2f(5.f, 7.f));
    shadow.setFillColor(sf::Color(0, 0, 0, 46));
    window.draw(shadow);

    sf::RectangleShape panel(size);
    panel.setPosition(position);
    panel.setFillColor(fill);
    panel.setOutlineThickness(1.f);
    panel.setOutlineColor(outline);
    window.draw(panel);
}

void drawButton(sf::RenderWindow& window, sf::RectangleShape& button, sf::Text& label, bool hovered, sf::Color normal, sf::Color hover)
{
    bool pressed = hovered && sf::Mouse::isButtonPressed(sf::Mouse::Button::Left);
    sf::Vector2f originalPosition = button.getPosition();
    sf::RectangleShape shadow(button.getSize());
    shadow.setPosition(originalPosition + sf::Vector2f(0.f, pressed ? 3.f : 6.f));
    shadow.setFillColor(sf::Color(0, 0, 0, hovered ? 74 : 48));
    window.draw(shadow);

    button.setPosition(originalPosition + sf::Vector2f(0.f, hovered && !pressed ? -2.f : 0.f));
    button.setFillColor(pressed ? sf::Color(
        static_cast<std::uint8_t>(std::max(0, hover.r - 24)),
        static_cast<std::uint8_t>(std::max(0, hover.g - 24)),
        static_cast<std::uint8_t>(std::max(0, hover.b - 24))) : (hovered ? hover : normal));
    button.setOutlineThickness(hovered ? 2.f : 1.f);
    button.setOutlineColor(hovered ? ACCENT_GOLD : PANEL_OUTLINE);
    fitTextToWidth(label, button.getSize().x - 28.f, 16);
    centerText(label, button);
    window.draw(button);
    window.draw(label);
    button.setPosition(originalPosition);
}

void drawTooltip(sf::RenderWindow& window, sf::Font& font, const std::string& message, sf::Vector2f position, float amount)
{
    if (amount <= 0.02f)
    {
        return;
    }

    amount = easeInOut(amount);
    std::uint8_t alpha = static_cast<std::uint8_t>(220.f * amount);
    sf::Vector2f size = { 212.f, 42.f };
    sf::Vector2f offset = { 0.f, 10.f * (1.f - amount) };

    sf::RectangleShape shadow(size);
    shadow.setPosition(position + offset + sf::Vector2f(4.f, 6.f));
    shadow.setFillColor(sf::Color(0, 0, 0, static_cast<std::uint8_t>(70.f * amount)));
    window.draw(shadow);

    sf::RectangleShape tooltip(size);
    tooltip.setPosition(position + offset);
    tooltip.setFillColor(withAlpha(TABLE_PANEL_DARK, alpha));
    tooltip.setOutlineThickness(1.f);
    tooltip.setOutlineColor(withAlpha(ACCENT_GOLD, static_cast<std::uint8_t>(180.f * amount)));
    window.draw(tooltip);

    sf::Text text(font, message, 17);
    text.setFillColor(withAlpha(TEXT_PRIMARY, static_cast<std::uint8_t>(255.f * amount)));
    fitTextToWidth(text, size.x - 24.f, 12);
    text.setPosition(position + offset + sf::Vector2f(12.f, 11.f));
    window.draw(text);
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
    box.setOutlineThickness(actionable ? 3.f : 1.f);
    if (hovered && actionable)
    {
        box.setOutlineColor(ACCENT_GOLD);
    }
    else if (actionable)
    {
        box.setOutlineColor(ACCENT_GREEN);
    }
    else if (hovered)
    {
        box.setOutlineColor(TEXT_SECONDARY);
    }
    else
    {
        box.setOutlineColor(PANEL_OUTLINE);
    }
}

void drawCardVisual(sf::RenderWindow& window, const sf::RectangleShape& baseBox, bool hovered, bool actionable, bool dimmed, bool selected, float hoverAmount, float pressAmount, float timeSeconds)
{
    sf::Vector2f basePosition = baseBox.getPosition();
    sf::Vector2f baseSize = baseBox.getSize();
    float lift = hovered ? 8.f * hoverAmount : 0.f;
    float scale = 1.f + 0.055f * hoverAmount - 0.035f * pressAmount;
    float glowPulse = 0.65f + 0.35f * std::sin(timeSeconds * 5.5f);

    if (actionable || selected)
    {
        sf::RectangleShape glow(baseSize + sf::Vector2f(14.f, 14.f));
        glow.setPosition(basePosition - sf::Vector2f(7.f, 7.f + lift));
        glow.setFillColor(sf::Color::Transparent);
        glow.setOutlineThickness(selected ? 5.f : 3.f);
        glow.setOutlineColor(withAlpha(selected ? ACCENT_GOLD : ACCENT_GREEN, static_cast<std::uint8_t>(90 + 90 * glowPulse)));
        window.draw(glow);
    }

    sf::RectangleShape shadow(baseSize);
    shadow.setPosition(basePosition + sf::Vector2f(7.f + 3.f * hoverAmount, 10.f + 6.f * hoverAmount));
    shadow.setFillColor(sf::Color(0, 0, 0, static_cast<std::uint8_t>(dimmed ? 54 : 92)));
    window.draw(shadow);

    sf::RectangleShape card = baseBox;
    card.setOrigin({ baseSize.x / 2.f, baseSize.y / 2.f });
    card.setPosition(basePosition + sf::Vector2f(baseSize.x / 2.f, baseSize.y / 2.f - lift + 3.f * pressAmount));
    card.setScale({ scale, scale });
    if (dimmed)
    {
        card.setFillColor(withAlpha(card.getFillColor(), 132));
        card.setOutlineColor(withAlpha(card.getOutlineColor(), 96));
    }
    window.draw(card);
}

void drawClickPulses(sf::RenderWindow& window, std::vector<ClickPulse>& pulses, float now)
{
    for (const ClickPulse& pulse : pulses)
    {
        float t = clamp01((now - pulse.startedAt) / 0.32f);
        float eased = easeInOut(t);
        sf::CircleShape ring(14.f + 28.f * eased);
        ring.setOrigin({ ring.getRadius(), ring.getRadius() });
        ring.setPosition(pulse.position);
        ring.setFillColor(sf::Color::Transparent);
        ring.setOutlineThickness(2.f);
        ring.setOutlineColor(withAlpha(ACCENT_GOLD, static_cast<std::uint8_t>(150.f * (1.f - t))));
        window.draw(ring);
    }

    pulses.erase(
        std::remove_if(pulses.begin(), pulses.end(), [now](const ClickPulse& pulse)
            {
                return now - pulse.startedAt >= 0.32f;
            }),
        pulses.end());
}

void drawStatusBar(sf::RenderWindow& window, sf::Font& font, const std::string& message, bool isError)
{
    if (message.empty())
    {
        return;
    }

    drawPanel(window, { 24.f, 626.f }, { 952.f, 46.f }, TABLE_PANEL_DARK, isError ? DANGER_TEXT : PANEL_OUTLINE);
    sf::Text text(font, message, 20);
    text.setFillColor(isError ? DANGER_TEXT : TEXT_PRIMARY);
    fitTextToWidth(text, 912.f, 14);
    text.setPosition({ 42.f, 638.f });
    window.draw(text);
}

void drawActionRevealOverlay(sf::RenderWindow& window, sf::Font& font, TextureManager& textures, const nlohmann::json& reveal, float elapsedSeconds, float durationSeconds)
{
    std::optional<nlohmann::json> revealedCard = getActionRevealCard(reveal);
    if (!revealedCard.has_value())
    {
        return;
    }

    float remainingSeconds = std::max(0.f, durationSeconds - elapsedSeconds);
    drawPanel(window, { 416.f, 130.f }, { 172.f, 238.f }, TABLE_PANEL_DARK, ACCENT_GOLD);

    sf::Text title(font, "Peek", 22);
    title.setFillColor(ACCENT_GOLD);
    sf::FloatRect titleBounds = title.getLocalBounds();
    title.setOrigin({ titleBounds.position.x + titleBounds.size.x / 2.f, titleBounds.position.y });
    title.setPosition({ 502.f, 146.f });
    window.draw(title);

    sf::RectangleShape card({ 124.f, 171.f });
    card.setPosition({ 440.f, 178.f });
    setBoxTexture(card, textures, cardFileName(revealedCard.value()));
    card.setOutlineThickness(3.f);
    card.setOutlineColor(ACCENT_GOLD);
    window.draw(card);

    sf::Text timer(font, std::to_string(static_cast<int>(std::ceil(remainingSeconds))) + "s", 18);
    timer.setFillColor(TEXT_SECONDARY);
    sf::FloatRect timerBounds = timer.getLocalBounds();
    timer.setOrigin({ timerBounds.position.x + timerBounds.size.x / 2.f, timerBounds.position.y });
    timer.setPosition({ 502.f, 346.f });
    window.draw(timer);
}

void drawRoundResultsPanel(sf::RenderWindow& window, sf::Font& font, const nlohmann::json& state, const std::string& localPlayerId)
{
    std::vector<nlohmann::json> results = getRoundResults(state);
    if (results.empty())
    {
        return;
    }

    drawPanel(window, { 506.f, 320.f }, { 458.f, 154.f }, TABLE_PANEL_DARK, ACCENT_GOLD);

    sf::Text title(font, "Round scores", 22);
    title.setFillColor(ACCENT_GOLD);
    title.setPosition({ 526.f, 334.f });
    window.draw(title);

    std::string screwCallerId = jsonString(state, { "screw_caller_id", "screwCallerId" });
    bool screwSuccessKnown = state.is_object() && state.contains("screw_success") && state["screw_success"].is_boolean();
    bool screwSuccess = screwSuccessKnown ? state["screw_success"].get<bool>() : false;

    sf::Text screwText(font, screwSuccessKnown ? (screwSuccess ? "Screw succeeded" : "Screw failed") : "Round finished", 16);
    screwText.setFillColor(screwSuccessKnown && !screwSuccess ? DANGER_TEXT : TEXT_SECONDARY);
    screwText.setPosition({ 744.f, 338.f });
    window.draw(screwText);

    float y = 368.f;
    for (const auto& result : results)
    {
        std::string id = jsonString(result, { "player_id", "playerId", "id" });
        std::string name = jsonString(result, { "nickname", "name", "player_id", "id" });
        if (name.empty())
        {
            name = "Player";
        }
        if (id == localPlayerId)
        {
            name += " (you)";
        }

        std::optional<int> rawScore = getIntegerField(result, { "raw_round_score", "rawRoundScore" });
        std::optional<int> appliedScore = getIntegerField(result, { "applied_round_score", "appliedRoundScore", "round_score", "roundScore", "score" });
        std::optional<int> totalScore = getIntegerField(result, { "total_score", "totalScore" });

        std::string line = name;
        if (id == screwCallerId)
        {
            line += " - Screw";
        }

        line += "    Round: " + (appliedScore.has_value() ? std::to_string(appliedScore.value()) : "?");
        if (rawScore.has_value() && appliedScore.has_value() && rawScore.value() != appliedScore.value())
        {
            line += " (raw " + std::to_string(rawScore.value()) + ")";
        }
        line += "    Total: " + (totalScore.has_value() ? std::to_string(totalScore.value()) : "?");

        sf::Text row(font, line, 16);
        row.setFillColor(id == localPlayerId ? TEXT_PRIMARY : TEXT_SECONDARY);
        fitTextToWidth(row, 416.f, 11);
        row.setPosition({ 526.f, y });
        window.draw(row);
        y += 24.f;
    }
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

void configureDllSearchPath()
{
    std::filesystem::path dllDir = getExecutableDirectory() / "dlls";
    if (std::filesystem::exists(dllDir))
    {
        SetDllDirectoryA(dllDir.string().c_str());
    }
}

std::string assetPath(const std::string& relativePath)
{
    std::filesystem::path bundledPath = getExecutableDirectory() / "dlls" / relativePath;
    if (std::filesystem::exists(bundledPath))
    {
        return bundledPath.string();
    }

    return relativePath;
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
    if (!font.openFromFile(assetPath("assets/fonts/arial.ttf")))
    {
        throw std::runtime_error("Failed to load assets/fonts/arial.ttf. Make sure the assets folder is next to Skrew.exe.");
    }

    std::cout << "Backend: " << appConfig.host << ":" << appConfig.port << "\n";
    std::cout << "Window: " << appConfig.width << "x" << appConfig.height << "\n";
    std::cout << "Polling: menu " << appConfig.menuPollIntervalSecs << "s, game " << appConfig.gamePollIntervalSecs << "s\n";

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
    bool screwConfirmOpen = false;
    int discardSwapOwnIndex = -1;
    int giveAndTakeOwnIndex = -1;
    std::string lastObservedStateSignature;
    std::string lastObservedCurrentPlayerId;
    nlohmann::json activeActionReveal;
    float activeActionRevealDuration = 0.f;

    bool animating = false;
    bool remoteAnimating = false;
    int animTargetIndex = -1;
    sf::Clock animClock;
    sf::Clock roomSelectPollClock;
    sf::Clock waitingRoomPollClock;
    sf::Clock lobbyPollClock;
    sf::Clock playingPollClock;
    sf::Clock remoteAnimClock;
    sf::Clock actionRevealClock;
    sf::Clock frameClock;
    sf::Clock uiClock;
    float animDuration = 0.42f;
    float remoteAnimDuration = 0.62f;
    sf::RectangleShape ghostToHand(CARD_SIZE);
    sf::RectangleShape ghostToDiscard(CARD_SIZE);
    std::vector<RemoteCardMotion> remoteCardMotions;
    std::vector<ClickPulse> clickPulses;
    std::array<float, 4> handHover = { 0.f, 0.f, 0.f, 0.f };
    std::array<float, 3> pileHover = { 0.f, 0.f, 0.f };
    float screwHoverAmount = 0.f;
    std::vector<std::array<float, 4>> opponentHover;

    sf::Vector2f deckPos = { 626.f, 112.f };
    sf::Vector2f discardPos = { 752.f, 112.f };
    sf::Vector2f drawnPos = { 452.f, 292.f };
    sf::Vector2f ghostToHandStart = drawnPos;
    sf::Vector2f ghostToDiscardStart = { 0.f, 0.f };

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

    sf::RectangleShape screwButton({ 140.f, 52.f });
    screwButton.setPosition({ 820.f, 604.f });
    screwButton.setFillColor(sf::Color(158, 69, 58));

    sf::Text screwButtonText(font, "Screw!", 25);
    screwButtonText.setFillColor(sf::Color::White);
    centerText(screwButtonText, screwButton);

    sf::RectangleShape screwConfirmYesButton({ 132.f, 44.f });
    screwConfirmYesButton.setPosition({ 438.f, 390.f });
    screwConfirmYesButton.setFillColor(sf::Color(158, 69, 58));

    sf::Text screwConfirmYesText(font, "Confirm", 20);
    screwConfirmYesText.setFillColor(sf::Color::White);
    centerText(screwConfirmYesText, screwConfirmYesButton);

    sf::RectangleShape screwConfirmNoButton({ 132.f, 44.f });
    screwConfirmNoButton.setPosition({ 590.f, 390.f });
    screwConfirmNoButton.setFillColor(ACCENT_BLUE);

    sf::Text screwConfirmNoText(font, "Cancel", 20);
    screwConfirmNoText.setFillColor(sf::Color::White);
    centerText(screwConfirmNoText, screwConfirmNoButton);

    sf::Text playerNameLabel(font, "", 24);
    playerNameLabel.setFillColor(TEXT_PRIMARY);
    playerNameLabel.setPosition({ 92.f, 560.f });

    sf::Text currentTurnLabel(font, "Current turn: waiting", 24);
    currentTurnLabel.setFillColor(ACCENT_GOLD);
    currentTurnLabel.setPosition({ 320.f, 27.f });

    sf::Text errorText(font, "", 20);
    errorText.setFillColor(DANGER_TEXT);
    errorText.setPosition({ 42.f, 638.f });

    sf::Text hintText(font, "", 19);
    hintText.setFillColor(TEXT_SECONDARY);
    hintText.setPosition({ 284.f, 455.f });

    sf::RectangleShape deckBox(CARD_SIZE);
    deckBox.setPosition(deckPos);
    setBoxTexture(deckBox, textures, "back");

    sf::RectangleShape discardBox(CARD_SIZE);
    discardBox.setPosition(discardPos);
    discardBox.setFillColor(SLOT_EMPTY);

    sf::RectangleShape drawnBox(CARD_SIZE);
    drawnBox.setPosition(drawnPos);
    drawnBox.setFillColor(SLOT_EMPTY);

    std::vector<sf::RectangleShape> handBoxes;
    for (int i = 0; i < 4; i++)
    {
        sf::RectangleShape box(CARD_SIZE);
        box.setPosition({ 284.f + i * PLAYER_CARD_SPACING, 526.f });
        handBoxes.push_back(box);
    }

    auto visualCardPosition = [&](const nlohmann::json& stateJson, const std::string& playerId, int cardIndex) -> std::optional<sf::Vector2f>
        {
            if (cardIndex < 0 || cardIndex >= 4)
            {
                return std::nullopt;
            }

            if (playerId == client.getPlayerId())
            {
                return handBoxes[cardIndex].getPosition();
            }

            int opponentRow = getOpponentRowForPlayer(stateJson, client.getPlayerId(), playerId);
            if (opponentRow >= 0)
            {
                return getOpponentCardBounds(opponentRow, cardIndex).position;
            }

            return std::nullopt;
        };

    auto visualCardSize = [&](const nlohmann::json& stateJson, const std::string& playerId, int cardIndex) -> sf::Vector2f
        {
            if (playerId == client.getPlayerId())
            {
                return CARD_SIZE;
            }

            int opponentRow = getOpponentRowForPlayer(stateJson, client.getPlayerId(), playerId);
            return opponentRow >= 0 && cardIndex >= 0 ? OPPONENT_CARD_SIZE : CARD_SIZE;
        };

    auto handEndpointForCard = [&](const nlohmann::json& cardSlot, const std::string& fieldName, const std::string& fallbackZone, std::string& playerId, int& cardIndex) -> bool
        {
            const nlohmann::json* location = cardLocation(cardSlot, fieldName);
            std::string zone = locationZone(location);
            if (zone.empty())
            {
                zone = normalizeZoneName(fallbackZone);
            }

            if (zone == "hand")
            {
                std::string locationPlayer = locationPlayerId(location);
                std::optional<int> index = locationIndex(location);
                if (!locationPlayer.empty() && index.has_value())
                {
                    playerId = locationPlayer;
                    cardIndex = index.value();
                    return true;
                }
            }

            const nlohmann::json* swapInfo = cardSwapInfo(cardSlot);
            if (swapInfo == nullptr)
            {
                return false;
            }

            std::string playerField = fieldName == "from" ? "from_player_id" : "to_player_id";
            std::string playerCamelField = fieldName == "from" ? "fromPlayerId" : "toPlayerId";
            std::string indexField = fieldName == "from" ? "from_index" : "to_index";
            std::string indexCamelField = fieldName == "from" ? "fromIndex" : "toIndex";

            std::string swapPlayer = jsonString(*swapInfo, { playerField, playerCamelField });
            std::optional<int> swapIndex = getIntegerField(*swapInfo, { indexField, indexCamelField });
            if (swapPlayer.empty() || !swapIndex.has_value())
            {
                return false;
            }

            playerId = swapPlayer;
            cardIndex = swapIndex.value();
            return true;
        };

    auto visualLocationPosition = [&](const nlohmann::json& stateJson, const nlohmann::json* location, const std::string& fallbackZone) -> std::optional<sf::Vector2f>
        {
            std::string zone = locationZone(location);
            if (zone.empty())
            {
                zone = normalizeZoneName(fallbackZone);
            }

            if (zone == "pile")
            {
                return deckPos;
            }

            if (zone == "discard")
            {
                return discardPos;
            }

            if (zone == "pending_drawn_card")
            {
                return drawnPos;
            }

            if (zone == "hand")
            {
                std::string locationPlayer = locationPlayerId(location);
                std::optional<int> index = locationIndex(location);
                if (!locationPlayer.empty() && index.has_value())
                {
                    return visualCardPosition(stateJson, locationPlayer, index.value());
                }
            }

            return std::nullopt;
        };

    auto visualLocationSize = [&](const nlohmann::json& stateJson, const nlohmann::json* location, const std::string& fallbackZone) -> sf::Vector2f
        {
            std::string zone = locationZone(location);
            if (zone.empty())
            {
                zone = normalizeZoneName(fallbackZone);
            }

            if (zone == "hand")
            {
                std::string locationPlayer = locationPlayerId(location);
                std::optional<int> index = locationIndex(location);
                if (!locationPlayer.empty() && index.has_value())
                {
                    return visualCardSize(stateJson, locationPlayer, index.value());
                }
            }

            return CARD_SIZE;
        };

    auto sourcePositionForCard = [&](const nlohmann::json& stateJson, const nlohmann::json& cardSlot, sf::Vector2f fallbackPosition) -> sf::Vector2f
        {
            std::string sourcePlayerId;
            int sourceIndex = -1;
            if (handEndpointForCard(cardSlot, "from", cardSource(cardSlot), sourcePlayerId, sourceIndex))
            {
                std::optional<sf::Vector2f> position = visualCardPosition(stateJson, sourcePlayerId, sourceIndex);
                if (position.has_value())
                {
                    return position.value();
                }
            }

            std::optional<sf::Vector2f> structuredPosition = visualLocationPosition(stateJson, cardLocation(cardSlot, "from"), cardSource(cardSlot));
            if (structuredPosition.has_value())
            {
                return structuredPosition.value();
            }

            std::string source = cardSource(cardSlot);
            if (source == "pile")
            {
                return deckPos;
            }

            if (source == "discard")
            {
                return discardPos;
            }

            if (source == "pending_drawn_card")
            {
                return drawnPos;
            }

            if (source == "action_card")
            {
                const nlohmann::json* swapInfo = cardSwapInfo(cardSlot);
                if (swapInfo != nullptr)
                {
                    std::string fromPlayerId = jsonString(*swapInfo, { "from_player_id", "fromPlayerId" });
                    std::optional<int> fromIndex = getIntegerField(*swapInfo, { "from_index", "fromIndex" });
                    if (!fromPlayerId.empty() && fromIndex.has_value())
                    {
                        std::optional<sf::Vector2f> position = visualCardPosition(stateJson, fromPlayerId, fromIndex.value());
                        if (position.has_value())
                        {
                            return position.value();
                        }
                    }
                }
            }

            return fallbackPosition;
        };

    auto destinationPositionForCard = [&](const nlohmann::json& stateJson, const nlohmann::json& cardSlot, sf::Vector2f fallbackPosition) -> sf::Vector2f
        {
            std::string destinationPlayerId;
            int destinationIndex = -1;
            if (handEndpointForCard(cardSlot, "to", cardDestination(cardSlot), destinationPlayerId, destinationIndex))
            {
                std::optional<sf::Vector2f> position = visualCardPosition(stateJson, destinationPlayerId, destinationIndex);
                if (position.has_value())
                {
                    return position.value();
                }
            }

            std::optional<sf::Vector2f> structuredPosition = visualLocationPosition(stateJson, cardLocation(cardSlot, "to"), cardDestination(cardSlot));
            return structuredPosition.has_value() ? structuredPosition.value() : fallbackPosition;
        };

    auto sourceSizeForCard = [&](const nlohmann::json& stateJson, const nlohmann::json& cardSlot) -> sf::Vector2f
        {
            std::string sourcePlayerId;
            int sourceIndex = -1;
            if (handEndpointForCard(cardSlot, "from", cardSource(cardSlot), sourcePlayerId, sourceIndex))
            {
                return visualCardSize(stateJson, sourcePlayerId, sourceIndex);
            }

            return visualLocationSize(stateJson, cardLocation(cardSlot, "from"), cardSource(cardSlot));
        };

    auto destinationSizeForCard = [&](const nlohmann::json& stateJson, const nlohmann::json& cardSlot) -> sf::Vector2f
        {
            std::string destinationPlayerId;
            int destinationIndex = -1;
            if (handEndpointForCard(cardSlot, "to", cardDestination(cardSlot), destinationPlayerId, destinationIndex))
            {
                return visualCardSize(stateJson, destinationPlayerId, destinationIndex);
            }

            return visualLocationSize(stateJson, cardLocation(cardSlot, "to"), cardDestination(cardSlot));
        };

    auto swapLaneOffset = [](sf::Vector2f start, sf::Vector2f end) -> sf::Vector2f
        {
            sf::Vector2f delta = end - start;
            float length = std::sqrt(delta.x * delta.x + delta.y * delta.y);
            if (length < 1.f)
            {
                return { 0.f, 0.f };
            }

            const float laneDistance = 24.f;
            return { -delta.y / length * laneDistance, delta.x / length * laneDistance };
        };

    auto queueRemoteCardMotion = [&](const nlohmann::json& cardSlot, sf::Vector2f start, sf::Vector2f end, sf::Vector2f startSize, sf::Vector2f endSize, sf::Vector2f laneOffset, const std::string& sourcePlayerId, int sourceIndex, const std::string& destinationPlayerId, int destinationIndex, bool hideHandSlots)
        {
            RemoteCardMotion motion;
            motion.start = start;
            motion.end = end;
            motion.startSize = startSize;
            motion.endSize = endSize;
            motion.laneOffset = laneOffset;
            motion.sourcePlayerId = sourcePlayerId;
            motion.destinationPlayerId = destinationPlayerId;
            motion.sourceIndex = sourceIndex;
            motion.destinationIndex = destinationIndex;
            motion.hideHandSlots = hideHandSlots;
            motion.startedAt = remoteAnimClock.getElapsedTime().asSeconds();
            motion.duration = hideHandSlots ? 0.9f : remoteAnimDuration;
            motion.card.setSize(startSize);
            motion.card.setPosition(start);
            setBoxTexture(motion.card, textures, cardFileName(cardSlot));
            motion.card.setFillColor(sf::Color(255, 255, 255, 230));
            motion.card.setOutlineThickness(2.f);
            motion.card.setOutlineColor(ACCENT_GOLD);
            remoteCardMotions.push_back(motion);
            remoteAnimating = true;
        };

    auto queueCardMotionsFromStateChange = [&](const nlohmann::json& previousState, const nlohmann::json& refreshedState)
        {
            std::optional<nlohmann::json> previousDrawnCard = getDrawnCard(previousState);
            std::vector<nlohmann::json> movedCards = collectMovedCardSlots(refreshedState, client.getPlayerId());

            for (const nlohmann::json& movedCard : movedCards)
            {
                std::string movementKey = cardMovementKey(movedCard);
                if (movementKey.empty())
                {
                    continue;
                }

                std::optional<nlohmann::json> previousDestinationCard = findCardAtMovementDestination(previousState, movedCard);
                if (previousDestinationCard.has_value() &&
                    cardMovementKey(previousDestinationCard.value()) == movementKey &&
                    cardIdentity(previousDestinationCard.value()) == cardIdentity(movedCard))
                {
                    continue;
                }

                sf::Vector2f endPosition = destinationPositionForCard(refreshedState, movedCard, discardPos);
                sf::Vector2f startPosition = sourcePositionForCard(refreshedState, movedCard, endPosition);
                if (previousDrawnCard.has_value() &&
                    cardIdentity(previousDrawnCard.value()) == cardIdentity(movedCard) &&
                    (cardDestination(movedCard) == "discard" || cardDestination(movedCard) == "hand"))
                {
                    startPosition = drawnPos;
                }

                if (std::abs(startPosition.x - endPosition.x) < 1.f &&
                    std::abs(startPosition.y - endPosition.y) < 1.f)
                {
                    continue;
                }

                std::string sourcePlayerId;
                std::string destinationPlayerId;
                int sourceIndex = -1;
                int destinationIndex = -1;
                bool sourceIsHand = handEndpointForCard(movedCard, "from", cardSource(movedCard), sourcePlayerId, sourceIndex);
                bool destinationIsHand = handEndpointForCard(movedCard, "to", cardDestination(movedCard), destinationPlayerId, destinationIndex);
                bool handToHand = sourceIsHand && destinationIsHand;
                sf::Vector2f laneOffset = handToHand ? swapLaneOffset(startPosition, endPosition) : sf::Vector2f(0.f, 0.f);

                queueRemoteCardMotion(
                    movedCard,
                    startPosition,
                    endPosition,
                    sourceSizeForCard(refreshedState, movedCard),
                    destinationSizeForCard(refreshedState, movedCard),
                    laneOffset,
                    sourcePlayerId,
                    sourceIndex,
                    destinationPlayerId,
                    destinationIndex,
                    handToHand);
            }
        };

    auto queueHandToHandSwapMotion = [&](const nlohmann::json& previousState, const nlohmann::json& refreshedState, const std::string& firstPlayerId, int firstIndex, const std::string& secondPlayerId, int secondIndex, const std::optional<nlohmann::json>& secondCardOverride) -> bool
        {
            if (firstPlayerId.empty() || secondPlayerId.empty() || firstIndex < 0 || secondIndex < 0 ||
                (firstPlayerId == secondPlayerId && firstIndex == secondIndex))
            {
                return false;
            }

            std::optional<sf::Vector2f> firstStart = visualCardPosition(previousState, firstPlayerId, firstIndex);
            std::optional<sf::Vector2f> secondStart = visualCardPosition(previousState, secondPlayerId, secondIndex);
            if (!firstStart.has_value() || !secondStart.has_value())
            {
                return false;
            }

            sf::Vector2f firstEnd = secondStart.value();
            sf::Vector2f secondEnd = firstStart.value();

            nlohmann::json firstCard = findPlayerCardByIndex(previousState, firstPlayerId, firstIndex).value_or(nlohmann::json::object());
            if (firstCard.empty())
            {
                firstCard = findPlayerCardByIndex(refreshedState, secondPlayerId, secondIndex).value_or(nlohmann::json::object());
            }

            nlohmann::json secondCard = secondCardOverride.value_or(nlohmann::json::object());
            if (secondCard.empty())
            {
                secondCard = findPlayerCardByIndex(previousState, secondPlayerId, secondIndex).value_or(nlohmann::json::object());
            }
            if (secondCard.empty() || cardFileName(secondCard) == "back")
            {
                nlohmann::json refreshedSecondCard = findPlayerCardByIndex(refreshedState, firstPlayerId, firstIndex).value_or(nlohmann::json::object());
                if (!refreshedSecondCard.empty())
                {
                    secondCard = refreshedSecondCard;
                }
            }

            queueRemoteCardMotion(
                firstCard,
                firstStart.value(),
                firstEnd,
                visualCardSize(previousState, firstPlayerId, firstIndex),
                visualCardSize(previousState, secondPlayerId, secondIndex),
                swapLaneOffset(firstStart.value(), firstEnd),
                firstPlayerId,
                firstIndex,
                secondPlayerId,
                secondIndex,
                true);

            queueRemoteCardMotion(
                secondCard,
                secondStart.value(),
                secondEnd,
                visualCardSize(previousState, secondPlayerId, secondIndex),
                visualCardSize(previousState, firstPlayerId, firstIndex),
                swapLaneOffset(secondStart.value(), secondEnd),
                secondPlayerId,
                secondIndex,
                firstPlayerId,
                firstIndex,
                true);

            return true;
        };

    auto slotHiddenByRemoteMotion = [&](const std::string& playerId, int cardIndex) -> bool
        {
            for (const RemoteCardMotion& motion : remoteCardMotions)
            {
                if (!motion.hideHandSlots)
                {
                    continue;
                }

                bool sourceMatches = motion.sourcePlayerId == playerId && motion.sourceIndex == cardIndex;
                bool destinationMatches = motion.destinationPlayerId == playerId && motion.destinationIndex == cardIndex;
                if (sourceMatches || destinationMatches)
                {
                    return true;
                }
            }

            return false;
        };

    auto captureActionRevealFromClientState = [&]()
        {
            const nlohmann::json& currentState = client.getState();
            if (currentState.is_object() && currentState.contains("action_reveal") && currentState["action_reveal"].is_object())
            {
                activeActionReveal = currentState["action_reveal"];
                activeActionRevealDuration = getActionRevealDuration(activeActionReveal);
                actionRevealClock.restart();
            }
        };

    auto readSeeAndSwapTargetForMotion = [&](const nlohmann::json& stateJson, std::string& targetPlayerId, int& targetIndex, std::optional<nlohmann::json>& targetCard) -> bool
        {
            nlohmann::json reveal;
            if (stateJson.is_object() &&
                stateJson.contains("pending_action") &&
                stateJson["pending_action"].is_object() &&
                stateJson["pending_action"].contains("revealed_card") &&
                stateJson["pending_action"]["revealed_card"].is_object())
            {
                reveal = stateJson["pending_action"]["revealed_card"];
            }
            else if (activeActionReveal.is_object())
            {
                reveal = activeActionReveal;
            }

            if (!reveal.is_object())
            {
                return false;
            }

            targetPlayerId = getActionRevealOwnerId(reveal);
            std::optional<int> cardIndex = getActionRevealCardIndex(reveal);
            targetCard = getActionRevealCard(reveal);
            if (targetPlayerId.empty() || !cardIndex.has_value())
            {
                return false;
            }

            targetIndex = cardIndex.value();
            return true;
        };

    while (window.isOpen())
    {
        float dt = std::min(frameClock.restart().asSeconds(), 1.f / 20.f);
        float uiTime = uiClock.getElapsedTime().asSeconds();
        if (activeActionReveal.is_object() && actionRevealClock.getElapsedTime().asSeconds() >= activeActionRevealDuration)
        {
            activeActionReveal = nullptr;
            activeActionRevealDuration = 0.f;
        }

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
                    screwConfirmOpen = false;
                    lastObservedStateSignature.clear();
                    lastObservedCurrentPlayerId.clear();
                    activeActionReveal = nullptr;
                    activeActionRevealDuration = 0.f;
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
                                roomSelectPollClock.restart();
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
                            roomSelectPollClock.restart();
                        }
                        else
                        {
                            statusMessage = client.getLastError();
                        }
                    }
                    else
                    {
                        const float firstRoomY = 318.f;
                        const float roomHeight = 52.f;
                        const float roomX = 220.f;
                        const float roomWidth = 560.f;

                        int roomsToShow = std::min(static_cast<int>(availableRooms.size()), 5);
                        for (int i = 0; i < roomsToShow; i++)
                        {
                            sf::FloatRect roomBounds({ roomX, firstRoomY + i * roomHeight }, { roomWidth, 42.f });
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
                    if (screwConfirmOpen)
                    {
                        screwConfirmOpen = false;
                        statusMessage.clear();
                    }
                    else if (hasAllowedAction(gameState, "cancel_see_and_swap"))
                    {
                        if (client.cancelSeeAndSwap())
                        {
                            lastObservedStateSignature = makeStateSignature(client.getState());
                            lastObservedCurrentPlayerId = getCurrentPlayerId(client.getState());
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
                clickPulses.push_back({ mouse, uiTime });
                const nlohmann::json& gameState = client.getState();
                nlohmann::json stateBeforeClick = gameState;
                std::optional<nlohmann::json> drawnCard = getDrawnCard(gameState);
                std::optional<nlohmann::json> discardCard = getDiscardCard(gameState);
                std::vector<nlohmann::json> hand = getPlayerHand(gameState, client.getPlayerId());
                std::vector<nlohmann::json> otherPlayers = getOtherPlayers(gameState, client.getPlayerId());

                if (screwConfirmOpen)
                {
                    if (screwConfirmYesButton.getGlobalBounds().contains(mouse))
                    {
                        if (client.callScrew())
                        {
                            lastObservedStateSignature = makeStateSignature(client.getState());
                            lastObservedCurrentPlayerId = getCurrentPlayerId(client.getState());
                            playingPollClock.restart();
                            waitingForDiscardSwap = false;
                            discardSwapOwnIndex = -1;
                            giveAndTakeOwnIndex = -1;
                            screwConfirmOpen = false;
                            statusMessage.clear();
                        }
                        else
                        {
                            statusMessage = client.getLastError();
                        }
                    }
                    else
                    {
                        screwConfirmOpen = false;
                        statusMessage.clear();
                    }

                    continue;
                }

                if (isRoundFinished(gameState))
                {
                    continue;
                }

                if (screwButton.getGlobalBounds().contains(mouse))
                {
                    if (canCallScrew(gameState, client.getPlayerId()))
                    {
                        screwConfirmOpen = true;
                        statusMessage.clear();
                    }
                    else
                    {
                        statusMessage = "You can only call Screw on your turn before drawing.";
                    }

                    continue;
                }

                if (isActionTargetPhase(gameState))
                {
                    bool actionWorked = false;
                    bool handledActionClick = false;
                    bool queueExplicitHandSwap = false;
                    bool skipStateChangeMotion = false;
                    int explicitOwnIndex = -1;
                    int explicitTargetIndex = -1;
                    std::string explicitTargetPlayerId;
                    std::optional<nlohmann::json> explicitTargetCard;

                    if (hasAllowedAction(gameState, "cancel_see_and_swap") && discardBox.getGlobalBounds().contains(mouse))
                    {
                        handledActionClick = true;
                        skipStateChangeMotion = true;
                        actionWorked = client.cancelSeeAndSwap();
                    }
                    else if (hasAllowedAction(gameState, "reveal_own_card"))
                    {
                        for (int i = 0; i < static_cast<int>(handBoxes.size()) && i < static_cast<int>(hand.size()); i++)
                        {
                            if (handBoxes[i].getGlobalBounds().contains(mouse))
                            {
                                handledActionClick = true;
                                skipStateChangeMotion = true;
                                actionWorked = client.revealOwnCard(i);
                                break;
                            }
                        }
                    }
                    else if (hasAllowedAction(gameState, "reveal_opponent_card"))
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
                                    skipStateChangeMotion = true;
                                    if (!targetPlayerId.empty())
                                    {
                                        actionWorked = client.revealOpponentCard(targetPlayerId, cardIndex);
                                    }
                                    break;
                                }
                            }
                        }
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
                                            explicitOwnIndex = giveAndTakeOwnIndex;
                                            explicitTargetPlayerId = targetPlayerId;
                                            explicitTargetIndex = cardIndex;
                                            explicitTargetCard = findPlayerCardByIndex(stateBeforeClick, targetPlayerId, cardIndex);
                                            queueExplicitHandSwap = true;
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
                                skipStateChangeMotion = true;
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
                                    skipStateChangeMotion = true;
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
                                std::string seeTargetPlayerId;
                                int seeTargetIndex = -1;
                                std::optional<nlohmann::json> seeTargetCard;
                                if (readSeeAndSwapTargetForMotion(gameState, seeTargetPlayerId, seeTargetIndex, seeTargetCard))
                                {
                                    explicitOwnIndex = i;
                                    explicitTargetPlayerId = seeTargetPlayerId;
                                    explicitTargetIndex = seeTargetIndex;
                                    explicitTargetCard = seeTargetCard;
                                    queueExplicitHandSwap = true;
                                }
                                actionWorked = client.resolveSeeAndSwap(i);
                                break;
                            }
                        }
                    }

                    if (actionWorked)
                    {
                        captureActionRevealFromClientState();
                        lastObservedStateSignature = makeStateSignature(client.getState());
                        lastObservedCurrentPlayerId = getCurrentPlayerId(client.getState());
                        playingPollClock.restart();
                        waitingForDiscardSwap = false;
                        discardSwapOwnIndex = -1;
                        giveAndTakeOwnIndex = -1;
                        bool explicitMotionQueued = queueExplicitHandSwap &&
                            queueHandToHandSwapMotion(stateBeforeClick, client.getState(), client.getPlayerId(), explicitOwnIndex, explicitTargetPlayerId, explicitTargetIndex, explicitTargetCard);
                        if (explicitMotionQueued)
                        {
                            activeActionReveal = nullptr;
                            activeActionRevealDuration = 0.f;
                        }
                        if (!explicitMotionQueued && !skipStateChangeMotion)
                        {
                            queueCardMotionsFromStateChange(stateBeforeClick, client.getState());
                        }
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
                        else if (hasAllowedAction(gameState, "reveal_own_card"))
                        {
                            statusMessage = "Choose one of your cards to peek.";
                        }
                        else if (hasAllowedAction(gameState, "reveal_opponent_card"))
                        {
                            statusMessage = "Choose one opponent card to peek.";
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
                        playingPollClock.restart();
                        statusMessage.clear();
                        waitingForDiscardSwap = false;
                        discardSwapOwnIndex = -1;
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
                            playingPollClock.restart();
                            queueCardMotionsFromStateChange(stateBeforeClick, client.getState());
                            statusMessage.clear();
                            waitingForDiscardSwap = false;
                            discardSwapOwnIndex = -1;
                            giveAndTakeOwnIndex = -1;
                        }
                        else
                        {
                            statusMessage = client.getLastError();
                        }
                    }
                    else
                    {
                        if (!waitingForDiscardSwap || discardSwapOwnIndex < 0)
                        {
                            statusMessage = hasAllowedAction(gameState, "take_discard_and_swap") ?
                                "Choose one of your cards first." :
                                "You cannot take discard right now.";
                        }
                        else if (!hasAllowedAction(gameState, "take_discard_and_swap"))
                        {
                            statusMessage = "You cannot take discard right now.";
                            waitingForDiscardSwap = false;
                            discardSwapOwnIndex = -1;
                        }
                        else if (client.takeDiscardAndSwap(discardSwapOwnIndex))
                        {
                            int selectedIndex = discardSwapOwnIndex;
                            lastObservedStateSignature = makeStateSignature(client.getState());
                            lastObservedCurrentPlayerId = getCurrentPlayerId(client.getState());
                            playingPollClock.restart();
                            animating = true;
                            animTargetIndex = selectedIndex;
                            animClock.restart();
                            setBoxTexture(ghostToHand, textures, discardCard.has_value() ? cardFileName(*discardCard) : "back");
                            ghostToHandStart = discardPos;
                            ghostToHand.setPosition(discardPos);
                            if (selectedIndex < static_cast<int>(hand.size()))
                            {
                                setBoxTexture(ghostToDiscard, textures, cardFileName(hand[selectedIndex]));
                            }
                            else
                            {
                                setBoxTexture(ghostToDiscard, textures, "back");
                            }
                            ghostToDiscardStart = handBoxes[selectedIndex].getPosition();
                            ghostToDiscard.setPosition(handBoxes[selectedIndex].getPosition());
                            waitingForDiscardSwap = false;
                            discardSwapOwnIndex = -1;
                            giveAndTakeOwnIndex = -1;
                            statusMessage.clear();
                        }
                        else
                        {
                            statusMessage = client.getLastError();
                        }
                    }
                }
                else
                {
                    for (int i = 0; i < static_cast<int>(handBoxes.size()); i++)
                    {
                        bool handClicked = handBoxes[i].getGlobalBounds().contains(mouse);
                        if (!handClicked && waitingForDiscardSwap && i == discardSwapOwnIndex)
                        {
                            sf::FloatRect liftedBounds(handBoxes[i].getPosition() - sf::Vector2f(0.f, 16.f), CARD_SIZE);
                            handClicked = liftedBounds.contains(mouse);
                        }

                        if (!handClicked)
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
                            if (i == discardSwapOwnIndex)
                            {
                                waitingForDiscardSwap = false;
                                discardSwapOwnIndex = -1;
                                statusMessage = "Discard swap canceled.";
                            }
                            else if (hasAllowedAction(gameState, "take_discard_and_swap"))
                            {
                                waitingForDiscardSwap = true;
                                discardSwapOwnIndex = i;
                                statusMessage = "Click Discard to swap with the selected card.";
                            }
                            else
                            {
                                statusMessage = "You cannot take discard right now.";
                                waitingForDiscardSwap = false;
                                discardSwapOwnIndex = -1;
                            }
                        }
                        else if (hasAllowedAction(gameState, "take_discard_and_swap"))
                        {
                            waitingForDiscardSwap = true;
                            discardSwapOwnIndex = i;
                            statusMessage = "Click Discard to swap with the selected card.";
                        }

                        if (actionWorked)
                        {
                            lastObservedStateSignature = makeStateSignature(client.getState());
                            lastObservedCurrentPlayerId = getCurrentPlayerId(client.getState());
                            playingPollClock.restart();
                            animating = true;
                            animTargetIndex = i;
                            animClock.restart();
                            setBoxTexture(ghostToHand, textures, drawnCard.has_value() ? cardFileName(*drawnCard) : "back");
                            ghostToHandStart = drawnPos;
                            ghostToHand.setPosition(drawnPos);
                            setBoxTexture(ghostToDiscard, textures, "back");
                            ghostToDiscardStart = handBoxes[i].getPosition();
                            ghostToDiscard.setPosition(handBoxes[i].getPosition());
                            waitingForDiscardSwap = false;
                            discardSwapOwnIndex = -1;
                            giveAndTakeOwnIndex = -1;
                            statusMessage.clear();
                        }
                        else if (drawnCard.has_value() || (waitingForDiscardSwap && discardSwapOwnIndex < 0))
                        {
                            statusMessage = client.getLastError();
                        }

                        break;
                    }
                }
            }
        }

        if (state == AppState::RoomSelect && roomSelectPollClock.getElapsedTime().asSeconds() >= appConfig.menuPollIntervalSecs)
        {
            roomSelectPollClock.restart();
            nlohmann::json roomsJson;
            if (client.fetchRooms(roomsJson))
            {
                availableRooms = readRoomsFromJson(roomsJson);
                if (statusMessage == "Refreshing rooms..." || statusMessage == "Loading rooms...")
                {
                    statusMessage.clear();
                }
            }
            else
            {
                statusMessage = client.getLastError();
            }
        }

        if (state == AppState::WaitingRoom && !isHost && waitingRoomPollClock.getElapsedTime().asSeconds() >= appConfig.menuPollIntervalSecs)
        {
            waitingRoomPollClock.restart();
            if (client.refreshState() && gameHasStarted(client.getState(), client.getPlayerId()))
            {
                state = AppState::Playing;
                playingPollClock.restart();
                lastObservedStateSignature = makeStateSignature(client.getState());
                lastObservedCurrentPlayerId = getCurrentPlayerId(client.getState());
                statusMessage.clear();
            }
        }

        if (state == AppState::WaitingRoom && lobbyPollClock.getElapsedTime().asSeconds() >= appConfig.menuPollIntervalSecs)
        {
            lobbyPollClock.restart();
            nlohmann::json lobbyJson;
            if (client.fetchLobby(lobbyJson))
            {
                latestLobby = lobbyJson;
            }
        }

        if (state == AppState::Playing && playingPollClock.getElapsedTime().asSeconds() >= appConfig.gamePollIntervalSecs)
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
                bool remoteStateChanged = !previousSignature.empty() &&
                    previousSignature != newSignature &&
                    previousCurrentPlayerId != client.getPlayerId();

                if (remoteStateChanged && !animating)
                {
                    queueCardMotionsFromStateChange(previousState, refreshedState);
                }

                lastObservedStateSignature = newSignature;
                lastObservedCurrentPlayerId = newCurrentPlayerId;
                if (!isActionTargetPhase(refreshedState))
                {
                    giveAndTakeOwnIndex = -1;
                }
                if (!hasAllowedAction(refreshedState, "take_discard_and_swap"))
                {
                    waitingForDiscardSwap = false;
                    discardSwapOwnIndex = -1;
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
                ghostToHand.setPosition(lerp(ghostToHandStart, handBoxes[animTargetIndex].getPosition(), easedT));
                ghostToDiscard.setPosition(lerp(ghostToDiscardStart, discardPos, easedT));
            }
        }

        if (state == AppState::Playing && remoteAnimating)
        {
            float now = remoteAnimClock.getElapsedTime().asSeconds();
            for (RemoteCardMotion& motion : remoteCardMotions)
            {
                float t = (now - motion.startedAt) / motion.duration;
                float easedT = easeInOut(t);
                float laneT = std::sin(easedT * 3.14159265f);
                sf::Vector2f lane = { motion.laneOffset.x * laneT, motion.laneOffset.y * laneT };
                motion.card.setSize(lerp(motion.startSize, motion.endSize, easedT));
                motion.card.setPosition(lerp(motion.start, motion.end, easedT) + lane);
                int alpha = static_cast<int>(170.f + 85.f * (1.f - std::abs((easedT - 0.5f) * 2.f)));
                motion.card.setFillColor(sf::Color(255, 255, 255, static_cast<std::uint8_t>(alpha)));
            }

            remoteCardMotions.erase(
                std::remove_if(remoteCardMotions.begin(), remoteCardMotions.end(), [now](const RemoteCardMotion& motion)
                    {
                        return now - motion.startedAt >= motion.duration;
                    }),
                remoteCardMotions.end());
            remoteAnimating = !remoteCardMotions.empty();
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
            drawPanel(window, { 245.f, 160.f }, { 510.f, 260.f }, TABLE_PANEL_DARK, PANEL_OUTLINE);
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

            drawPanel(window, { 180.f, 48.f }, { 640.f, 545.f }, TABLE_PANEL_DARK, PANEL_OUTLINE);
            window.draw(roomSelectTitle);
            window.draw(roomSelectInfo);
            drawButton(window, createRoomButton, createRoomText, createRoomButton.getGlobalBounds().contains(mouse), ACCENT_GREEN, sf::Color(96, 156, 120));
            drawButton(window, refreshRoomsButton, refreshRoomsText, refreshRoomsButton.getGlobalBounds().contains(mouse), ACCENT_BLUE, sf::Color(90, 130, 122));

            if (availableRooms.empty())
            {
                sf::Text noRoomsText(font, "No public rooms found.", 24);
                noRoomsText.setFillColor(TEXT_SECONDARY);
                noRoomsText.setPosition({ 420.f, 325.f });
                window.draw(noRoomsText);
            }
            else
            {
                const float firstRoomY = 318.f;
                const float roomHeight = 52.f;
                const float roomX = 220.f;
                const float roomWidth = 560.f;
                int roomsToShow = std::min(static_cast<int>(availableRooms.size()), 5);

                for (int i = 0; i < roomsToShow; i++)
                {
                    sf::RectangleShape roomButton({ roomWidth, 42.f });
                    roomButton.setPosition({ roomX, firstRoomY + i * roomHeight });
                    bool roomHovered = roomButton.getGlobalBounds().contains(mouse);
                    roomButton.setFillColor(roomHovered ? TABLE_PANEL : TABLE_PANEL_DARK);
                    roomButton.setOutlineThickness(1.f);
                    roomButton.setOutlineColor(roomHovered ? ACCENT_GOLD : PANEL_OUTLINE);

                    sf::Text roomText(font, availableRooms[i].displayText, 22);
                    roomText.setFillColor(TEXT_PRIMARY);
                    fitTextToWidth(roomText, roomWidth - 36.f, 14);
                    roomText.setPosition({ roomX + 18.f, firstRoomY + i * roomHeight + 8.f });

                    window.draw(roomButton);
                    window.draw(roomText);
                }
            }
        }
        else if (state == AppState::WaitingRoom)
        {
            drawPanel(window, { 235.f, 70.f }, { 530.f, 500.f }, TABLE_PANEL_DARK, PANEL_OUTLINE);
            std::string playerListText;
            std::vector<std::string> lobbyLabels = readLobbyPlayerLabels(latestLobby);
            if (lobbyLabels.empty())
            {
                playerListText = "\n- " + nickname + (isHost ? " (host)" : "");
            }
            else
            {
                for (const std::string& playerLabel : lobbyLabels)
                {
                    playerListText += "\n- " + playerLabel;
                }
            }

            if (isHost)
            {
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
                    "Players: " + getLobbyPlayerCountText(latestLobby) + playerListText + "\n\n" +
                    "Waiting for host to start..."
                );
            }
            waitingInfo.setCharacterSize(26);
            fitTextToBox(waitingInfo, { 455.f, 205.f }, 16);

            window.draw(waitingTitle);
            window.draw(waitingInfo);

            if (isHost)
            {
                drawButton(window, startButton, startButtonText, startButton.getGlobalBounds().contains(mouse), ACCENT_BLUE, sf::Color(90, 130, 122));
            }
        }
        else if (state == AppState::Playing)
        {
            const nlohmann::json& gameState = client.getState();
            bool roundFinished = isRoundFinished(gameState);
            std::vector<nlohmann::json> hand = getDisplayHand(gameState, client.getPlayerId());
            std::vector<nlohmann::json> otherPlayers = getDisplayOtherPlayers(gameState, client.getPlayerId());
            std::optional<nlohmann::json> discardCard = getDiscardCard(gameState);
            std::optional<nlohmann::json> drawnCard = getDrawnCard(gameState);
            std::string currentPlayerId = getCurrentPlayerId(gameState);
            bool localTurn = !roundFinished && (currentPlayerId.empty() || currentPlayerId == client.getPlayerId() || !gameState.value("allowed_actions", nlohmann::json::array()).empty());
            float turnPulse = 0.5f + 0.5f * std::sin(uiTime * 4.2f);


            int visibleOpponentRows = std::max(1, static_cast<int>(otherPlayers.size()));
            float opponentPanelHeight = 134.f + static_cast<float>(visibleOpponentRows - 1) * OPPONENT_ROW_SPACING;
            drawPanel(window, { 24.f, 84.f }, { 360.f, opponentPanelHeight }, TABLE_PANEL_DARK, PANEL_OUTLINE);
            drawPanel(window, { 592.f, 84.f }, { 300.f, 202.f }, TABLE_PANEL, PANEL_OUTLINE);
            drawPanel(window, { 250.f, 480.f }, { 522.f, 184.f }, TABLE_PANEL_DARK, PANEL_OUTLINE);

            setFittedText(currentTurnLabel, getCurrentTurnText(gameState), 24, 360.f, 16);
            sf::Color turnOutline = localTurn ? withAlpha(ACCENT_GOLD, static_cast<std::uint8_t>(150 + 80 * turnPulse)) : PANEL_OUTLINE;
            drawPanel(window, { 300.f, 16.f }, { 400.f, 45.f }, localTurn ? sf::Color(42, 56, 45) : TABLE_PANEL_DARK, turnOutline);
            currentTurnLabel.setFillColor(localTurn ? ACCENT_GOLD : TEXT_SECONDARY);
            window.draw(currentTurnLabel);

            bool actionTargetPhase = !roundFinished && isActionTargetPhase(gameState);
            bool canGiveAndTake = hasAllowedAction(gameState, "resolve_give_and_take");
            bool canRevealOwnCard = hasAllowedAction(gameState, "reveal_own_card");
            bool canRevealOpponentCard = hasAllowedAction(gameState, "reveal_opponent_card");
            bool canRevealSeeAndSwap = hasAllowedAction(gameState, "reveal_see_and_swap_target");
            bool canResolveSeeAndSwap = hasAllowedAction(gameState, "resolve_see_and_swap");
            bool actionRevealActive = activeActionReveal.is_object() && actionRevealClock.getElapsedTime().asSeconds() < activeActionRevealDuration;
            opponentHover.resize(otherPlayers.size());

            for (int playerIndex = 0; playerIndex < static_cast<int>(otherPlayers.size()); playerIndex++)
            {
                const nlohmann::json& otherPlayer = otherPlayers[playerIndex];
                std::string otherPlayerId = jsonString(otherPlayer, { "player_id", "playerId", "id" });
                std::string otherName = jsonString(otherPlayer, { "nickname", "name", "player_id", "id" });
                if (otherName.empty())
                {
                    otherName = "Player";
                }

                float rowY = getOpponentCardBounds(playerIndex, 0).position.y;
                sf::Text otherNameText(font, otherName, 18);
                bool opponentTurn = !currentPlayerId.empty() && currentPlayerId == otherPlayerId;
                otherNameText.setFillColor(opponentTurn ? ACCENT_GOLD : TEXT_SECONDARY);
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

                    if (slotHiddenByRemoteMotion(otherPlayerId, cardIndex))
                    {
                        otherCardBox.setTexture(nullptr);
                        otherCardBox.setFillColor(SLOT_EMPTY);
                        styleCardSlot(otherCardBox, false, false);
                        drawCardVisual(window, otherCardBox, false, false, false, false, 0.f, 0.f, uiTime);
                        continue;
                    }

                    bool revealedSlot = actionRevealActive && actionRevealMatchesSlot(activeActionReveal, otherPlayerId, cardIndex);
                    std::optional<nlohmann::json> revealedCard = revealedSlot ? getActionRevealCard(activeActionReveal) : std::nullopt;
                    if (revealedCard.has_value())
                    {
                        setBoxTexture(otherCardBox, textures, cardFileName(revealedCard.value()));
                    }
                    else if (cardIndex < static_cast<int>(otherCards.size()))
                    {
                        setBoxTexture(otherCardBox, textures, cardFileName(otherCards[cardIndex]));
                    }
                    else
                    {
                        setBoxTexture(otherCardBox, textures, "back");
                    }

                    bool actionableOpponent = canRevealOpponentCard || canRevealSeeAndSwap || (canGiveAndTake && giveAndTakeOwnIndex >= 0);
                    bool hovered = otherCardBox.getGlobalBounds().contains(mouse);
                    opponentHover[playerIndex][cardIndex] = approach(opponentHover[playerIndex][cardIndex], hovered ? 1.f : 0.f, hovered ? 9.f : 7.f, dt);
                    styleCardSlot(otherCardBox, hovered, actionableOpponent);
                    drawCardVisual(window, otherCardBox, hovered, actionableOpponent, !revealedSlot && !opponentTurn && !actionableOpponent && !localTurn, revealedSlot, opponentHover[playerIndex][cardIndex], 0.f, uiTime);
                }
            }

            bool canSwapDrawn = !roundFinished && drawnCard.has_value() && hasAllowedAction(gameState, "swap_drawn_with_own_card");
            bool canSelectDiscardSwap = !roundFinished && !drawnCard.has_value() && hasAllowedAction(gameState, "take_discard_and_swap");
            for (int i = 0; i < static_cast<int>(handBoxes.size()); i++)
            {
                if (animating && i == animTargetIndex)
                {
                    continue;
                }

                if (slotHiddenByRemoteMotion(client.getPlayerId(), i))
                {
                    handBoxes[i].setTexture(nullptr);
                    handBoxes[i].setFillColor(SLOT_EMPTY);
                    styleCardSlot(handBoxes[i], false, false);
                    drawCardVisual(window, handBoxes[i], false, false, false, false, 0.f, 0.f, uiTime);
                    continue;
                }

                bool revealedOwnSlot = actionRevealActive && actionRevealMatchesSlot(activeActionReveal, client.getPlayerId(), i);
                std::optional<nlohmann::json> revealedOwnCard = revealedOwnSlot ? getActionRevealCard(activeActionReveal) : std::nullopt;
                if (revealedOwnCard.has_value())
                {
                    setBoxTexture(handBoxes[i], textures, cardFileName(revealedOwnCard.value()));
                }
                else if (i < static_cast<int>(hand.size()))
                {
                    setBoxTexture(handBoxes[i], textures, cardFileName(hand[i]));
                }
                else
                {
                    handBoxes[i].setTexture(nullptr);
                    handBoxes[i].setFillColor(SLOT_EMPTY);
                }

                bool selectedForDiscardSwap = waitingForDiscardSwap && i == discardSwapOwnIndex;
                bool ownCardActionable = canSwapDrawn || canSelectDiscardSwap || canGiveAndTake || canRevealOwnCard || canRevealSeeAndSwap || canResolveSeeAndSwap;
                bool hovered = handBoxes[i].getGlobalBounds().contains(mouse);
                if (!hovered && selectedForDiscardSwap)
                {
                    sf::FloatRect liftedBounds(handBoxes[i].getPosition() - sf::Vector2f(0.f, 16.f), CARD_SIZE);
                    hovered = liftedBounds.contains(mouse);
                }
                bool pressed = hovered && sf::Mouse::isButtonPressed(sf::Mouse::Button::Left);
                handHover[i] = approach(handHover[i], hovered ? 1.f : 0.f, hovered ? 10.f : 7.f, dt);
                styleCardSlot(handBoxes[i], hovered, ownCardActionable);
                if ((canGiveAndTake && i == giveAndTakeOwnIndex) || selectedForDiscardSwap || revealedOwnSlot)
                {
                    handBoxes[i].setOutlineThickness(5.f);
                    handBoxes[i].setOutlineColor(ACCENT_GOLD);
                }
                sf::RectangleShape displayHandBox = handBoxes[i];
                if (selectedForDiscardSwap)
                {
                    displayHandBox.setPosition(displayHandBox.getPosition() - sf::Vector2f(0.f, 16.f));
                }
                drawCardVisual(window, displayHandBox, hovered, ownCardActionable, !revealedOwnSlot && !localTurn && !ownCardActionable, (canGiveAndTake && i == giveAndTakeOwnIndex) || selectedForDiscardSwap || revealedOwnSlot, handHover[i], pressed ? 1.f : 0.f, uiTime);
            }

            if (discardCard.has_value())
            {
                setBoxTexture(discardBox, textures, cardFileName(*discardCard));
            }
            else
            {
                discardBox.setTexture(nullptr);
                discardBox.setFillColor(SLOT_EMPTY);
            }
            bool discardHovered = discardBox.getGlobalBounds().contains(mouse);
            bool discardActionable = !roundFinished && (drawnCard.has_value() || (waitingForDiscardSwap && discardSwapOwnIndex >= 0 && hasAllowedAction(gameState, "take_discard_and_swap")) || hasAllowedAction(gameState, "cancel_see_and_swap"));
            pileHover[1] = approach(pileHover[1], discardHovered ? 1.f : 0.f, discardHovered ? 10.f : 7.f, dt);
            styleCardSlot(discardBox, discardHovered, discardActionable);

            drawCardVisual(window, discardBox, discardHovered, discardActionable, !discardActionable && !localTurn, false, pileHover[1], discardHovered && sf::Mouse::isButtonPressed(sf::Mouse::Button::Left) ? 1.f : 0.f, uiTime);
            drawCardLabel(window, font, "Discard", discardPos, TEXT_SECONDARY);

            bool deckHovered = sf::FloatRect(deckPos, CARD_SIZE).contains(mouse);
            bool deckActionable = !roundFinished && !drawnCard.has_value() && hasAllowedAction(gameState, "draw_from_deck");
            pileHover[0] = approach(pileHover[0], deckHovered ? 1.f : 0.f, deckHovered ? 10.f : 7.f, dt);
            for (int i = 0; i < 3; i++)
            {
                sf::RectangleShape deckLayer(CARD_SIZE);
                deckLayer.setTexture(deckBox.getTexture(), true);
                deckLayer.setFillColor(sf::Color::White);
                deckLayer.setPosition({ deckPos.x - (i * 4.f), deckPos.y - (i * 4.f) });
                styleCardSlot(deckLayer, i == 0 && deckHovered, i == 0 && deckActionable);
                drawCardVisual(window, deckLayer, i == 0 && deckHovered, i == 0 && deckActionable, !deckActionable && !localTurn, false, i == 0 ? pileHover[0] : 0.f, i == 0 && deckHovered && sf::Mouse::isButtonPressed(sf::Mouse::Button::Left) ? 1.f : 0.f, uiTime);
            }
            drawCardLabel(window, font, "Deck", deckPos, TEXT_SECONDARY);

            if (drawnCard.has_value())
            {
                setBoxTexture(drawnBox, textures, cardFileName(*drawnCard));
                bool drawnHovered = drawnBox.getGlobalBounds().contains(mouse);
                pileHover[2] = approach(pileHover[2], drawnHovered ? 1.f : 0.f, drawnHovered ? 10.f : 7.f, dt);
                styleCardSlot(drawnBox, drawnHovered, true);
                drawCardVisual(window, drawnBox, drawnHovered, true, false, true, pileHover[2], 0.f, uiTime);
                drawCardLabel(window, font, "Picked card", drawnPos, ACCENT_GOLD);
            }
            else
            {
                pileHover[2] = approach(pileHover[2], 0.f, 7.f, dt);
            }

            setFittedText(playerNameLabel, getPlayerNameFromState(gameState, nickname), 24, 148.f, 15);
            window.draw(playerNameLabel);

            if (actionTargetPhase)
            {
                if (canGiveAndTake)
                {
                    setFittedText(hintText, giveAndTakeOwnIndex < 0 ? "Give And Take: choose one of your cards" : "Give And Take: choose an opponent card", 19, 438.f, 13);
                }
                else if (canRevealOwnCard)
                {
                    setFittedText(hintText, "Peek: choose one of your cards", 19, 438.f, 13);
                }
                else if (canRevealOpponentCard)
                {
                    setFittedText(hintText, "Peek: choose one opponent card", 19, 438.f, 13);
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
                    roundFinished ? "Round finished. Cards are revealed and scores are shown." :
                    (drawnCard.has_value() ? "Discard the picked card or choose one of your cards to swap." :
                        (waitingForDiscardSwap ? "Click Discard to swap, or click the selected card to cancel." : "Deck draws. Choose one of your cards before taking discard.")),
                    19,
                    438.f,
                    13);
            }
            window.draw(hintText);
            sf::Text handLabel(font, "Your cards", 20);
            handLabel.setFillColor(ACCENT_GOLD);
            handLabel.setPosition({ 284.f, 488.f });
            window.draw(handLabel);

            if (animating)
            {
                window.draw(ghostToHand);
                window.draw(ghostToDiscard);
            }

            if (remoteAnimating)
            {
                for (const RemoteCardMotion& motion : remoteCardMotions)
                {
                    sf::RectangleShape remoteShadow(motion.card.getSize());
                    remoteShadow.setPosition(motion.card.getPosition() + sf::Vector2f(8.f, 10.f));
                    remoteShadow.setFillColor(sf::Color(0, 0, 0, 70));
                    remoteShadow.setOutlineThickness(0.f);
                    window.draw(remoteShadow);
                    window.draw(motion.card);
                }
            }

            if (actionRevealActive)
            {
                drawActionRevealOverlay(window, font, textures, activeActionReveal, actionRevealClock.getElapsedTime().asSeconds(), activeActionRevealDuration);
            }

            if (roundFinished)
            {
                drawRoundResultsPanel(window, font, gameState, client.getPlayerId());
                screwConfirmOpen = false;
            }
            else
            {
                bool screwAllowed = canCallScrew(gameState, client.getPlayerId());
                bool screwHovered = screwButton.getGlobalBounds().contains(mouse);
                screwHoverAmount = approach(screwHoverAmount, screwHovered && !screwConfirmOpen ? 1.f : 0.f, screwHovered ? 9.f : 7.f, dt);
                screwButtonText.setFillColor(screwAllowed ? sf::Color::White : TEXT_SECONDARY);
                drawButton(
                    window,
                    screwButton,
                    screwButtonText,
                    screwHovered && screwAllowed && !screwConfirmOpen,
                    screwAllowed ? sf::Color(158, 69, 58) : sf::Color(55, 61, 57),
                    screwAllowed ? sf::Color(188, 82, 68) : sf::Color(55, 61, 57));
                drawTooltip(window, font, "This ends the turn.", { 748.f, 552.f }, screwHoverAmount);
            }

            drawClickPulses(window, clickPulses, uiTime);
        }

        if (screwConfirmOpen && state == AppState::Playing)
        {
            sf::RectangleShape overlay({ 1000.f, 700.f });
            overlay.setFillColor(sf::Color(0, 0, 0, 112));
            window.draw(overlay);

            drawPanel(window, { 310.f, 250.f }, { 380.f, 210.f }, TABLE_PANEL_DARK, ACCENT_GOLD);

            sf::Text title(font, "Call Screw?", 30);
            title.setFillColor(ACCENT_GOLD);
            title.setPosition({ 414.f, 278.f });
            window.draw(title);

            sf::Text body(font, "This ends your turn and starts final turns.", 19);
            body.setFillColor(TEXT_PRIMARY);
            fitTextToWidth(body, 320.f, 13);
            body.setPosition({ 344.f, 330.f });
            window.draw(body);

            sf::Vector2f confirmMouse = window.mapPixelToCoords(sf::Mouse::getPosition(window));
            drawButton(window, screwConfirmYesButton, screwConfirmYesText, screwConfirmYesButton.getGlobalBounds().contains(confirmMouse), sf::Color(158, 69, 58), sf::Color(188, 82, 68));
            drawButton(window, screwConfirmNoButton, screwConfirmNoText, screwConfirmNoButton.getGlobalBounds().contains(confirmMouse), ACCENT_BLUE, sf::Color(90, 130, 122));
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
        configureDllSearchPath();
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
