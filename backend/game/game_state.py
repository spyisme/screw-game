from __future__ import annotations

from dataclasses import dataclass, field
from typing import Any, List

from game.cards import Card


@dataclass
class GameState:
    draw_pile: List[Card] = field(default_factory=list)
    discard_pile: List[Card] = field(default_factory=list)
    current_player_id: str | None = None
    phase: str = "not_started"
    pending_action: dict[str, Any] | None = None
    initial_reveal_active: bool = True
    screw_caller_id: str | None = None
    final_turn_player_ids: List[str] = field(default_factory=list)
    round_results: List[dict[str, Any]] = field(default_factory=list)
    screw_success: bool | None = None

    @property
    def top_discard_card(self) -> Card | None:
        if not self.discard_pile:
            return None
        return self.discard_pile[-1]
