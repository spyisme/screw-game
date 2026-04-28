from __future__ import annotations

import random
import uuid
from typing import List

from game.cards import Card, CardType


def _new_card(card_type: CardType, value: int | None, image_name: str) -> Card:
    return Card(
        id=str(uuid.uuid4()),
        type=card_type,
        value=value,
        image_name=image_name,
    )


def create_deck() -> List[Card]:
    deck: List[Card] = []

    for value in range(1, 7): # 1 to 6
        for _ in range(6):
            deck.append(_new_card(CardType.NUMBER, value, f"{value}.png"))

    for value in range(7, 11): # 7 to 10
        for _ in range(4):
            deck.append(_new_card(CardType.NUMBER, value, f"{value}.png"))


    for _ in range(2): #Green skrew
        deck.append(_new_card(CardType.NUMBER, 0, "0.png"))

    for _ in range(2): # - 1
        deck.append(_new_card(CardType.NUMBER, -1, "-1.png"))

    for _ in range(2):
        deck.append(_new_card(CardType.NUMBER, 20, "20.png"))

    for _ in range(2):
        deck.append(_new_card(CardType.NUMBER, 25, "25.png"))



    for _ in range(3):
        deck.append(_new_card(CardType.GIVE_AND_TAKE, None, "GiveAndTake.png"))

    for _ in range(3):
        # Leave the typo too lazy to fix everywhere else in the client and server
        deck.append(_new_card(CardType.SEE_AND_SWAB, None, "Seeandswab.png"))

    return deck


def shuffled_deck() -> List[Card]:
    deck = create_deck()
    random.shuffle(deck)
    return deck
