# Skrew - Codebase Summary

## Project Overview
Skrew is a C++ card game application built with Visual Studio and SFML (Simple and Fast Multimedia Library). The project implements a card game with various card types including number cards and special cards like Basra, GiveAndTake, and SeeAndSwab.

---

## Project Structure

### Root Directory Files

| File | Description |
|------|-------------|
| `README.md` | Project documentation file (currently contains minimal content: "# Skrew") |
| `Skrew.slnx` | Visual Studio solution file that defines the project structure and build configurations (x64, x86 platforms) |
| `.gitignore` | Git ignore configuration file that excludes build artifacts, temporary files, and Visual Studio-specific files from version control |
| `.gitattributes` | Git attributes file that configures line ending normalization and merge behavior for different file types |

---

## Source Code Files

### `Skrew/src/` - Source Implementation Files

| File | Description |
|------|-------------|
| `main.cpp` | Entry point of the application. Contains the main function that initializes and runs the game. Currently includes SFML headers and game loop setup. |
| `Deck.cpp` | Implementation of the Deck class. Handles deck creation (66 cards total), shuffling using random device and Mersenne Twister RNG, and card drawing operations. Creates number cards (-1 to 10, 4 copies each), special number cards (20: 6 copies, 25: 4 copies), and special cards (Basra, GiveAndTake, SeeAndSwab: 4 copies each). |
| `Player.cpp` | Implementation of the Player class. Manages player name and hand of cards. Provides methods to add cards, clear hand, and retrieve player information. |
| `MainMenu.cpp` | Implementation of the MainMenu class. Handles the main menu UI and user interactions for game start and navigation. |

---

## Header Files

### `Skrew/include/` - Header Files

| File | Description |
|------|-------------|
| `Card.h` | Defines the Card structure and CardType enum. CardType includes: Number, Basra, GiveAndTake, Giveonly, and SeeAndSwab. Card struct contains type and value fields with constructors for both number cards and special cards. |
| `Deck.h` | Declares the Deck class interface. Contains private vector of Card objects and public methods: constructor, createDeck(), shuffle(), size(), and drawCard(). |
| `Player.h` | Declares the Player class interface. Contains private members: name (string) and hand (vector of Card). Public methods: constructor, getName(), setName(), addCard(), clearHand(), and getHand(). |
| `MainMenu.h` | Declares the MainMenu class interface. Handles main menu display and user input processing for game navigation. |

---

## Project Configuration Files

### `Skrew/` - Visual Studio Project Files

| File | Description |
|------|-------------|
| `Skrew.vcxproj` | Visual Studio C++ project file that defines build settings, source file references, compiler options, and linker configuration for the Skrew project. |
| `Skrew.vcxproj.filters` | Visual Studio project filter file that organizes source files into logical groups (Source Files, Header Files, Resource Files) for better IDE navigation. |
| `Skrew.vcxproj.user` | User-specific Visual Studio project settings file (auto-generated, contains empty PropertyGroup). |

---

## Asset Files

### `Skrew/assets/` - Game Resources

#### `Skrew/assets/fonts/`
| File | Description |
|------|-------------|
| `arial.ttf` | Arial font file used for text rendering in the game UI. |

#### `Skrew/assets/cards/` - Card Images
| File | Description |
|------|-------------|
| `back.png` | Card back image used for face-down cards. |
| `-1.png` | Image for -1 value card. |
| `0.png` | Image for 0 value card. |
| `1.png` through `10.png` | Images for number cards 1-10. |
| `20.png` | Image for 20 value card (special high-value card). |
| `25.png` | Image for 25 value card (special high-value card). |
| `Basra.png` | Image for Basra special card. |
| `Seeandswab.png` | Image for SeeAndSwab special card. |
| `5odbs.png` | Image for a special card variant (5 of something). |
| `5odwHat.png` | Image for a special card variant (5 of something with hat). |

---

## Build Artifacts (Generated Files)

### `x64/Debug/` - Debug Build Output
| File | Description |
|------|-------------|
| `sfml-system-d-3.dll` | SFML System library (debug version). |
| `sfml-window-d-3.dll` | SFML Window library (debug version). |
| `sfml-network-d-3.dll` | SFML Network library (debug version). |
| `sfml-graphics-d-3.dll` | SFML Graphics library (debug version). |
| `sfml-audio-d-3.dll` | SFML Audio library (debug version). |
| `sfml-system-3.dll` | SFML System library (release version). |
| `sfml-window-3.dll` | SFML Window library (release version). |
| `sfml-network-3.dll` | SFML Network library (release version). |
| `sfml-graphics-3.dll` | SFML Graphics library (release version). |
| `sfml-audio-3.dll` | SFML Audio library (release version). |

### `Skrew/x64/Debug/` - Project Build Artifacts
| File | Description |
|------|-------------|
| `Player.obj` | Compiled object file for Player.cpp. |
| `MainMenu.obj` | Compiled object file for MainMenu.cpp. |
| `Deck.obj` | Compiled object file for Deck.cpp. |
| `main.obj` | Compiled object file for main.cpp. |
| `vc145.idb` | Visual C++ incremental database file for debug information. |
| `vc145.pdb` | Program database file for debugging. |
| `Skrew.tlog/` | Build log directory containing CL (compiler) and link command/read/write logs. |
| `Skrew.ilk` | Incremental link file for faster rebuilds. |
| `Skrew.log` | Build log file. |
| `Skrew.exe.recipe` | Executable recipe file for build system. |

---

## Git Configuration

### `.git/` - Git Repository Files
| Directory/File | Description |
|----------------|-------------|
| `.git/` | Git repository directory containing version control metadata, object database, refs, hooks, and configuration. |
| `.git/hooks/` | Directory containing sample Git hook scripts (applypatch-msg, commit-msg, post-update, pre-commit, pre-push, etc.). |
| `.git/objects/` | Git object database storing compressed file contents and commit objects. |
| `.git/refs/` | Git references directory storing branch and tag pointers. |
| `.git/info/exclude` | Local Git ignore file for repository-specific exclusions. |
| `.git/description` | Repository description file. |

---

## IDE Configuration

### `.vs/` - Visual Studio IDE Files
| Directory/File | Description |
|----------------|-------------|
| `.vs/` | Visual Studio IDE configuration directory (auto-generated). |
| `.vs/Skrew.slnx/v18/` | Solution-specific settings for Visual Studio 2022. |
| `.vs/Skrew.slnx/v18/ipch/` | IntelliSense precompiled header cache directory. |
| `.vs/Skrew.slnx/v18/Solution.VC.db` | Visual Studio IntelliSense database. |

---

## Card Game Mechanics

### Card Types
1. **Number Cards**: Values from -1 to 10, with 4 copies of each value (44 total)
2. **Special Number Cards**:
   - 20: 6 copies
   - 25: 4 copies
3. **Special Cards** (4 copies each):
   - **Basra**: Special effect card
   - **GiveAndTake**: Special effect card
   - **SeeAndSwab**: Special effect card
   - **Giveonly**: Special effect card (defined in Card.h but not used in deck creation)

### Total Deck Composition: 66 cards

---

## Dependencies
- **SFML 3.x**: Multimedia library for graphics, window management, audio, and networking
- **Visual Studio 2022**: Development environment and C++ compiler
- **C++ Standard Library**: STL containers (vector, string) and algorithms

---

## Build System
- **Platform**: Windows (x64, x86)
- **Configuration**: Debug/Release
- **Build Tool**: MSBuild (via Visual Studio)
- **Compiler**: MSVC (Microsoft Visual C++)

---

## File Count Summary
- **Source Files (.cpp)**: 4
- **Header Files (.h)**: 4
- **Asset Files**: 15 (1 font, 14 card images)
- **Project Configuration Files**: 3
- **Solution Files**: 1
- **Git Configuration Files**: 2
- **Build Artifacts**: 20+ (generated)
- **Total Source/Asset Files**: 27
