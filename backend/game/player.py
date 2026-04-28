from __future__ import annotations

from dataclasses import dataclass, field
from typing import List

from game.cards import Card


@dataclass
class Player:
    player_id: str
    nickname: str
    cards: List[Card] = field(default_factory=list)
    pending_drawn_card: Card | None = None
    turns_completed: int = 0
    total_score: int = 0

    def lobby_dict(self) -> dict:
        return {
            "player_id": self.player_id,
            "nickname": self.nickname,
        }
