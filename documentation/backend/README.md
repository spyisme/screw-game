# Skrew Backend

Flask backend for the multiplayer card game Skrew. The backend is the source of truth for rooms, cards, turns, draws, hidden card visibility, and valid actions.

## Install

```bash
cd backend
python -m venv .venv
.venv\Scripts\activate
pip install -r requirements.txt
```

## Run

```bash
python app.py
```

The server runs on port `5000`.

## API Examples

### Create Room

```bash
curl -X POST http://localhost:5000/rooms ^
  -H "Content-Type: application/json" ^
  -d "{\"is_public\":true,\"max_players\":4}"
```

### Join Room

```bash
curl -X POST http://localhost:5000/rooms/ROOM_ID_OR_CODE/join ^
  -H "Content-Type: application/json" ^
  -d "{\"nickname\":\"Amr\"}"
```

Join at least two players before starting.

### View Public Rooms

```bash
curl http://localhost:5000/rooms
```

You can also use:

```bash
curl http://localhost:5000/rooms/public
```

### Start Game

```bash
curl -X POST http://localhost:5000/rooms/ROOM_ID/start ^
  -H "Content-Type: application/json" ^
  -d "{\"player_id\":\"HOST_PLAYER_ID\"}"
```

### Get Player-Specific State

```bash
curl "http://localhost:5000/rooms/ROOM_ID/state?player_id=PLAYER_ID"
```

The response only reveals cards known to the requesting player. At game start, each player only knows their own cards at indexes `2` and `3`. As soon as any player takes a turn action, all players' cards are hidden again.

### Draw From Deck

```bash
curl -X POST http://localhost:5000/rooms/ROOM_ID/actions ^
  -H "Content-Type: application/json" ^
  -d "{\"player_id\":\"PLAYER_ID\",\"action\":\"draw_from_deck\",\"payload\":{}}"
```

### Swap Drawn Card

```bash
curl -X POST http://localhost:5000/rooms/ROOM_ID/actions ^
  -H "Content-Type: application/json" ^
  -d "{\"player_id\":\"PLAYER_ID\",\"action\":\"swap_drawn_with_own_card\",\"payload\":{\"own_card_index\":0}}"
```

### Drop Drawn Card

```bash
curl -X POST http://localhost:5000/rooms/ROOM_ID/actions ^
  -H "Content-Type: application/json" ^
  -d "{\"player_id\":\"PLAYER_ID\",\"action\":\"drop_drawn_card\",\"payload\":{}}"
```

If the dropped card was drawn from the draw pile during this same turn and is an action card, the turn moves to `waiting_for_action_target` instead of ending immediately.

### Take Discard And Swap

```bash
curl -X POST http://localhost:5000/rooms/ROOM_ID/actions ^
  -H "Content-Type: application/json" ^
  -d "{\"player_id\":\"PLAYER_ID\",\"action\":\"take_discard_and_swap\",\"payload\":{\"own_card_index\":1}}"
```

Taking an action card from the discard pile does not activate it. Discarding an action card that was already in your hand also does not activate it.

### Resolve Give And Take

Available only after the current player draws `GiveAndTake` from the draw pile and drops it in the same turn.

```bash
curl -X POST http://localhost:5000/rooms/ROOM_ID/actions ^
  -H "Content-Type: application/json" ^
  -d "{\"player_id\":\"PLAYER_ID\",\"action\":\"resolve_give_and_take\",\"payload\":{\"own_card_index\":0,\"target_player_id\":\"OPPONENT_PLAYER_ID\",\"target_card_index\":2}}"
```

### See And Swap: Reveal Target

Available only after the current player draws `SeeAndSwab` from the draw pile and drops it in the same turn.

```bash
curl -X POST http://localhost:5000/rooms/ROOM_ID/actions ^
  -H "Content-Type: application/json" ^
  -d "{\"player_id\":\"PLAYER_ID\",\"action\":\"reveal_see_and_swap_target\",\"payload\":{\"target_player_id\":\"TARGET_PLAYER_ID\",\"target_card_index\":1}}"
```

The revealed card is visible only to the current player in `pending_action.revealed_card` and in that card slot inside `players`.

### See And Swap: Swap Or Cancel

```bash
curl -X POST http://localhost:5000/rooms/ROOM_ID/actions ^
  -H "Content-Type: application/json" ^
  -d "{\"player_id\":\"PLAYER_ID\",\"action\":\"resolve_see_and_swap\",\"payload\":{\"own_card_index\":3}}"
```

```bash
curl -X POST http://localhost:5000/rooms/ROOM_ID/actions ^
  -H "Content-Type: application/json" ^
  -d "{\"player_id\":\"PLAYER_ID\",\"action\":\"cancel_see_and_swap\",\"payload\":{}}"
```

## Current Rules

- Rooms use in-memory storage only.
- Public waiting rooms can be listed with `GET /rooms/public`.
- Private rooms can be joined by room code.
- Only the host can start a room.
- A game can start with at least 2 players.
- The implemented deck currently has number cards plus two action cards:
  - `GiveAndTake`, 3 copies
  - `SeeAndSwab`, 3 copies
- Action cards activate only when both conditions are true:
  1. The current player drew the action card from the draw pile.
  2. The current player dropped that drawn card to the discard pile in the same turn.
- See [action_cards.md](action_cards.md) for the full action-card game flow.

## Turn Management

- Each player's turn progresses through actions:
  1. Draw from deck (or take from discard pile)
  2. Swap the drawn card with one of their own cards, or drop the drawn card
- After completing their action, the turn automatically ends and passes to the next player in rotation.
- If a drawn action card is dropped, the current player resolves that action first, then the turn passes to the next player.
- The game state response includes both `current_player_id` and `current_player_nickname` to identify whose turn it is
- Only the current player can perform actions; other players receive an empty `allowed_actions` list until their turn

## Card Source Tracking

Cards returned in `visible_state()` include movement metadata so the frontend can know where each card came from and where it moved to.

Each moved card includes:

- `source`: simple source zone, for example `pile`, `hand`, or `discard`
- `destination`: simple destination zone, for example `hand`, `discard`, or `pending_drawn_card`
- `from`: structured source location
- `to`: structured destination location

Hand locations include both `player_id` and `index`. Action swaps also include a `swap_info` object with `{ action, from_player_id, from_index, to_player_id, to_index }`.

Example card:

```json
{
  "id": "card-789",
  "type": "number",
  "value": 3,
  "image_name": "3.png",
  "source": "pile",
  "destination": "hand",
  "from": { "zone": "pile" },
  "to": { "zone": "hand", "player_id": "player-1", "index": 3 }
}
```

See [CARD_TRACKING.md](CARD_TRACKING.md) for scenario response examples.
