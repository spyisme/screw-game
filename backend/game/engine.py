from __future__ import annotations

from dataclasses import dataclass, replace
import uuid

from game.cards import Card, CardType, is_action_card
from game.deck import shuffled_deck
from game.player import Player
from game.room import Room


PHASE_PLAYING = "playing"
PHASE_FINAL_TURNS = "final_turns"
PHASE_WAITING_FOR_ACTION_TARGET = "waiting_for_action_target"
PHASE_ROUND_FINISHED = "round_finished"

SCREW_FAILED_PENALTY = 25
ACTION_CARD_SCORE_VALUE = 10
NUMERIC_PEEK_DURATION_SECONDS = 3
ACTION_PEEK_OWN_CARD = "peek_own_card"
ACTION_PEEK_OPPONENT_CARD = "peek_opponent_card"
SPECIAL_CARD_SCORE_VALUES = {
    "basra": ACTION_CARD_SCORE_VALUE,
    "Basra": ACTION_CARD_SCORE_VALUE,
    CardType.GIVE_AND_TAKE.value: ACTION_CARD_SCORE_VALUE,
    "GiveAndTake": ACTION_CARD_SCORE_VALUE,
    CardType.SEE_AND_SWAB.value: ACTION_CARD_SCORE_VALUE,
    "SeeAndSwab": ACTION_CARD_SCORE_VALUE,
}


class GameError(Exception):
    status_code = 400

    def __init__(self, message: str, status_code: int | None = None):
        super().__init__(message)
        self.message = message
        if status_code is not None:
            self.status_code = status_code


class BadRequestError(GameError):
    status_code = 400


class ForbiddenError(GameError):
    status_code = 403


class NotFoundError(GameError):
    status_code = 404


class ConflictError(GameError):
    status_code = 409


@dataclass
class ActionResult:
    state: dict
    message: str | None = None

    def to_dict(self) -> dict:
        data = self.state
        if self.message:
            data["message"] = self.message
        return data


def _location(zone: str, player_id: str | None = None, index: int | None = None) -> dict:
    location = {"zone": zone}
    if player_id is not None:
        location["player_id"] = player_id
    if index is not None:
        location["index"] = index
    return location


def _card_with_movement(
    card: Card,
    source: dict,
    destination: dict,
    swap_info: dict | None = None,
) -> Card:
    """Create a new card value with the latest movement metadata."""
    return replace(
        card,
        source=source["zone"],
        destination=destination["zone"],
        from_location=source,
        to_location=destination,
        swap_info=swap_info,
    )


def create_room(is_public: bool, max_players: int) -> Room:
    if not isinstance(is_public, bool):
        raise BadRequestError("is_public must be a boolean")
    if not isinstance(max_players, int):
        raise BadRequestError("max_players must be an integer")
    if max_players < 2:
        raise BadRequestError("max_players must be at least 2")

    room_id = str(uuid.uuid4())
    room_code = uuid.uuid4().hex[:6].upper()
    return Room(
        room_id=room_id,
        room_code=room_code,
        is_public=is_public,
        max_players=max_players,
    )


def join_room(room: Room, nickname: str) -> Player:
    nickname = nickname.strip()
    if not nickname:
        raise BadRequestError("nickname is required")
    if room.status != "waiting":
        raise ConflictError("players can only join rooms that are waiting")
    if len(room.players) >= room.max_players:
        raise ConflictError("room is full")
    if room.has_nickname(nickname):
        raise ConflictError("nickname already exists in this room")

    player = Player(player_id=str(uuid.uuid4()), nickname=nickname)
    room.players.append(player)
    if room.host_player_id is None:
        room.host_player_id = player.player_id
    return player


def start_game(room: Room, player_id: str) -> dict:
    if room.status != "waiting":
        raise ConflictError("game has already started")
    if room.host_player_id != player_id:
        raise ForbiddenError("only the host can start the game")
    if len(room.players) < 2:
        raise ConflictError("at least 2 players are required to start")

    deck = shuffled_deck()
    for player in room.players:
        player.cards = [
            _card_with_movement(
                deck.pop(),
                _location("pile"),
                _location("hand", player.player_id, index),
            )
            for index in range(4)
        ]
        player.pending_drawn_card = None
        player.turns_completed = 0

    room.game_state.draw_pile = deck
    # Place one card on discard pile and keep the movement visible.
    initial_discard = room.game_state.draw_pile.pop()
    room.game_state.discard_pile = [
        _card_with_movement(initial_discard, _location("pile"), _location("discard"))
    ]
    room.game_state.current_player_id = room.players[0].player_id
    room.game_state.phase = PHASE_PLAYING
    room.game_state.pending_action = None
    room.game_state.initial_reveal_active = True
    room.game_state.screw_caller_id = None
    room.game_state.final_turn_player_ids = []
    room.game_state.round_results = []
    room.game_state.screw_success = None
    room.status = "playing"

    return visible_state(room, player_id)


def apply_action(room: Room, player_id: str, action: str, payload: dict | None = None) -> ActionResult:
    payload = payload or {}
    if action == "call_screw":
        return call_screw(room, player_id)

    if room.status != "playing":
        raise ConflictError("game is not playing")

    player = _require_player(room, player_id)
    if room.game_state.phase == PHASE_ROUND_FINISHED:
        raise ConflictError("round is finished")
    _require_current_turn(room, player_id)
    _require_current_player_can_play_phase(room, player_id)
    if room.game_state.phase == PHASE_WAITING_FOR_ACTION_TARGET:
        return _apply_pending_action(room, player, action, payload)

    if action == "draw_from_deck":
        return _draw_from_deck(room, player)
    if action == "swap_drawn_with_own_card":
        return _swap_drawn_with_own_card(room, player, payload)
    if action == "drop_drawn_card":
        return _drop_drawn_card(room, player)
    if action == "take_discard_and_swap":
        return _take_discard_and_swap(room, player, payload)

    raise BadRequestError("unsupported action")


def visible_state(room: Room, requesting_player_id: str) -> dict:
    requester = _require_player(room, requesting_player_id)
    top_discard = room.game_state.top_discard_card
    current_player = _get_current_player(room)
    current_player_nickname = current_player.nickname if current_player else None

    return {
        "room_id": room.room_id,
        "room_code": room.room_code,
        "status": room.status,
        "phase": room.game_state.phase,
        "current_player_id": room.game_state.current_player_id,
        "current_player_nickname": current_player_nickname,
        "screw_caller_id": room.game_state.screw_caller_id,
        "screw_success": room.game_state.screw_success,
        "lowest_score": _lowest_score_from_results(room.game_state.round_results),
        "final_turn_player_ids": list(room.game_state.final_turn_player_ids),
        "round_results": room.game_state.round_results,
        "results": room.game_state.round_results,
        "draw_pile_count": len(room.game_state.draw_pile),
        "top_discard_card": top_discard.to_dict() if top_discard else None,
        "players": [_visible_player(room, player, requester) for player in room.players],
        "pending_drawn_card": requester.pending_drawn_card.to_dict() if requester.pending_drawn_card else None,
        "pending_action": _visible_pending_action(room, requester),
        "allowed_actions": _allowed_actions(room, requester),
    }


def _visible_player(room: Room, player: Player, requester: Player) -> dict:
    revealed_card = _revealed_card_for_requester(room, requester)
    if room.game_state.phase == PHASE_ROUND_FINISHED:
        return {
            "player_id": player.player_id,
            "nickname": player.nickname,
            "cards": [
                {"index": index, "face_down": False, "card": card.to_dict()}
                for index, card in enumerate(player.cards)
            ],
            "total_score": player.total_score,
        }

    if player.player_id != requester.player_id:
        cards = []
        for index, _card in enumerate(player.cards):
            if revealed_card and revealed_card["card_owner_id"] == player.player_id and revealed_card["card_index"] == index:
                cards.append({"index": index, "face_down": False, "card": player.cards[index].to_dict()})
            else:
                cards.append({"index": index, "face_down": True})

        return {
            "player_id": player.player_id,
            "nickname": player.nickname,
            "card_count": len(player.cards),
            "cards": cards,
            "total_score": player.total_score,
        }

    cards = []
    show_initial_hand = room.game_state.initial_reveal_active
    initial_visible_indexes = set(range(max(0, len(player.cards) - 2), len(player.cards)))
    for index, card in enumerate(player.cards):
        if revealed_card and revealed_card["card_owner_id"] == player.player_id and revealed_card["card_index"] == index:
            cards.append({"index": index, "face_down": False, "card": card.to_dict()})
            continue
        if show_initial_hand and index in initial_visible_indexes:
            cards.append({"index": index, "face_down": False, "card": card.to_dict()})
        else:
            cards.append({"index": index, "face_down": True})

    return {
        "player_id": player.player_id,
        "nickname": player.nickname,
        "cards": cards,
        "total_score": player.total_score,
    }


def _allowed_actions(room: Room, player: Player) -> list[str]:
    if room.status != "playing":
        return []
    if room.game_state.current_player_id != player.player_id:
        return []
    if room.game_state.phase == PHASE_WAITING_FOR_ACTION_TARGET:
        pending_action = room.game_state.pending_action or {}
        action_type = pending_action.get("type")
        if action_type == CardType.GIVE_AND_TAKE.value:
            return ["resolve_give_and_take"]
        if action_type == CardType.SEE_AND_SWAB.value:
            if pending_action.get("revealed_card"):
                return ["resolve_see_and_swap", "cancel_see_and_swap"]
            return ["reveal_see_and_swap_target"]
        if action_type == ACTION_PEEK_OWN_CARD:
            return ["reveal_own_card"]
        if action_type == ACTION_PEEK_OPPONENT_CARD:
            return ["reveal_opponent_card"]
        return []
    if room.game_state.phase == PHASE_ROUND_FINISHED:
        return []
    if room.game_state.phase == PHASE_FINAL_TURNS and player.player_id not in room.game_state.final_turn_player_ids:
        return []
    if player.pending_drawn_card:
        return ["swap_drawn_with_own_card", "drop_drawn_card"]
    if room.game_state.phase == PHASE_FINAL_TURNS:
        return ["draw_from_deck", "take_discard_and_swap"]
    return ["draw_from_deck", "take_discard_and_swap", "call_screw"]


def _draw_from_deck(room: Room, player: Player) -> ActionResult:
    if player.pending_drawn_card:
        raise ConflictError("player already has a pending drawn card")
    if not room.game_state.draw_pile:
        raise ConflictError("draw pile is empty")

    _disable_initial_reveal(room)
    drawn_card = room.game_state.draw_pile.pop()
    player.pending_drawn_card = _card_with_movement(
        drawn_card,
        _location("pile"),
        _location("pending_drawn_card", player.player_id),
    )
    return ActionResult(visible_state(room, player.player_id))


def _swap_drawn_with_own_card(room: Room, player: Player, payload: dict) -> ActionResult:
    if not player.pending_drawn_card:
        raise ConflictError("player has no pending drawn card")

    _disable_initial_reveal(room)
    own_card_index = _own_card_index(player, payload)
    old_card = player.cards[own_card_index]
    player.cards[own_card_index] = _card_with_movement(
        player.pending_drawn_card,
        _location("pile"),
        _location("hand", player.player_id, own_card_index),
    )
    player.pending_drawn_card = None
    room.game_state.discard_pile.append(
        _card_with_movement(
            old_card,
            _location("hand", player.player_id, own_card_index),
            _location("discard"),
        )
    )
    complete_turn(room)
    return ActionResult(visible_state(room, player.player_id))


def _drop_drawn_card(room: Room, player: Player) -> ActionResult:
    if not player.pending_drawn_card:
        raise ConflictError("player has no pending drawn card")

    _disable_initial_reveal(room)
    dropped_card = player.pending_drawn_card
    player.pending_drawn_card = None
    room.game_state.discard_pile.append(
        _card_with_movement(
            dropped_card,
            _location("pile"),
            _location("discard"),
        )
    )

    pending_action_type = _pending_action_type_for_dropped_card(dropped_card)
    if not pending_action_type:
        complete_turn(room)
        return ActionResult(visible_state(room, player.player_id))

    room.game_state.phase = PHASE_WAITING_FOR_ACTION_TARGET
    room.game_state.pending_action = {
        "type": pending_action_type,
        "card_id": dropped_card.id,
    }
    if dropped_card.type == CardType.NUMBER:
        room.game_state.pending_action["card_value"] = dropped_card.value
        room.game_state.pending_action["reveal_duration_seconds"] = NUMERIC_PEEK_DURATION_SECONDS
    return ActionResult(visible_state(room, player.player_id), message=_action_started_message(dropped_card))


def _take_discard_and_swap(room: Room, player: Player, payload: dict) -> ActionResult:
    if player.pending_drawn_card:
        raise ConflictError("cannot take from discard after drawing from deck")
    if not room.game_state.discard_pile:
        raise ConflictError("discard pile is empty")

    _disable_initial_reveal(room)
    own_card_index = _own_card_index(player, payload)
    taken_card = room.game_state.discard_pile.pop()
    old_card = player.cards[own_card_index]
    player.cards[own_card_index] = _card_with_movement(
        taken_card,
        _location("discard"),
        _location("hand", player.player_id, own_card_index),
    )
    room.game_state.discard_pile.append(
        _card_with_movement(
            old_card,
            _location("hand", player.player_id, own_card_index),
            _location("discard"),
        )
    )
    complete_turn(room)
    return ActionResult(visible_state(room, player.player_id))


def _apply_pending_action(room: Room, player: Player, action: str, payload: dict) -> ActionResult:
    pending_action = room.game_state.pending_action
    if not pending_action:
        raise ConflictError("no special card action is pending")

    action_type = pending_action.get("type")
    if action_type == CardType.GIVE_AND_TAKE.value:
        if action != "resolve_give_and_take":
            raise BadRequestError("resolve_give_and_take is required")
        return _resolve_give_and_take(room, player, payload)

    if action_type == CardType.SEE_AND_SWAB.value:
        if action == "reveal_see_and_swap_target":
            return _reveal_see_and_swap_target(room, player, payload)
        if action == "resolve_see_and_swap":
            return _resolve_see_and_swap(room, player, payload)
        if action == "cancel_see_and_swap":
            return _cancel_pending_action(room, player, "See and Swap was cancelled")
        raise BadRequestError("unsupported See and Swap action")

    if action_type == ACTION_PEEK_OWN_CARD:
        if action != "reveal_own_card":
            raise BadRequestError("reveal_own_card is required")
        return _reveal_own_card(room, player, payload)

    if action_type == ACTION_PEEK_OPPONENT_CARD:
        if action != "reveal_opponent_card":
            raise BadRequestError("reveal_opponent_card is required")
        return _reveal_opponent_card(room, player, payload)

    raise BadRequestError("unsupported pending action")


def _resolve_give_and_take(room: Room, player: Player, payload: dict) -> ActionResult:
    _disable_initial_reveal(room)
    own_card_index = _own_card_index(player, payload)
    opponent = _target_player(room, player, payload)
    opponent_card_index = _target_card_index(opponent, payload)

    # Swap cards and track the exact hand indexes involved.
    card_from_player = player.cards[own_card_index]
    card_from_opponent = opponent.cards[opponent_card_index]

    player.cards[own_card_index] = _card_with_movement(
        card_from_opponent,
        _location("hand", opponent.player_id, opponent_card_index),
        _location("hand", player.player_id, own_card_index),
        {
            "action": "give_and_take",
            "from_player_id": opponent.player_id,
            "from_index": opponent_card_index,
            "to_player_id": player.player_id,
            "to_index": own_card_index,
        },
    )
    opponent.cards[opponent_card_index] = _card_with_movement(
        card_from_player,
        _location("hand", player.player_id, own_card_index),
        _location("hand", opponent.player_id, opponent_card_index),
        {
            "action": "give_and_take",
            "from_player_id": player.player_id,
            "from_index": own_card_index,
            "to_player_id": opponent.player_id,
            "to_index": opponent_card_index,
        },
    )
    _finish_pending_action_and_end_turn(room)
    return ActionResult(visible_state(room, player.player_id), message="Give and Take resolved")


def _reveal_see_and_swap_target(room: Room, player: Player, payload: dict) -> ActionResult:
    pending_action = room.game_state.pending_action
    if pending_action and pending_action.get("revealed_card"):
        raise ConflictError("See and Swap target has already been revealed")

    _disable_initial_reveal(room)
    target = _target_player(room, player, payload, allow_self=True)
    target_card_index = _target_card_index(target, payload)
    room.game_state.pending_action = {
        **(pending_action or {}),
        "revealed_card": {
            "card_owner_id": target.player_id,
            "card_index": target_card_index,
        },
    }
    return ActionResult(visible_state(room, player.player_id), message="See and Swap target revealed")


def _resolve_see_and_swap(room: Room, player: Player, payload: dict) -> ActionResult:
    pending_action = room.game_state.pending_action or {}
    revealed_card = pending_action.get("revealed_card")
    if not revealed_card:
        raise ConflictError("reveal a target card before resolving See and Swap")

    _disable_initial_reveal(room)
    own_card_index = _own_card_index(player, payload)
    target = _require_player(room, revealed_card["card_owner_id"])
    target_card_index = revealed_card["card_index"]
    if target.player_id == player.player_id and target_card_index == own_card_index:
        raise BadRequestError("cannot swap a card with itself")

    # Swap cards and track the exact hand indexes involved.
    card_from_player = player.cards[own_card_index]
    card_from_target = target.cards[target_card_index]

    player.cards[own_card_index] = _card_with_movement(
        card_from_target,
        _location("hand", target.player_id, target_card_index),
        _location("hand", player.player_id, own_card_index),
        {
            "action": "see_and_swap",
            "from_player_id": target.player_id,
            "from_index": target_card_index,
            "to_player_id": player.player_id,
            "to_index": own_card_index,
        },
    )
    target.cards[target_card_index] = _card_with_movement(
        card_from_player,
        _location("hand", player.player_id, own_card_index),
        _location("hand", target.player_id, target_card_index),
        {
            "action": "see_and_swap",
            "from_player_id": player.player_id,
            "from_index": own_card_index,
            "to_player_id": target.player_id,
            "to_index": target_card_index,
        },
    )
    _finish_pending_action_and_end_turn(room)
    return ActionResult(visible_state(room, player.player_id), message="See and Swap resolved")


def _cancel_pending_action(room: Room, player: Player, message: str) -> ActionResult:
    _disable_initial_reveal(room)
    _finish_pending_action_and_end_turn(room)
    return ActionResult(visible_state(room, player.player_id), message=message)


def _reveal_own_card(room: Room, player: Player, payload: dict) -> ActionResult:
    _disable_initial_reveal(room)
    own_card_index = _own_card_index(player, payload)
    revealed_card = player.cards[own_card_index]

    _finish_pending_action_and_end_turn(room)
    state = visible_state(room, player.player_id)
    state["action_reveal"] = _action_reveal_payload(
        player.player_id,
        own_card_index,
        revealed_card,
    )
    return ActionResult(state, message="Own card revealed")


def _reveal_opponent_card(room: Room, player: Player, payload: dict) -> ActionResult:
    _disable_initial_reveal(room)
    opponent = _target_player(room, player, payload)
    opponent_card_index = _target_card_index(opponent, payload)
    revealed_card = opponent.cards[opponent_card_index]

    _finish_pending_action_and_end_turn(room)
    state = visible_state(room, player.player_id)
    state["action_reveal"] = _action_reveal_payload(
        opponent.player_id,
        opponent_card_index,
        revealed_card,
    )
    return ActionResult(state, message="Opponent card revealed")


def call_screw(room: Room, player_id: str) -> ActionResult:
    if room.status != "playing":
        raise ConflictError("Cannot call Screw unless the game is playing")
    if room.game_state.screw_caller_id or room.game_state.phase in {PHASE_FINAL_TURNS, PHASE_ROUND_FINISHED}:
        raise ConflictError("Screw has already been called")

    player = _require_player(room, player_id)
    if room.game_state.current_player_id != player_id:
        raise ForbiddenError("Only the current player can call Screw")
    if player.pending_drawn_card:
        raise ConflictError("Cannot call Screw while holding a drawn card")
    if room.game_state.phase == PHASE_WAITING_FOR_ACTION_TARGET:
        raise ConflictError("Cannot call Screw while an action card is pending")

    _disable_initial_reveal(room)
    start_final_turns(room, player_id)
    return ActionResult(visible_state(room, player_id))


def start_final_turns(room: Room, caller_id: str) -> None:
    caller_index = _player_index(room, caller_id)
    final_turn_player_ids = [
        room.players[(caller_index + offset) % len(room.players)].player_id
        for offset in range(1, len(room.players))
    ]

    room.game_state.screw_caller_id = caller_id
    room.game_state.final_turn_player_ids = final_turn_player_ids
    room.game_state.phase = PHASE_FINAL_TURNS
    room.game_state.pending_action = None
    room.game_state.current_player_id = final_turn_player_ids[0] if final_turn_player_ids else None

    if not final_turn_player_ids:
        finish_round(room)


def complete_turn(room: Room) -> None:
    current_id = room.game_state.current_player_id
    current_index = _player_index(room, current_id)

    room.players[current_index].turns_completed += 1
    room.game_state.pending_action = None

    if room.game_state.screw_caller_id:
        _complete_final_turn(room, current_id)
        return

    next_index = (current_index + 1) % len(room.players)
    room.game_state.current_player_id = room.players[next_index].player_id
    room.game_state.phase = PHASE_PLAYING


def _finish_pending_action_and_end_turn(room: Room) -> None:
    room.game_state.pending_action = None
    room.game_state.phase = PHASE_FINAL_TURNS if room.game_state.screw_caller_id else PHASE_PLAYING
    complete_turn(room)


def _complete_final_turn(room: Room, player_id: str) -> None:
    if player_id not in room.game_state.final_turn_player_ids:
        raise ConflictError("current player is not in final turns")

    room.game_state.final_turn_player_ids = [
        final_turn_player_id
        for final_turn_player_id in room.game_state.final_turn_player_ids
        if final_turn_player_id != player_id
    ]
    if not room.game_state.final_turn_player_ids:
        finish_round(room)
        return

    room.game_state.current_player_id = room.game_state.final_turn_player_ids[0]
    room.game_state.phase = PHASE_FINAL_TURNS


def finish_round(room: Room) -> None:
    room.game_state.pending_action = None
    room.game_state.final_turn_player_ids = []
    room.game_state.current_player_id = None
    room.game_state.round_results = build_round_results(room)
    room.game_state.phase = PHASE_ROUND_FINISHED


def calculate_round_scores(room: Room) -> dict[str, int]:
    return {
        player.player_id: sum(_card_score(card) for card in player.cards)
        for player in room.players
    }


def build_round_results(room: Room) -> list[dict]:
    scores = calculate_round_scores(room)
    if not scores:
        return []

    lowest_score = min(scores.values())
    caller_id = room.game_state.screw_caller_id
    caller_score = scores.get(caller_id) if caller_id else None
    screw_success = caller_score is not None and caller_score == lowest_score
    room.game_state.screw_success = screw_success

    results = []
    for player in room.players:
        raw_round_score = scores[player.player_id]
        is_screw_caller = player.player_id == caller_id
        applied_round_score = raw_round_score
        if is_screw_caller and not screw_success:
            applied_round_score += SCREW_FAILED_PENALTY

        player.total_score += applied_round_score
        results.append(
            {
                "player_id": player.player_id,
                "nickname": player.nickname,
                "cards": [
                    {"index": index, "card": card.to_dict()}
                    for index, card in enumerate(player.cards)
                ],
                "raw_round_score": raw_round_score,
                "applied_round_score": applied_round_score,
                "total_score": player.total_score,
                "is_screw_caller": is_screw_caller,
                "had_lowest_score": raw_round_score == lowest_score,
            }
        )
    return results


def _disable_initial_reveal(room: Room) -> None:
    room.game_state.initial_reveal_active = False


def _player_index(room: Room, player_id: str | None) -> int:
    player_index = next(
        (index for index, player in enumerate(room.players) if player.player_id == player_id),
        None,
    )
    if player_index is None:
        raise ConflictError("current player is no longer in the room")
    return player_index


def _require_player(room: Room, player_id: str) -> Player:
    player = room.get_player(player_id)
    if not player:
        raise NotFoundError("player not found")
    return player


def _get_current_player(room: Room) -> Player | None:
    """Get the current player object, or None if not found."""
    if room.game_state.current_player_id:
        return room.get_player(room.game_state.current_player_id)
    return None


def _require_current_turn(room: Room, player_id: str) -> None:
    if room.game_state.current_player_id != player_id:
        raise ForbiddenError("it is not this player's turn")


def _require_current_player_can_play_phase(room: Room, player_id: str) -> None:
    if room.game_state.phase == PHASE_FINAL_TURNS and player_id not in room.game_state.final_turn_player_ids:
        raise ForbiddenError("it is not this player's final turn")


def _own_card_index(player: Player, payload: dict) -> int:
    if "own_card_index" not in payload:
        raise BadRequestError("own_card_index is required")
    own_card_index = payload["own_card_index"]
    if not isinstance(own_card_index, int):
        raise BadRequestError("own_card_index must be an integer")
    if own_card_index < 0 or own_card_index >= len(player.cards):
        raise BadRequestError("own_card_index is out of range")
    return own_card_index


def _target_player(room: Room, current_player: Player, payload: dict, allow_self: bool = False) -> Player:
    if "target_player_id" not in payload:
        raise BadRequestError("target_player_id is required")
    target_player_id = payload["target_player_id"]
    if not isinstance(target_player_id, str):
        raise BadRequestError("target_player_id must be a string")
    target = _require_player(room, target_player_id)
    if not allow_self and target.player_id == current_player.player_id:
        raise BadRequestError("target_player_id must be an opponent")
    return target


def _target_card_index(player: Player, payload: dict) -> int:
    if "target_card_index" not in payload:
        raise BadRequestError("target_card_index is required")
    target_card_index = payload["target_card_index"]
    if not isinstance(target_card_index, int):
        raise BadRequestError("target_card_index must be an integer")
    if target_card_index < 0 or target_card_index >= len(player.cards):
        raise BadRequestError("target_card_index is out of range")
    return target_card_index


def _visible_pending_action(room: Room, requester: Player) -> dict | None:
    pending_action = room.game_state.pending_action
    if not pending_action or room.game_state.current_player_id != requester.player_id:
        return None

    visible = {
        "type": pending_action.get("type"),
        "card_id": pending_action.get("card_id"),
    }
    if "card_value" in pending_action:
        visible["card_value"] = pending_action["card_value"]
    if "reveal_duration_seconds" in pending_action:
        visible["reveal_duration_seconds"] = pending_action["reveal_duration_seconds"]
    revealed_card = pending_action.get("revealed_card")
    if revealed_card:
        target = room.get_player(revealed_card["card_owner_id"])
        card_index = revealed_card["card_index"]
        visible["revealed_card"] = {
            "card_owner_id": revealed_card["card_owner_id"],
            "card_index": card_index,
            "card": target.cards[card_index].to_dict() if target else None,
        }
    return visible


def _revealed_card_for_requester(room: Room, requester: Player) -> dict | None:
    if room.game_state.current_player_id != requester.player_id:
        return None
    pending_action = room.game_state.pending_action or {}
    if pending_action.get("type") != CardType.SEE_AND_SWAB.value:
        return None
    return pending_action.get("revealed_card")


def _action_started_message(card: Card) -> str:
    if card.type == CardType.GIVE_AND_TAKE:
        return "Give and Take activated"
    if card.type == CardType.SEE_AND_SWAB:
        return "See and Swap activated"
    if card.type == CardType.NUMBER and card.value in {7, 8}:
        return "Peek own card activated"
    if card.type == CardType.NUMBER and card.value in {9, 10}:
        return "Peek opponent card activated"
    return "Action activated"


def _pending_action_type_for_dropped_card(card: Card) -> str | None:
    if is_action_card(card):
        return card.type.value
    if card.type != CardType.NUMBER:
        return None
    if card.value in {7, 8}:
        return ACTION_PEEK_OWN_CARD
    if card.value in {9, 10}:
        return ACTION_PEEK_OPPONENT_CARD
    return None


def _action_reveal_payload(card_owner_id: str, card_index: int, card: Card) -> dict:
    return {
        "card_owner_id": card_owner_id,
        "card_index": card_index,
        "card": card.to_dict(),
        "reveal_duration_seconds": NUMERIC_PEEK_DURATION_SECONDS,
    }


def _card_score(card: Card) -> int:
    if card.type == CardType.NUMBER:
        if card.value is None:
            raise ConflictError("number card is missing a value")
        return card.value

    card_type = card.type.value if isinstance(card.type, CardType) else str(card.type)
    if is_action_card(card):
        return SPECIAL_CARD_SCORE_VALUES.get(card_type, ACTION_CARD_SCORE_VALUE)
    if card_type not in SPECIAL_CARD_SCORE_VALUES:
        raise ConflictError(f"unsupported card type for scoring: {card_type}")
    return SPECIAL_CARD_SCORE_VALUES[card_type]


def _lowest_score_from_results(results: list[dict]) -> int | None:
    if not results:
        return None
    return min(result["raw_round_score"] for result in results)
