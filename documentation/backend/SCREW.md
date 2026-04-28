# Screw Action

Use this action when the current player wants to call Screw and start the final-turn phase.

## Request

```http
POST /rooms/<room_id>/actions
Content-Type: application/json
```

```json
{
  "player_id": "PLAYER_ID",
  "action": "call_screw",
  "payload": {}
}
```

Example with curl:

```bash
curl -X POST http://localhost:5000/rooms/ROOM_ID/actions \
  -H "Content-Type: application/json" \
  -d "{\"player_id\":\"PLAYER_ID\",\"action\":\"call_screw\",\"payload\":{}}"
```

## Successful Response

Calling Screw does not reveal cards immediately. It starts `phase: "final_turns"` and moves the turn to the next player after the caller.

```json
{
  "room_id": "ROOM_ID",
  "status": "playing",
  "phase": "final_turns",
  "current_player_id": "NEXT_PLAYER_ID",
  "screw_caller_id": "PLAYER_ID",
  "screw_success": null,
  "lowest_score": null,
  "final_turn_player_ids": ["NEXT_PLAYER_ID", "OTHER_PLAYER_ID"],
  "results": [],
  "round_results": [],
  "allowed_actions": []
}
```

The caller will not play again this round. Each player in `final_turn_player_ids` gets one normal final turn.

## During Final Turns

The current final-turn player can use:

```json
["draw_from_deck", "take_discard_and_swap"]
```

If they draw from the deck, they then finish the turn with:

```json
["swap_drawn_with_own_card", "drop_drawn_card"]
```

No player can call Screw again during `final_turns`.

## Round Finished Response

After the last final-turn player completes their turn, the backend finishes the round, reveals all cards, calculates scores, applies the Screw penalty if needed, and returns `phase: "round_finished"`.

```json
{
  "room_id": "ROOM_ID",
  "status": "playing",
  "phase": "round_finished",
  "current_player_id": null,
  "screw_caller_id": "PLAYER_ID",
  "screw_success": true,
  "lowest_score": 4,
  "final_turn_player_ids": [],
  "results": [
    {
      "player_id": "PLAYER_ID",
      "nickname": "Amr",
      "cards": [
        {
          "index": 0,
          "card": {
            "id": "CARD_ID",
            "type": "number",
            "value": 1,
            "image_name": "1.png"
          }
        }
      ],
      "raw_round_score": 4,
      "applied_round_score": 4,
      "total_score": 14,
      "is_screw_caller": true,
      "had_lowest_score": true
    }
  ],
  "round_results": [
    {
      "player_id": "PLAYER_ID",
      "nickname": "Amr",
      "cards": [
        {
          "index": 0,
          "card": {
            "id": "CARD_ID",
            "type": "number",
            "value": 1,
            "image_name": "1.png"
          }
        }
      ],
      "raw_round_score": 4,
      "applied_round_score": 4,
      "total_score": 14,
      "is_screw_caller": true,
      "had_lowest_score": true
    }
  ]
}
```

`results` and `round_results` contain the same round result data.

## Scoring

At round end, each player's 4 cards are added together.

Number cards use their direct value:

```text
-1, 0, 1, 2, 10, 20, 25
```

Any action card still in a player's hand counts as `10`.

If the Screw caller has the lowest score, or is tied for the lowest score, the call succeeds and they receive their actual score.

If another player has a lower score, the Screw call fails and the caller receives:

```text
caller raw score + 25
```

All other players always receive their actual score.

## Error Responses

Non-current player:

```json
{
  "error": "Only the current player can call Screw"
}
```

Caller is holding a drawn card:

```json
{
  "error": "Cannot call Screw while holding a drawn card"
}
```

Screw already called:

```json
{
  "error": "Screw has already been called"
}
```

Room is not currently playing:

```json
{
  "error": "Cannot call Screw unless the game is playing"
}
```
