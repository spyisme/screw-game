#pragma once

#include <string>
#include <vector>
#include "third_party/json.hpp"

class GameClient
{
private:
    std::string host;
    int port;
    std::string roomId;
    std::string roomCode;
    std::string playerId;
    nlohmann::json latestState;
    std::string lastError;

    bool postJson(const std::string& path, const nlohmann::json& body, nlohmann::json& responseJson);
    bool getJson(const std::string& path, nlohmann::json& responseJson);
    bool sendAction(const std::string& action, const nlohmann::json& payload);
    void setError(const std::string& message);
    std::string findStringField(const nlohmann::json& data, const std::vector<std::string>& names) const;

public:
    GameClient(const std::string& host, int port);

    bool createRoom(bool isPublic, int maxPlayers);
    bool fetchRooms(nlohmann::json& roomsJson);
    bool fetchLobby(nlohmann::json& lobbyJson);
    bool joinRoom(const std::string& roomIdOrCode, const std::string& nickname);
    bool startGame();
    bool refreshState();
    bool drawFromDeck();
    bool dropDrawnCard();
    bool callScrew();
    bool swapDrawnWithOwnCard(int ownCardIndex);
    bool takeDiscardAndSwap(int ownCardIndex);
    bool revealOwnCard(int ownCardIndex);
    bool revealOpponentCard(const std::string& targetPlayerId, int targetCardIndex);
    bool resolveGiveAndTake(int ownCardIndex, const std::string& targetPlayerId, int targetCardIndex);
    bool revealSeeAndSwapTarget(const std::string& targetPlayerId, int targetCardIndex);
    bool resolveSeeAndSwap(int ownCardIndex);
    bool cancelSeeAndSwap();

    const nlohmann::json& getState() const;
    std::string getRoomId() const;
    std::string getRoomCode() const;
    std::string getPlayerId() const;
    std::string getLastError() const;
    bool hasRoom() const;
    bool hasPlayer() const;
};
