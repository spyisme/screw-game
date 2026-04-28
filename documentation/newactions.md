# Numeric Draw-Then-Discard Actions

Cards `7`, `8`, `9`, and `10` only activate when the current player draws the card from the pile and then discards that same pending drawn card to the ground with `drop_drawn_card`.

They do not activate when taken from the discard pile, swapped into the player's hand, or discarded from the player's hand later.

## Action Rules

- `7` or `8`: the player can look at one of their own cards for `3` seconds.
- `9` or `10`: the player can look at one opponent card for `3` seconds.

The backend returns the revealed card in `action_reveal`. The frontend should show that card to the acting player for `reveal_duration_seconds`, then hide it.

## Step 1: Draw From Pile

Request:

```bash
curl -X POST http://localhost:5000/rooms/ROOM_ID/actions ^
  -H "Content-Type: application/json" ^
  -d "{\"player_id\":\"P1\",\"action\":\"draw_from_deck\",\"payload\":{}}"
```

Example response when the drawn card is `7`:

```json
{
  "phase": "playing",
  "current_player_id": "P1",
  "pending_drawn_card": {
    "id": "CARD_ID",
    "type": "number",
    "value": 7,
    "image_name": "7.png",
    "source": "pile",
    "destination": "pending_drawn_card"
  },
  "pending_action": null,
  "allowed_actions": ["swap_drawn_with_own_card", "drop_drawn_card"]
}
```

## Step 2: Discard The Drawn Card

Request:

```bash
curl -X POST http://localhost:5000/rooms/ROOM_ID/actions ^
  -H "Content-Type: application/json" ^
  -d "{\"player_id\":\"P1\",\"action\":\"drop_drawn_card\",\"payload\":{}}"
```

Example response after dropping `7` or `8`:

```json
{
  "phase": "waiting_for_action_target",
  "current_player_id": "P1",
  "pending_drawn_card": null,
  "top_discard_card": {
    "id": "CARD_ID",
    "type": "number",
    "value": 7,
    "image_name": "7.png",
    "source": "pile",
    "destination": "discard"
  },
  "pending_action": {
    "type": "peek_own_card",
    "card_id": "CARD_ID",
    "card_value": 7,
    "reveal_duration_seconds": 3
  },
  "allowed_actions": ["reveal_own_card"],
  "message": "Peek own card activated"
}
```

Example response after dropping `9` or `10`:

```json
{
  "phase": "waiting_for_action_target",
  "current_player_id": "P1",
  "pending_drawn_card": null,
  "pending_action": {
    "type": "peek_opponent_card",
    "card_id": "CARD_ID",
    "card_value": 9,
    "reveal_duration_seconds": 3
  },
  "allowed_actions": ["reveal_opponent_card"],
  "message": "Peek opponent card activated"
}
```

## Step 3A: Reveal One Of Your Own Cards For 7 Or 8

Request:

```bash
curl -X POST http://localhost:5000/rooms/ROOM_ID/actions ^
  -H "Content-Type: application/json" ^
  -d "{\"player_id\":\"P1\",\"action\":\"reveal_own_card\",\"payload\":{\"own_card_index\":0}}"
```

Expected response shape:

```json
{
  "phase": "playing",
  "current_player_id": "NEXT_PLAYER_ID",
  "pending_action": null,
  "allowed_actions": [],
  "action_reveal": {
    "card_owner_id": "P1",
    "card_index": 0,
    "card": {
      "id": "OWN_CARD_ID",
      "type": "number",
      "value": 4,
      "image_name": "4.png"
    },
    "reveal_duration_seconds": 3
  },
  "message": "Own card revealed"
}
```

## Step 3B: Reveal One Opponent Card For 9 Or 10

Request:

```bash
curl -X POST http://localhost:5000/rooms/ROOM_ID/actions ^
  -H "Content-Type: application/json" ^
  -d "{\"player_id\":\"P1\",\"action\":\"reveal_opponent_card\",\"payload\":{\"target_player_id\":\"P2\",\"target_card_index\":2}}"
```

Expected response shape:

```json
{
  "phase": "playing",
  "current_player_id": "NEXT_PLAYER_ID",
  "pending_action": null,
  "allowed_actions": [],
  "action_reveal": {
    "card_owner_id": "P2",
    "card_index": 2,
    "card": {
      "id": "OPPONENT_CARD_ID",
      "type": "number",
      "value": 10,
      "image_name": "10.png"
    },
    "reveal_duration_seconds": 3
  },
  "message": "Opponent card revealed"
}
```

`target_player_id` must be an opponent for `reveal_opponent_card`.

## Invalid Flows

- Drawing `7`, `8`, `9`, or `10` and using `swap_drawn_with_own_card` does not activate the peek action.
- Taking `7`, `8`, `9`, or `10` from the discard pile with `take_discard_and_swap` does not activate the peek action.
- Calling `reveal_own_card` is only valid after dropping a drawn `7` or `8`.
- Calling `reveal_opponent_card` is only valid after dropping a drawn `9` or `10`.
