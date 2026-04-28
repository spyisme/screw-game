# Skrew

Skrew is a multiplayer card game project with a C++/SFML desktop client and a Flask backend. The backend is the source of truth for rooms, turns, card visibility, card movement, and action validation. The client renders the game and talks to the backend over HTTP.

Mainly used Codex cli Chatgpt 5.5 medium & high & extra high throught the project . (over 13mil token)
## Repository Layout

```text
backend/          Flask API, game rules, room storage, debug pages
client/Skrew/     C++ Visual Studio/SFML client project
documentation/    API notes, card tracking docs, summaries, and rule details
build.ps1         Windows packaging script for the client release output
Skrew.slnx        Visual Studio solution
```

## Backend Setup

Requirements:

- Python 3.12 or newer
- `pip`

From the repo root:

```powershell
cd backend
python -m venv .venv
.\.venv\Scripts\Activate.ps1
pip install -r requirements.txt
python app.py
```

The backend runs on:

```text
http://127.0.0.1:5000
```

Useful debug pages:

```text
http://127.0.0.1:5000/debug/rooms
```

## Client Setup

Requirements:

- Windows
- Visual Studio with C++ desktop development tools
- MSVC toolset matching the project (`v145`)
- SFML 3.x

The current project file expects SFML at:

```text
C:\Users\Spy\source\SFML-3.0.2
```

If SFML is installed somewhere else, update these values in `client/Skrew/Skrew.vcxproj`:

- `AdditionalIncludeDirectories`
- `AdditionalLibraryDirectories`

To run the client:

1. Start the backend first.
2. Open `Skrew.slnx` in Visual Studio.
3. Select `x64` and either `Debug` or `Release`.
4. Build and run the `Skrew` project.

The client connection settings are in:

```text
client/Skrew/config.json
```

By default it connects to `127.0.0.1:5000`.

## Packaging The Client

After building a release executable, run:

```powershell
.\build.ps1
```

The script creates a local `final/` folder and `final.zip`. These are generated files and are ignored by Git.

## How The Game Works

Players join a room, then the host starts the game. Each player has four hidden cards. At the start, each player can see only their own known starting cards. During a turn, the current player either draws from the deck or takes the top discard card, then swaps or drops a card.

The goal is to finish the round with the lowest score. Number cards score their face value, while action cards left in hand count as points. A player can call Screw to start the final-turn phase; every other player gets one last turn, then all cards are revealed and scores are calculated.

Action cards currently include:

- `GiveAndTake`: swap one of your cards with another player's card.
- `SeeAndSwab`: reveal another player's card, then optionally swap with it.

For full backend API examples and rule details, see:

```text
documentation/backend/README.md
documentation/backend/action_cards.md
documentation/backend/SCREW.md
documentation/backend/CARD_TRACKING.md
```
