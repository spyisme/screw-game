from __future__ import annotations

from dataclasses import dataclass, field
from datetime import datetime, timezone
from typing import List

from game.game_state import GameState
from game.player import Player


@dataclass
class Room:
    room_id: str
    room_code: str
    is_public: bool
    max_players: int
    host_player_id: str | None = None
    status: str = "waiting"
    players: List[Player] = field(default_factory=list)
    created_at: str = field(default_factory=lambda: datetime.now(timezone.utc).isoformat())
    game_state: GameState = field(default_factory=GameState)

    def get_player(self, player_id: str) -> Player | None:
        return next((player for player in self.players if player.player_id == player_id), None)

    def has_nickname(self, nickname: str) -> bool:
        return any(player.nickname.lower() == nickname.lower() for player in self.players)

    def lobby_dict(self) -> dict:
        return {
            "room_id": self.room_id,
            "room_code": self.room_code,
            "status": self.status,
            "players": [player.lobby_dict() for player in self.players],
            "host_player_id": self.host_player_id,
            "is_public": self.is_public,
            "max_players": self.max_players,
            "created_at": self.created_at,
        }
