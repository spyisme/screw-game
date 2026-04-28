# Card Tracking

Every card response now includes the latest movement pair:

- `source`: simple zone name the card came from.
- `destination`: simple zone name the card moved to.
- `from`: structured source location.
- `to`: structured destination location.

Hand locations always include `player_id` and `index`.

## Location Shape

```json
{
  "source": "pile",
  "destination": "hand",
  "from": { "zone": "pile" },
  "to": { "zone": "hand", "player_id": "player-1", "index": 3 }
}
```

Known zones:

- `pile`
- `pending_drawn_card`
- `hand`
- `discard`

## Scenario Responses

### Start Game: Pile To Hand

Initial dealt cards are tracked as cards moving from the draw pile to each player's hand index.

```json
{
  "index": 3,
  "face_down": false,
  "card": {
    "id": "card-104",
    "type": "number",
    "value": 8,
    "image_name": "8.png",
    "source": "pile",
    "destination": "hand",
    "from": { "zone": "pile" },
    "to": { "zone": "hand", "player_id": "player-1", "index": 3 }
  }
}
```

### Draw Normal Card: Pile To Pending

`draw_from_deck` returns the drawn card in `pending_drawn_card`.

```json
{
  "pending_drawn_card": {
    "id": "draw-7",
    "type": "number",
    "value": 7,
    "image_name": "7.png",
    "source": "pile",
    "destination": "pending_drawn_card",
    "from": { "zone": "pile" },
    "to": { "zone": "pending_drawn_card", "player_id": "player-1" }
  }
}
```

### Swap Drawn Card: Pile To Hand

`swap_drawn_with_own_card` moves the pending drawn card into the selected hand index.

```json
{
  "id": "draw-7",
  "type": "number",
  "value": 7,
  "image_name": "7.png",
  "source": "pile",
  "destination": "hand",
  "from": { "zone": "pile" },
  "to": { "zone": "hand", "player_id": "player-1", "index": 3 }
}
```

### Replaced Own Card: Hand To Discard

The card that was already at `own_card_index` moves to the discard pile.

```json
{
  "top_discard_card": {
    "id": "old-hand-card",
    "type": "number",
    "value": 8,
    "image_name": "8.png",
    "source": "hand",
    "destination": "discard",
    "from": { "zone": "hand", "player_id": "player-1", "index": 3 },
    "to": { "zone": "discard" }
  }
}
```

### Drop Normal Drawn Card: Pile To Discard

`drop_drawn_card` moves the drawn card directly from pile to discard.

```json
{
  "top_discard_card": {
    "id": "draw-8",
    "type": "number",
    "value": 8,
    "image_name": "8.png",
    "source": "pile",
    "destination": "discard",
    "from": { "zone": "pile" },
    "to": { "zone": "discard" }
  }
}
```

### Take Discard And Swap: Discard To Hand

`take_discard_and_swap` moves the top discard card to the selected hand index.

```json
{
  "id": "discard-9",
  "type": "number",
  "value": 9,
  "image_name": "9.png",
  "source": "discard",
  "destination": "hand",
  "from": { "zone": "discard" },
  "to": { "zone": "hand", "player_id": "player-1", "index": 1 }
}
```

The replaced hand card moves to discard:

```json
{
  "top_discard_card": {
    "id": "replaced-card",
    "type": "number",
    "value": 6,
    "image_name": "6.png",
    "source": "hand",
    "destination": "discard",
    "from": { "zone": "hand", "player_id": "player-1", "index": 1 },
    "to": { "zone": "discard" }
  }
}
```

### Drop Give And Take: Pile To Discard

Action cards keep the same movement shape when dropped. Dropping a drawn action card also starts `waiting_for_action_target`.

```json
{
  "phase": "waiting_for_action_target",
  "message": "Give and Take activated",
  "top_discard_card": {
    "id": "give-and-take-card",
    "type": "give_and_take",
    "value": null,
    "image_name": "give_and_take.png",
    "source": "pile",
    "destination": "discard",
    "from": { "zone": "pile" },
    "to": { "zone": "discard" }
  },
  "pending_action": {
    "type": "give_and_take",
    "card_id": "give-and-take-card"
  }
}
```

### Give And Take: Hand To Hand

`resolve_give_and_take` swaps one current-player card with one opponent card. Both moved cards include hand indexes.

```json
{
  "id": "opponent-card",
  "type": "number",
  "value": 20,
  "image_name": "20.png",
  "source": "hand",
  "destination": "hand",
  "from": { "zone": "hand", "player_id": "player-2", "index": 2 },
  "to": { "zone": "hand", "player_id": "player-1", "index": 0 },
  "swap_info": {
    "action": "give_and_take",
    "from_player_id": "player-2",
    "from_index": 2,
    "to_player_id": "player-1",
    "to_index": 0
  }
}
```

The opposite card gets the reverse movement:

```json
{
  "id": "current-player-card",
  "type": "number",
  "value": 4,
  "image_name": "4.png",
  "source": "hand",
  "destination": "hand",
  "from": { "zone": "hand", "player_id": "player-1", "index": 0 },
  "to": { "zone": "hand", "player_id": "player-2", "index": 2 },
  "swap_info": {
    "action": "give_and_take",
    "from_player_id": "player-1",
    "from_index": 0,
    "to_player_id": "player-2",
    "to_index": 2
  }
}
```

### Drop See And Swab: Pile To Discard

```json
{
  "phase": "waiting_for_action_target",
  "message": "See and Swap activated",
  "top_discard_card": {
    "id": "see-and-swab-card",
    "type": "see_and_swab",
    "value": null,
    "image_name": "see_and_swab.png",
    "source": "pile",
    "destination": "discard",
    "from": { "zone": "pile" },
    "to": { "zone": "discard" }
  },
  "pending_action": {
    "type": "see_and_swab",
    "card_id": "see-and-swab-card"
  }
}
```

### See And Swap Reveal

The reveal response shows the target card without moving it. Its tracking fields remain whatever its latest movement was.

```json
{
  "message": "See and Swap target revealed",
  "pending_action": {
    "type": "see_and_swab",
    "card_id": "see-and-swab-card",
    "revealed_card": {
      "card_owner_id": "player-2",
      "card_index": 1,
      "card": {
        "id": "target-card",
        "type": "number",
        "value": 6,
        "image_name": "6.png",
        "source": "pile",
        "destination": "hand",
        "from": { "zone": "pile" },
        "to": { "zone": "hand", "player_id": "player-2", "index": 1 }
      }
    }
  }
}
```

### See And Swap Resolve: Hand To Hand

`resolve_see_and_swap` moves the revealed target card to the current player's selected index.

```json
{
  "id": "target-card",
  "type": "number",
  "value": 6,
  "image_name": "6.png",
  "source": "hand",
  "destination": "hand",
  "from": { "zone": "hand", "player_id": "player-2", "index": 1 },
  "to": { "zone": "hand", "player_id": "player-1", "index": 3 },
  "swap_info": {
    "action": "see_and_swap",
    "from_player_id": "player-2",
    "from_index": 1,
    "to_player_id": "player-1",
    "to_index": 3
  }
}
```

The current player's swapped card gets the reverse movement:

```json
{
  "id": "current-player-card",
  "type": "number",
  "value": 7,
  "image_name": "7.png",
  "source": "hand",
  "destination": "hand",
  "from": { "zone": "hand", "player_id": "player-1", "index": 3 },
  "to": { "zone": "hand", "player_id": "player-2", "index": 1 },
  "swap_info": {
    "action": "see_and_swap",
    "from_player_id": "player-1",
    "from_index": 3,
    "to_player_id": "player-2",
    "to_index": 1
  }
}
```

## Notes For Testing

- Hidden hand slots still obey normal visibility rules, so a moved card may not appear in `players[].cards[].card` after a turn ends.
- `top_discard_card` is always visible and is useful for verifying `hand -> discard` and `pile -> discard`.
- `pending_drawn_card` is visible only to the current player who drew it.
- A card stores its latest movement, not a full historical list.
