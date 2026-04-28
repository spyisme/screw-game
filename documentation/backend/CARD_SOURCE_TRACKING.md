# Card Source Tracking Implementation

> This older note described the first source-only implementation. The current response contract tracks both source and destination; see [CARD_TRACKING.md](CARD_TRACKING.md) for the up-to-date schema and scenario response examples.

## Overview

This implementation adds source tracking to cards, allowing the frontend to know where each card came from. Every card in a player's hand now includes metadata about its source and how it was obtained.

## Changes Made

### 1. Modified `game/cards.py`

- Added two optional fields to the `Card` dataclass:
  - `source` (str): Where the card came from
  - `swap_info` (dict): Metadata about the swap (indices, player IDs, action type)
- Updated `to_dict()` method to include source and swap_info in card responses

### 2. Modified `game/engine.py`

- Imported `replace` from dataclasses for creating card copies with source info
- Added helper function `_card_with_source()` to attach source information to cards
- Updated all card movement operations to track source:
  - **`_draw_from_deck()`**: Sets `source="pile"`
  - **`_take_discard_and_swap()`**: Sets `source="ground"`
  - **`_resolve_give_and_take()`**: Sets `source="action_card"` with swap details
  - **`_resolve_see_and_swap()`**: Sets `source="action_card"` with swap details

## Card Source Types

### 1. Pile Source (`source: "pile"`)

When a card is drawn from the draw pile.

**Example:**

```json
{
  "id": "card-123",
  "type": "number",
  "value": 7,
  "image_name": "number_7.png",
  "source": "pile"
}
```

### 2. Ground Source (`source: "ground"`)

When a card is taken from the discard pile.

**Example:**

```json
{
  "id": "card-456",
  "type": "give_and_take",
  "value": null,
  "image_name": "give_and_take.png",
  "source": "ground"
}
```

### 3. Action Card Source (`source: "action_card"`)

When a card is swapped via an action card (Give and Take or See and Swap).

Includes `swap_info` with details about the swap:

- `action`: "give_and_take" or "see_and_swap"
- `from_player_id`: Which player's card was taken
- `from_index`: Which index in the source player's hand
- `to_player_id`: Which player received the card
- `to_index`: Which index in the receiving player's hand

**Example (Give and Take):**

```json
{
  "id": "card-789",
  "type": "number",
  "value": 3,
  "image_name": "number_3.png",
  "source": "action_card",
  "swap_info": {
    "action": "give_and_take",
    "from_player_id": "player-abc123",
    "from_index": 2,
    "to_player_id": "player-xyz789",
    "to_index": 0
  }
}
```

**Example (See and Swap):**

```json
{
  "id": "card-101",
  "type": "number",
  "value": 9,
  "image_name": "number_9.png",
  "source": "action_card",
  "swap_info": {
    "action": "see_and_swap",
    "from_player_id": "player-def456",
    "from_index": 1,
    "to_player_id": "player-xyz789",
    "to_index": 3
  }
}
```

## Complete Response Example

Here's a complete `visible_state()` response showing card source tracking:

```json
{
  "room_id": "550e8400-e29b-41d4-a716-446655440000",
  "room_code": "ABC123",
  "status": "playing",
  "phase": "turn",
  "current_player_id": "player-1",
  "current_player_nickname": "Alice",
  "draw_pile_count": 15,
  "top_discard_card": {
    "id": "card-d1",
    "type": "number",
    "value": 5,
    "image_name": "number_5.png"
  },
  "players": [
    {
      "player_id": "player-1",
      "nickname": "Alice",
      "cards": [
        {
          "index": 0,
          "face_down": false,
          "card": {
            "id": "card-101",
            "type": "number",
            "value": 7,
            "image_name": "number_7.png",
            "source": "pile"
          }
        },
        {
          "index": 1,
          "face_down": false,
          "card": {
            "id": "card-102",
            "type": "give_and_take",
            "value": null,
            "image_name": "give_and_take.png",
            "source": "ground"
          }
        },
        {
          "index": 2,
          "face_down": false,
          "card": {
            "id": "card-103",
            "type": "number",
            "value": 3,
            "image_name": "number_3.png",
            "source": "action_card",
            "swap_info": {
              "action": "give_and_take",
              "from_player_id": "player-2",
              "from_index": 1,
              "to_player_id": "player-1",
              "to_index": 2
            }
          }
        },
        {
          "index": 3,
          "face_down": false,
          "card": {
            "id": "card-104",
            "type": "see_and_swab",
            "value": null,
            "image_name": "see_and_swab.png",
            "source": "action_card",
            "swap_info": {
              "action": "see_and_swap",
              "from_player_id": "player-3",
              "from_index": 0,
              "to_player_id": "player-1",
              "to_index": 3
            }
          }
        }
      ]
    },
    {
      "player_id": "player-2",
      "nickname": "Bob",
      "card_count": 4,
      "cards": [
        {
          "index": 0,
          "face_down": true
        },
        {
          "index": 1,
          "face_down": false,
          "card": {
            "id": "card-201",
            "type": "number",
            "value": 7,
            "image_name": "number_7.png",
            "source": "action_card",
            "swap_info": {
              "action": "give_and_take",
              "from_player_id": "player-1",
              "from_index": 2,
              "to_player_id": "player-2",
              "to_index": 1
            }
          }
        },
        {
          "index": 2,
          "face_down": true
        },
        {
          "index": 3,
          "face_down": true
        }
      ]
    }
  ],
  "pending_drawn_card": null,
  "pending_action": null,
  "allowed_actions": ["draw_from_deck", "take_discard_and_swap"]
}
```

## Backend Flow

1. **Drawing from Pile**: Card gets `source="pile"` and optional `swap_info=None`
2. **Swapping with Hand**: Card retains `source="pile"` when moved to hand
3. **Taking from Ground**: Card gets `source="ground"` and optional `swap_info=None`
4. **Action Card Swap**: Both players' cards get `source="action_card"` with full swap metadata

## Frontend Usage

The frontend can now:

- Display different visual indicators based on card source
- Show player which cards were obtained through action cards
- Track swap history by storing swap_info data
- Highlight recently swapped cards in the UI

## Testing

All existing tests should pass as the changes are backward compatible:

- Cards without source info display normally
- The `source` and `swap_info` fields are optional
- Existing game logic remains unchanged
