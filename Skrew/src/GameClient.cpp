#include "../include/GameClient.h"

#include <iostream>
#include <vector>
#include "../include/third_party/httplib.h"

GameClient::GameClient(const std::string& host, int port)
{
    this->host = host;
    this->port = port;
}

void GameClient::setError(const std::string& message)
{
    lastError = message;
    std::cout << "GameClient error: " << lastError << "\n";
}

std::string GameClient::findStringField(const nlohmann::json& data, const std::vector<std::string>& names) const
{
    if (!data.is_object())
    {
        return "";
    }

    for (const std::string& name : names)
    {
        if (data.contains(name))
        {
            if (data[name].is_string())
            {
                return data[name].get<std::string>();
            }

            if (data[name].is_number_integer())
            {
                return std::to_string(data[name].get<int>());
            }
        }
    }

    for (auto it = data.begin(); it != data.end(); ++it)
    {
        if (it.value().is_object())
        {
            std::string value = findStringField(it.value(), names);
            if (!value.empty())
            {
                return value;
            }
        }
    }

    return "";
}

bool GameClient::postJson(const std::string& path, const nlohmann::json& body, nlohmann::json& responseJson)
{
    httplib::Client httpClient(host, port);
    httpClient.set_connection_timeout(2, 0);
    httpClient.set_read_timeout(5, 0);

    auto response = httpClient.Post(path, body.dump(), "application/json");
    if (!response)
    {
        setError("Could not connect to backend at " + host + ":" + std::to_string(port) + " (" + httplib::to_string(response.error()) + ")");
        return false;
    }

    if (response->status < 200 || response->status >= 300)
    {
        setError("Request failed with status " + std::to_string(response->status) + ": " + response->body);
        return false;
    }

    if (response->body.empty())
    {
        responseJson = nlohmann::json::object();
        lastError.clear();
        return true;
    }

    try
    {
        responseJson = nlohmann::json::parse(response->body);
    }
    catch (const nlohmann::json::parse_error&)
    {
        setError("Backend returned invalid JSON.");
        return false;
    }

    lastError.clear();
    return true;
}

bool GameClient::getJson(const std::string& path, nlohmann::json& responseJson)
{
    httplib::Client httpClient(host, port);
    httpClient.set_connection_timeout(2, 0);
    httpClient.set_read_timeout(5, 0);

    auto response = httpClient.Get(path);
    if (!response)
    {
        setError("Could not connect to backend at " + host + ":" + std::to_string(port) + " (" + httplib::to_string(response.error()) + ")");
        return false;
    }

    if (response->status < 200 || response->status >= 300)
    {
        setError("Request failed with status " + std::to_string(response->status) + ": " + response->body);
        return false;
    }

    try
    {
        responseJson = nlohmann::json::parse(response->body);
    }
    catch (const nlohmann::json::parse_error&)
    {
        setError("Backend returned invalid JSON.");
        return false;
    }

    lastError.clear();
    return true;
}

bool GameClient::createRoom(bool isPublic, int maxPlayers)
{
    nlohmann::json body;
    body["is_public"] = isPublic;
    body["max_players"] = maxPlayers;

    nlohmann::json responseJson;
    if (!postJson("/rooms", body, responseJson))
    {
        return false;
    }

    std::string newRoomId = findStringField(responseJson, { "room_id", "roomId", "id", "code", "room_code" });
    std::string newRoomCode = findStringField(responseJson, { "room_code", "code" });
    if (newRoomId.empty())
    {
        setError("Create room response did not include a room id.");
        return false;
    }

    roomId = newRoomId;
    roomCode = newRoomCode.empty() ? newRoomId : newRoomCode;
    return true;
}

bool GameClient::fetchRooms(nlohmann::json& roomsJson)
{
    return getJson("/rooms", roomsJson);
}

bool GameClient::fetchLobby(nlohmann::json& lobbyJson)
{
    if (!hasRoom())
    {
        setError("Cannot fetch lobby before joining a room.");
        return false;
    }

    return getJson("/rooms/" + roomId + "/lobby", lobbyJson);
}

bool GameClient::joinRoom(const std::string& roomIdOrCode, const std::string& nickname)
{
    nlohmann::json body;
    body["nickname"] = nickname;

    nlohmann::json responseJson;
    if (!postJson("/rooms/" + roomIdOrCode + "/join", body, responseJson))
    {
        return false;
    }

    std::string newPlayerId = findStringField(responseJson, { "player_id", "playerId" });
    if (newPlayerId.empty() && responseJson.is_object() && responseJson.contains("player"))
    {
        newPlayerId = findStringField(responseJson["player"], { "id" });
    }

    std::string joinedRoomId = findStringField(responseJson, { "room_id", "roomId", "room_code", "code" });
    if (joinedRoomId.empty() && responseJson.is_object() && responseJson.contains("room"))
    {
        joinedRoomId = findStringField(responseJson["room"], { "id", "code" });
    }

    std::string joinedRoomCode = findStringField(responseJson, { "room_code", "code" });
    if (joinedRoomCode.empty() && responseJson.is_object() && responseJson.contains("room"))
    {
        joinedRoomCode = findStringField(responseJson["room"], { "room_code", "code" });
    }

    if (newPlayerId.empty())
    {
        setError("Join room response did not include a player id.");
        return false;
    }

    roomId = joinedRoomId.empty() ? roomIdOrCode : joinedRoomId;
    roomCode = joinedRoomCode.empty() ? roomId : joinedRoomCode;
    playerId = newPlayerId;

    // Some backends only expose full player state after the game starts.
    if (!refreshState())
    {
        latestState = nlohmann::json::object();
        lastError.clear();
    }

    return true;
}

bool GameClient::startGame()
{
    if (!hasRoom() || !hasPlayer())
    {
        setError("Cannot start game before joining a room.");
        return false;
    }

    nlohmann::json body;
    body["player_id"] = playerId;

    nlohmann::json responseJson;
    if (!postJson("/rooms/" + roomId + "/start", body, responseJson))
    {
        return false;
    }

    return refreshState();
}

bool GameClient::refreshState()
{
    if (!hasRoom() || !hasPlayer())
    {
        setError("Cannot refresh state before joining a room.");
        return false;
    }

    nlohmann::json responseJson;
    if (!getJson("/rooms/" + roomId + "/state?player_id=" + playerId, responseJson))
    {
        return false;
    }

    latestState = responseJson;

    std::string stateRoomCode = findStringField(latestState, { "room_code", "code" });
    if (!stateRoomCode.empty())
    {
        roomCode = stateRoomCode;
    }

    return true;
}

bool GameClient::sendAction(const std::string& action, const nlohmann::json& payload)
{
    if (!hasRoom() || !hasPlayer())
    {
        setError("Cannot send action before joining a room.");
        return false;
    }

    nlohmann::json body;
    body["player_id"] = playerId;
    body["action"] = action;
    body["payload"] = payload;

    nlohmann::json responseJson;
    if (!postJson("/rooms/" + roomId + "/actions", body, responseJson))
    {
        return false;
    }

    return refreshState();
}

bool GameClient::drawFromDeck()
{
    return sendAction("draw_from_deck", nlohmann::json::object());
}

bool GameClient::dropDrawnCard()
{
    return sendAction("drop_drawn_card", nlohmann::json::object());
}

bool GameClient::swapDrawnWithOwnCard(int ownCardIndex)
{
    nlohmann::json payload;
    payload["own_card_index"] = ownCardIndex;
    return sendAction("swap_drawn_with_own_card", payload);
}

bool GameClient::takeDiscardAndSwap(int ownCardIndex)
{
    nlohmann::json payload;
    payload["own_card_index"] = ownCardIndex;
    return sendAction("take_discard_and_swap", payload);
}

bool GameClient::resolveGiveAndTake(int ownCardIndex, const std::string& targetPlayerId, int targetCardIndex)
{
    nlohmann::json payload;
    payload["own_card_index"] = ownCardIndex;
    payload["target_player_id"] = targetPlayerId;
    payload["target_card_index"] = targetCardIndex;
    return sendAction("resolve_give_and_take", payload);
}

bool GameClient::revealSeeAndSwapTarget(const std::string& targetPlayerId, int targetCardIndex)
{
    nlohmann::json payload;
    payload["target_player_id"] = targetPlayerId;
    payload["target_card_index"] = targetCardIndex;
    return sendAction("reveal_see_and_swap_target", payload);
}

bool GameClient::resolveSeeAndSwap(int ownCardIndex)
{
    nlohmann::json payload;
    payload["own_card_index"] = ownCardIndex;
    return sendAction("resolve_see_and_swap", payload);
}

bool GameClient::cancelSeeAndSwap()
{
    return sendAction("cancel_see_and_swap", nlohmann::json::object());
}

const nlohmann::json& GameClient::getState() const
{
    return latestState;
}

std::string GameClient::getRoomId() const
{
    return roomId;
}

std::string GameClient::getRoomCode() const
{
    return roomCode.empty() ? roomId : roomCode;
}

std::string GameClient::getPlayerId() const
{
    return playerId;
}

std::string GameClient::getLastError() const
{
    return lastError;
}

bool GameClient::hasRoom() const
{
    return !roomId.empty();
}

bool GameClient::hasPlayer() const
{
    return !playerId.empty();
}
