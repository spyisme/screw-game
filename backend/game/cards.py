from __future__ import annotations

from dataclasses import dataclass
from enum import Enum
from typing import Optional


class CardType(str, Enum):
    NUMBER = "number"
    GIVE_AND_TAKE = "give_and_take"
    SEE_AND_SWAB = "see_and_swab"


@dataclass(frozen=True)
class Card:
    id: str
    type: CardType
    value: Optional[int]
    image_name: str
    source: Optional[str] = None
    destination: Optional[str] = None
    from_location: Optional[dict] = None
    to_location: Optional[dict] = None
    swap_info: Optional[dict] = None

    def to_dict(self) -> dict:
        data = {
            "id": self.id,
            "type": self.type.value,
            "value": self.value,
            "image_name": self.image_name,
        }
        if self.source:
            data["source"] = self.source
        if self.destination:
            data["destination"] = self.destination
        if self.from_location:
            data["from"] = self.from_location
        if self.to_location:
            data["to"] = self.to_location
        if self.swap_info:
            data["swap_info"] = self.swap_info
        return data


def is_action_card(card: Card) -> bool:
    return card.type in {
        CardType.GIVE_AND_TAKE,
        CardType.SEE_AND_SWAB,
    }
