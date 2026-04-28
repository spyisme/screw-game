# Action Cards

Action cards are normal cards while they are in a player's hand or on top of the discard pile. They activate only when both conditions are true:

1. The current player draws the action card from the draw pile with `draw_from_deck`.
2. The current player discards that same pending drawn card with `drop_drawn_card` during the same turn.

If either condition is missing, no action-card effect runs. For example, swapping a drawn action card into your hand does not activate it. Taking an action card from the discard pile does not activate it. Discarding an action card that was already in your hand does not activate it.

## State Flow

Normal turn:

1. `draw_from_deck`
2. `swap_drawn_with_own_card` or `drop_drawn_card`
3. Turn ends automatically

Action-card turn:

1. `draw_from_deck`
2. `drop_drawn_card`
3. State changes to `phase: "waiting_for_action_target"`
4. Current player resolves or cancels the action-card route
5. Turn ends automatically

During `waiting_for_action_target`, only the current player has `allowed_actions`. Other players cannot act until the action route finishes and the turn advances.

## Give And Take

Route action: `resolve_give_and_take`

Payload:

```json
{
  "own_card_index": 0,
  "target_player_id": "OPPONENT_PLAYER_ID",
  "target_card_index": 2
}
```

Flow:

1. Current player chooses one of their own cards with `own_card_index`.
2. Current player chooses one opponent card with `target_player_id` and `target_card_index`.
3. The two cards swap immediately.
4. The turn ends.

`target_player_id` must be an opponent. Give And Take cannot target the current player's own cards.

## See And Swap

Step 1 route action: `reveal_see_and_swap_target`

Payload:

```json
{
  "target_player_id": "TARGET_PLAYER_ID",
  "target_card_index": 1
}
```

Flow:

1. Current player chooses a target card.
2. The backend reveals that target card only to the current player.
3. The response includes the revealed card in `pending_action.revealed_card`.
4. The same card is also visible in the target player's `players[].cards[]` entry for the current player's state response.

After the reveal, the current player chooses one of two actions.

Swap route action: `resolve_see_and_swap`

```json
{
  "own_card_index": 3
}
```

This swaps the revealed target card with one of the current player's cards, then ends the turn.

Cancel route action: `cancel_see_and_swap`

```json
{}
```

This keeps all cards where they are and ends the turn.

See And Swap may target any player's card, including the current player's own cards. The backend rejects swapping a card with itself.

## Test Scenario

Assume:

- Room id: `ROOM_ID`
- Current player id: `P1`
- Opponent player id: `P2`
- `P1` wants to swap their card at index `0`
- `P2` has a target card at index `2`

### Scenario A: Player Draws An Action Card And Uses It

This example shows player `P1` drawing from the pile, finding out it is an action card, dropping it, then using the action.

#### 1. Player draws from the pile

```bash
curl -X POST http://localhost:5000/rooms/ROOM_ID/actions ^
  -H "Content-Type: application/json" ^
  -d "{\"player_id\":\"P1\",\"action\":\"draw_from_deck\",\"payload\":{}}"
```

Example response shape if the drawn card is `GiveAndTake`:

```json
{
  "phase": "turn",
  "current_player_id": "P1",
  "pending_drawn_card": {
    "id": "CARD_ID",
    "type": "give_and_take",
    "value": null,
    "image_name": "GiveAndTake.png"
  },
  "allowed_actions": ["swap_drawn_with_own_card", "drop_drawn_card"]
}
```

At this point the action card is not active yet. It is only a pending drawn card.

#### 2. Player drops the drawn action card

```bash
curl -X POST http://localhost:5000/rooms/ROOM_ID/actions ^
  -H "Content-Type: application/json" ^
  -d "{\"player_id\":\"P1\",\"action\":\"drop_drawn_card\",\"payload\":{}}"
```

Example response:

```json
{
  "phase": "waiting_for_action_target",
  "current_player_id": "P1",
  "pending_drawn_card": null,
  "pending_action": {
    "type": "give_and_take",
    "card_id": "CARD_ID"
  },
  "allowed_actions": ["resolve_give_and_take"],
  "message": "Give and Take activated"
}
```

Now the action card is active because both activation conditions happened in the same turn:

1. `P1` drew the card from the draw pile.
2. `P1` dropped that same drawn card.

#### 3. Player uses Give And Take

`P1` chooses one of their cards and one opponent card. The backend swaps them immediately.

```bash
curl -X POST http://localhost:5000/rooms/ROOM_ID/actions ^
  -H "Content-Type: application/json" ^
  -d "{\"player_id\":\"P1\",\"action\":\"resolve_give_and_take\",\"payload\":{\"own_card_index\":0,\"target_player_id\":\"P2\",\"target_card_index\":2}}"
```

Example response shape:

```json
{
  "phase": "turn",
  "current_player_id": "NEXT_PLAYER_ID",
  "pending_action": null,
  "message": "Give and Take resolved"
}
```

The swap is complete, and the turn has passed to the next player.

### Scenario B: Player Draws A Non-Action Card

This example shows player `P1` drawing from the pile and the card is a normal number card.

#### 1. Player draws from the pile

```bash
curl -X POST http://localhost:5000/rooms/ROOM_ID/actions ^
  -H "Content-Type: application/json" ^
  -d "{\"player_id\":\"P1\",\"action\":\"draw_from_deck\",\"payload\":{}}"
```

Example response shape if the drawn card is a normal `7` card:

```json
{
  "phase": "turn",
  "current_player_id": "P1",
  "pending_drawn_card": {
    "id": "CARD_ID",
    "type": "number",
    "value": 7,
    "image_name": "7.png"
  },
  "pending_action": null,
  "allowed_actions": ["swap_drawn_with_own_card", "drop_drawn_card"]
}
```

The player has two choices:

1. Swap the drawn number card with one of their own cards.
2. Drop the drawn number card to the discard pile.

No action-card route is available because the drawn card is not an action card.

#### 2A. Player swaps the drawn number card with one of their cards

```bash
curl -X POST http://localhost:5000/rooms/ROOM_ID/actions ^
  -H "Content-Type: application/json" ^
  -d "{\"player_id\":\"P1\",\"action\":\"swap_drawn_with_own_card\",\"payload\":{\"own_card_index\":0}}"
```

What happens:

1. The drawn `7` goes into `P1` card index `0`.
2. The old card from `P1` card index `0` goes to the discard pile.
3. The turn ends.

Example response shape:

```json
{
  "phase": "turn",
  "current_player_id": "NEXT_PLAYER_ID",
  "pending_drawn_card": null,
  "pending_action": null,
  "top_discard_card": {
    "id": "OLD_CARD_ID",
    "type": "number",
    "value": 4,
    "image_name": "4.png"
  },
  "allowed_actions": []
}
```

The `allowed_actions` list is empty for `P1` because it is no longer `P1`'s turn. The next player will see their own turn actions.

#### 2B. Player drops the drawn number card

```bash
curl -X POST http://localhost:5000/rooms/ROOM_ID/actions ^
  -H "Content-Type: application/json" ^
  -d "{\"player_id\":\"P1\",\"action\":\"drop_drawn_card\",\"payload\":{}}"
```

What happens:

1. The drawn `7` goes to the discard pile.
2. No action-card effect starts.
3. The turn ends.

Example response shape:

```json
{
  "phase": "turn",
  "current_player_id": "NEXT_PLAYER_ID",
  "pending_drawn_card": null,
  "pending_action": null,
  "top_discard_card": {
    "id": "CARD_ID",
    "type": "number",
    "value": 7,
    "image_name": "7.png"
  },
  "allowed_actions": []
}
```

The important difference from an action card is that the phase stays `"turn"` and `pending_action` stays `null`. The backend does not ask for targets.

### Scenario C: Player Draws An Action Card But Keeps It

This example shows why an action card in hand is treated like any normal card.

#### 1. Player draws `SeeAndSwab`

```json
{
  "phase": "turn",
  "current_player_id": "P1",
  "pending_drawn_card": {
    "id": "ACTION_CARD_ID",
    "type": "see_and_swab",
    "value": null,
    "image_name": "Seeandswab.png"
  },
  "pending_action": null,
  "allowed_actions": ["swap_drawn_with_own_card", "drop_drawn_card"]
}
```

#### 2. Player swaps it into their hand instead of dropping it

```bash
curl -X POST http://localhost:5000/rooms/ROOM_ID/actions ^
  -H "Content-Type: application/json" ^
  -d "{\"player_id\":\"P1\",\"action\":\"swap_drawn_with_own_card\",\"payload\":{\"own_card_index\":0}}"
```

Example response shape:

```json
{
  "phase": "turn",
  "current_player_id": "NEXT_PLAYER_ID",
  "pending_drawn_card": null,
  "pending_action": null,
  "allowed_actions": []
}
```

The `SeeAndSwab` card is now in `P1`'s hand. It did not activate because it was not discarded as the pending drawn card in the same turn.
