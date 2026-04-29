# Skrew Detailed Codebase README

Skrew is a multiplayer card game project with two main runtime parts:

- `client/Skrew/`: a Windows C++ desktop client built with SFML that renders the game table and communicates with the backend over HTTP.
- `backend/`: a Flask API that owns rooms, players, cards, turns, visibility rules, scoring, and action validation.


## Client File Inventory

### `client/Skrew/config.json`

Runtime configuration loaded by the C++ client.

- `host`: backend host, default `127.0.0.1`.
- `port`: backend port, default `5000`.
- `width`: window width, default `1000`.
- `height`: window height, default `700`.
- `menu_poll_interval_secs`: lobby/menu polling interval.
- `game_poll_interval_secs`: in-game state refresh interval.

### `client/Skrew/Skrew.vcxproj`

Visual Studio C++ project file.

- Builds a console application.
- Uses C++20.
- Defines Debug/Release and Win32/x64 configurations.
- References SFML include and library directories.
- Links SFML libraries.
- Includes all client `.cpp`, `.h`, and third-party header files.
- In Release x64, runs `build.ps1` as a post-build event.

### `client/Skrew/Skrew.vcxproj.filters`

Visual Studio filter metadata. Organizes files into "Source Files", "Header Files", and "Resource Files" in Solution Explorer. It does not affect runtime behavior.

### `client/Skrew/include/Card.h`

Defines the lightweight local card model used by the old local `Deck` and `Player` classes.

- `CardType`: `Number`, `GiveAndTake`, `SeeAndSwab`.
- `Card`: stores `type` and integer `value`.
- Number constructor sets `CardType::Number`.
- Special-card constructor sets value to `0`.

The live multiplayer client mainly relies on JSON state from the backend, but these types still exist in the project.

### `client/Skrew/include/Deck.h`

Declares the local C++ `Deck` class.

- Holds `std::vector<Card> deckArray`.
- `createDeck()`: fills the local deck.
- `shuffle()`: randomizes local deck order.
- `size()`: returns count.
- `drawCard()`: pops and returns the last card.

This is separate from the authoritative backend deck.

### `client/Skrew/include/Player.h`

Declares a local C++ `Player` class.

- Stores a player name and local vector of `Card`.
- Provides name getter/setter, `addCard()`, `clearHand()`, and `getHand()`.

The networked game identity comes from backend `player_id`; this class is a simple local model.

### `client/Skrew/include/MainMenu.h`

Declares the SFML main menu UI.

- `MenuResult`: `None`, `StartGame`, or `Exit`.
- `MainMenu`: owns title/subtitle text and start/exit button shapes.
- `handleEvent()`: returns a menu result for mouse clicks.
- `draw()`: renders the menu.

### `client/Skrew/include/TextureManager.h`

Declares the texture cache used by rendering code.

- Stores textures in an `unordered_map` keyed by card image base name.
- `getTexture()`: loads a card texture on first use and returns the cached pointer.

### `client/Skrew/include/GameClient.h`

Declares the HTTP API wrapper used by the UI.

Stores:

- backend `host` and `port`
- current `roomId`
- current `roomCode`
- current `playerId`
- `latestState`
- `lastError`

Important methods:

- Room flow: `createRoom()`, `fetchRooms()`, `fetchLobby()`, `joinRoom()`, `startGame()`, `refreshState()`.
- Turn actions: `drawFromDeck()`, `dropDrawnCard()`, `callScrew()`, `swapDrawnWithOwnCard()`, `takeDiscardAndSwap()`.
- Action-card routes: `revealOwnCard()`, `revealOpponentCard()`, `resolveGiveAndTake()`, `revealSeeAndSwapTarget()`, `resolveSeeAndSwap()`, `cancelSeeAndSwap()`.
- Accessors: `getState()`, `getRoomId()`, `getRoomCode()`, `getPlayerId()`, `getLastError()`, `hasRoom()`, `hasPlayer()`.

### `client/Skrew/include/third_party/httplib.h`

Header-only HTTP client/server library. The project uses its client API to send `GET` and `POST` requests to the Flask backend.

### `client/Skrew/include/third_party/json.hpp`

Header-only nlohmann/json library. The client uses it to parse backend responses, construct request bodies, and inspect flexible JSON shapes.

### `client/Skrew/src/GameClient.cpp`

Implements the HTTP client wrapper.

Key behavior:

- Uses `httplib::Client` with short connection/read timeouts.
- `postJson()` and `getJson()` handle request execution, HTTP status validation, JSON parsing, and error messages.
- `findStringField()` searches common field names and nested objects to tolerate backend response shape changes.
- `createRoom()` calls `POST /rooms`.
- `fetchRooms()` calls `GET /rooms`.
- `fetchLobby()` calls `GET /rooms/<room>/lobby`.
- `joinRoom()` calls the join endpoint, stores `playerId`, then tries to refresh state.
- `startGame()` calls the start endpoint, then refreshes state.
- `refreshState()` calls the player-specific state endpoint and updates `latestState`.
- `sendAction()` posts an action, refreshes state, and preserves transient `action_reveal` and `message` fields from the action response.
- Each public action method builds the required payload and delegates to `sendAction()`.

### `client/Skrew/src/TextureManager.cpp`

Implements lazy card texture loading.

Lookup order:

1. `dlls/assets/cards/<fileName>.png`
2. `assets/cards/<fileName>.png`

Loaded textures are smoothed and cached. Missing texture files are reported to stdout and return `nullptr`.

### `client/Skrew/src/Deck.cpp`

Implements the local C++ deck.

- Constructor creates and shuffles the deck.
- `createDeck()` creates local number and special cards.
- `shuffle()` uses `std::random_device`, `std::mt19937`, and `std::shuffle`.
- `drawCard()` pops the last card.

Note: this local deck does not exactly match the current backend deck. The backend deck is authoritative for multiplayer play.

### `client/Skrew/src/Player.cpp`

Implements the local C++ `Player` class.

- Constructor sets the name.
- `getName()` and `setName()` read/write the name.
- `addCard()` appends a card to the local hand.
- `clearHand()` clears the local hand.
- `getHand()` returns a const reference to the hand.

### `client/Skrew/src/MainMenu.cpp`

Implements the SFML main menu.

- Creates title, subtitle, start button, and exit button.
- Centers menu text manually using SFML bounds.
- `handleEvent()` checks mouse clicks against button bounds.
- `draw()` paints a table-themed menu background and hover states.

### `client/Skrew/src/main.cpp`

Main application file for the SFML client. It contains the executable entry point and most UI logic.

Major responsibilities:

- Loads `config.json`.
- Configures DLL search paths for packaged SFML/runtime dependencies.
- Creates the SFML render window.
- Loads the font from `assets/fonts/arial.ttf`.
- Creates `GameClient`, `TextureManager`, and menu/game UI state.
- Manages app states: menu, joining, room selection, waiting room, and playing.
- Polls backend rooms, lobby state, and game state.
- Parses backend JSON into display-friendly room, lobby, card, player, score, action, and turn data.
- Maps backend card objects to texture names.
- Draws the game table, opponent cards, player cards, deck, discard pile, pending drawn card, status bar, hints, overlays, and round results.
- Handles mouse and keyboard input for room creation, joining, starting, drawing, swapping, dropping, targeting action cards, calling Screw, and confirming Screw.
- Animates local card swaps and remote card movements using backend movement metadata.
- Displays temporary action reveals for numeric peek cards.
- Catches fatal exceptions in `main()` and shows a Windows message box.

Because most UI helper functions are local to this file, it is the largest and most central client implementation file.

## Client Asset Inventory

### `client/Skrew/assets/fonts/arial.ttf`

Font used by the SFML UI for menu, lobby, game table, labels, status messages, and result panels.

### `client/Skrew/assets/cards/back.png`

Card-back image used for hidden cards and the deck.

### `client/Skrew/assets/cards/-1.png`

Image for the `-1` number card.

### `client/Skrew/assets/cards/0.png`

Image for the `0` number card.

### `client/Skrew/assets/cards/1.png`

Image for the `1` number card.

### `client/Skrew/assets/cards/2.png`

Image for the `2` number card.

### `client/Skrew/assets/cards/3.png`

Image for the `3` number card.

### `client/Skrew/assets/cards/4.png`

Image for the `4` number card.

### `client/Skrew/assets/cards/5.png`

Image for the `5` number card.

### `client/Skrew/assets/cards/6.png`

Image for the `6` number card.

### `client/Skrew/assets/cards/7.png`

Image for the `7` number card. In backend rules, dropping a freshly drawn `7` activates a temporary own-card peek.

### `client/Skrew/assets/cards/8.png`

Image for the `8` number card. In backend rules, dropping a freshly drawn `8` activates a temporary own-card peek.

### `client/Skrew/assets/cards/9.png`

Image for the `9` number card. In backend rules, dropping a freshly drawn `9` activates a temporary opponent-card peek.

### `client/Skrew/assets/cards/10.png`

Image for the `10` number card. In backend rules, dropping a freshly drawn `10` activates a temporary opponent-card peek.

### `client/Skrew/assets/cards/20.png`

Image for the `20` number card.

### `client/Skrew/assets/cards/25.png`

Image for the `25` number card.

### `client/Skrew/assets/cards/GiveAndTake.png`

Image for the `GiveAndTake` action card. If drawn from deck and dropped immediately, it lets the acting player swap one own card with one opponent card.

### `client/Skrew/assets/cards/Seeandswab.png`

Image for the `SeeAndSwab` action card. The filename intentionally uses this spelling because both backend and client reference it. If drawn from deck and dropped immediately, it lets the acting player reveal a target card, then either swap with it or cancel.

## Client And Backend Contract

The client expects backend state responses to include these core fields:

- `room_id`
- `room_code`
- `status`
- `phase`
- `current_player_id`
- `current_player_nickname`
- `draw_pile_count`
- `top_discard_card`
- `players`
- `pending_drawn_card`
- `pending_action`
- `allowed_actions`
- `round_results` / `results`

Cards may include movement metadata:

- `source`
- `destination`
- `from`
- `to`
- `swap_info`

The client uses this metadata for card movement animations, but gameplay authority remains in `backend/game/engine.py`.

## Important Notes

- Backend storage is process-local memory only.
- Room ids and player ids are UUID strings.
- Room codes are six-character uppercase strings.
- The client can join by room id or room code because `MemoryStore.get_room()` supports both.
- `SeeAndSwab` is consistently misspelled in filenames and enum names; changing it requires coordinated backend, client, and asset updates.
- The backend deck and the old local C++ deck are not identical. The backend deck should be treated as the real deck for multiplayer.
- The docs under `documentation/` contain API notes and scenario examples, but the code in `backend/game/engine.py` is the current source of truth.


The backend is the source of truth. The client stores only the local connection/session data and the latest server response.

## High-Level Architecture

```text
client/Skrew
  C++ SFML app
  Uses httplib for HTTP requests
  Uses nlohmann/json for JSON parsing
  Loads card images and font assets from assets/
        |
        | HTTP JSON
        v
backend
  Flask app
  In-memory room store
  Game engine and rule validation
  Player-specific visible game state
```

## Runtime Requirements

### Backend

- Python 3.12 or newer
- Flask 3.0.3

Run from the repository root:

```powershell
cd backend
python -m venv .venv
.\.venv\Scripts\Activate.ps1
pip install -r requirements.txt
python app.py
```

The backend listens on:

```text
http://127.0.0.1:5000
```

### Client

- Windows
- Visual Studio with C++ desktop workload
- MSVC toolset `v145`
- SFML 3.x

The Visual Studio project currently references SFML at:

```text
C:\Users\Spy\source\SFML-3.0.2
```

If SFML is elsewhere, update `AdditionalIncludeDirectories` and `AdditionalLibraryDirectories` in `client/Skrew/Skrew.vcxproj`.

## Game Flow

1. A player creates a room.
2. Players join by room id or room code.
3. The host starts the game once at least two players are present.
4. Each player receives four cards.
5. At the start, each player can see only their own last two dealt cards.
6. On a normal turn, the current player either draws from the deck or takes from discard.
7. If drawing from deck, the player swaps the pending drawn card with one own card or drops it.
8. If taking from discard, the player swaps that discard card with one own card.
9. Special cards and numeric peek cards activate only when drawn from the deck and dropped in that same turn.
10. A player can call Screw to start final turns.
11. After every other player gets one final turn, all cards are revealed and scores are calculated.

## Backend API Summary

| Method | Path | Purpose |
| --- | --- | --- |
| `GET` | `/health` | Health check returning `{"status":"ok"}`. |
| `GET` | `/` | Attempts to download `final.zip` from the backend folder. |
| `POST` | `/rooms` | Creates a room with `is_public` and `max_players`. |
| `GET` | `/rooms` | Lists public waiting rooms. |
| `GET` | `/rooms/public` | Same public waiting-room list as `/rooms`. |
| `POST` | `/rooms/<room_id_or_code>/join` | Joins a room by id or code. |
| `GET` | `/rooms/<room_id_or_code>/lobby` | Returns waiting-room lobby details. |
| `POST` | `/rooms/<room_id_or_code>/start` | Host starts the game. |
| `GET` | `/rooms/<room_id_or_code>/state?player_id=...` | Returns state visible to one player. |
| `POST` | `/rooms/<room_id_or_code>/actions` | Applies a turn action. |
| `GET` | `/debug/rooms` | HTML page listing rooms in memory. |
| `GET` | `/debug/rooms/<room_id>` | HTML page showing one room's server-side state. |

## Backend Actions

| Action | Payload | Meaning |
| --- | --- | --- |
| `draw_from_deck` | `{}` | Current player draws one card into `pending_drawn_card`. |
| `swap_drawn_with_own_card` | `{"own_card_index": 0}` | Moves pending drawn card into hand and discards the replaced card. |
| `drop_drawn_card` | `{}` | Drops the pending drawn card to discard. May activate a pending action. |
| `take_discard_and_swap` | `{"own_card_index": 0}` | Takes top discard card and swaps with one own card. |
| `call_screw` | `{}` | Starts final-turn phase. |
| `resolve_give_and_take` | `{"own_card_index": 0, "target_player_id": "...", "target_card_index": 2}` | Swaps one current-player card with one opponent card. |
| `reveal_see_and_swap_target` | `{"target_player_id": "...", "target_card_index": 1}` | Reveals one target card to the acting player. |
| `resolve_see_and_swap` | `{"own_card_index": 3}` | Swaps the revealed target card with one own card. |
| `cancel_see_and_swap` | `{}` | Ends See And Swap without moving cards. |
| `reveal_own_card` | `{"own_card_index": 0}` | Resolves a `7` or `8` numeric peek action. |
| `reveal_opponent_card` | `{"target_player_id": "...", "target_card_index": 2}` | Resolves a `9` or `10` numeric peek action. |

## Current Backend Deck

The active backend deck is defined in `backend/game/deck.py`:

- Number cards `1` through `6`: 6 copies each.
- Number cards `7` through `10`: 4 copies each.
- Number card `0`: 2 copies.
- Number card `-1`: 2 copies.
- Number card `20`: 2 copies.
- Number card `25`: 2 copies.
- `GiveAndTake`: 3 copies.
- `SeeAndSwab`: 3 copies.

Each backend card has a UUID, type, optional value, image filename, and optional latest movement metadata.

## Backend Game Phases

| Phase | Meaning |
| --- | --- |
| `not_started` | Room exists but game has not started. |
| `playing` | Normal turn rotation. |
| `waiting_for_action_target` | A dropped action card or numeric peek is waiting for target selection. |
| `final_turns` | Screw was called and remaining players each get one final turn. |
| `round_finished` | Round is scored and cards are fully revealed. |

## Backend File Inventory

### `backend/app.py`

Creates the Flask app, registers all route blueprints, exposes `/health`, and exposes `/` as a `final.zip` download route if that zip exists in the backend folder. Also starts Flask on `0.0.0.0:5000` when run directly.

### `backend/requirements.txt`

Pins the Python backend dependency:

- `Flask==3.0.3`

### `backend/game/__init__.py`

Package marker for the `game` module. It currently contains no runtime code.

### `backend/game/cards.py`

Defines card data and card type helpers.

- `CardType`: enum values `number`, `give_and_take`, and `see_and_swab`.
- `Card`: frozen dataclass containing `id`, `type`, `value`, `image_name`, source/destination movement fields, and optional `swap_info`.
- `Card.to_dict()`: serializes the card for API responses and includes movement metadata only when present.
- `is_action_card()`: returns true for `GiveAndTake` and `SeeAndSwab`.

### `backend/game/deck.py`

Builds the backend draw deck.

- `_new_card()`: creates a card with a UUID.
- `create_deck()`: creates all number and action cards in the active backend deck.
- `shuffled_deck()`: creates and randomizes a fresh deck.

### `backend/game/player.py`

Defines the server-side player model.

- `Player.player_id`: UUID string assigned on join.
- `Player.nickname`: display name.
- `Player.cards`: four-card hand after game start.
- `Player.pending_drawn_card`: temporary card held after drawing from deck.
- `Player.turns_completed`: turn counter.
- `Player.total_score`: accumulated score across finished rounds.
- `lobby_dict()`: serializes player id and nickname for lobby responses.

### `backend/game/game_state.py`

Defines mutable per-room game state.

- `draw_pile`: remaining deck cards.
- `discard_pile`: discard stack.
- `current_player_id`: whose turn it is.
- `phase`: current phase.
- `pending_action`: target-selection state for special actions.
- `initial_reveal_active`: controls the initial two-card reveal.
- `screw_caller_id`: player who called Screw.
- `final_turn_player_ids`: players still owed a final turn.
- `round_results`: scoring results for the finished round.
- `screw_success`: whether the Screw caller had the lowest score.
- `top_discard_card`: property returning the visible discard card.

### `backend/game/room.py`

Defines a room and lobby behavior.

- Stores room id, room code, public/private flag, max players, host id, status, players, creation time, and `GameState`.
- `get_player()`: finds a player by id.
- `has_nickname()`: case-insensitive duplicate nickname check.
- `lobby_dict()`: serializes lobby data for the client.

### `backend/game/engine.py`

Contains almost all backend rules and validation.

Important public functions:

- `create_room()`: validates room settings and creates a `Room`.
- `join_room()`: validates nickname, capacity, room status, and creates a `Player`.
- `start_game()`: validates host/start requirements, shuffles deck, deals four cards to each player, starts turn order, and places the first discard.
- `apply_action()`: main action dispatcher for all turn actions.
- `visible_state()`: builds a player-specific state response with hidden cards removed.
- `call_screw()`: starts the final-turn phase.
- `start_final_turns()`: computes final-turn order after Screw.
- `complete_turn()`: advances normal or final-turn rotation.
- `finish_round()`: ends the round and builds scoring results.
- `calculate_round_scores()`: totals each player's visible card values internally.
- `build_round_results()`: applies Screw success/failure logic and updates total scores.

Important internal behavior:

- Uses custom `GameError` subclasses for HTTP-safe error messages.
- Uses `_card_with_movement()` to attach latest card movement metadata.
- Hides opponent cards unless a card is specifically revealed to the requesting player.
- Allows `GiveAndTake`, `SeeAndSwab`, and numeric peek actions only after a matching draw-then-drop flow.
- Scores number cards by face value.
- Scores action cards as 10.
- Adds a 25 point penalty to a failed Screw caller.

### `backend/routes/__init__.py`

Package marker for route modules. It currently contains no runtime code.

### `backend/routes/room_routes.py`

Defines room and lobby HTTP endpoints.

- Registers `room_bp`.
- Converts `GameError` exceptions to JSON responses.
- `POST /rooms`: creates a room and stores it in memory.
- `GET /rooms`: lists public waiting rooms.
- `GET /rooms/public`: same as `/rooms`.
- `POST /rooms/<room_id>/join`: joins by room id or room code.
- `GET /rooms/<room_id>/lobby`: returns room lobby data.
- `POST /rooms/<room_id>/start`: starts the game for the host.

### `backend/routes/game_routes.py`

Defines game-state and action endpoints.

- Registers `game_bp`.
- Converts `GameError` exceptions to JSON responses.
- `GET /rooms/<room_id>/state`: returns `visible_state()` for the requesting `player_id`.
- `POST /rooms/<room_id>/actions`: validates request shape and calls `apply_action()`.

### `backend/routes/debug_routes.py`

Defines HTML debug pages under `/debug`.

- `GET /debug/rooms`: lists rooms from memory, newest first.
- `GET /debug/rooms/<room_id>`: shows one room's internal state, players, hands, scores, piles, and current phase.

### `backend/storage/__init__.py`

Package marker for storage modules. It currently contains no runtime code.

### `backend/storage/memory_store.py`

Implements in-memory room storage.

- `MemoryStore.rooms`: dictionary keyed by room UUID.
- `add_room()`: stores a new room.
- `get_room()`: finds by UUID first, then by uppercase room code.
- `all_rooms()`: returns every room.
- `public_waiting_rooms()`: returns public rooms with `status == "waiting"`.
- `store`: process-global singleton used by route modules.

Because storage is in memory, rooms disappear when the backend process restarts.

### `backend/templates/debug_rooms.html`

Jinja template for `/debug/rooms`. Renders a table of room id, room code, status, player count, current turn, and creation time.

### `backend/templates/debug_room.html`

Jinja template for `/debug/rooms/<room_id>`. Renders room metadata, phase, Screw state, pile counts, top discard, and each player's cards, pending drawn card, turns completed, and total score.

### `backend/templates/debug_room_not_found.html`

Jinja template returned when a debug room id is not found. Provides a back link to the debug rooms index.

