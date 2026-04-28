from __future__ import annotations

from flask import Blueprint, jsonify, request

from game.engine import GameError, create_room, join_room, start_game
from storage.memory_store import store

room_bp = Blueprint("room_routes", __name__)


@room_bp.errorhandler(GameError)
def handle_game_error(error: GameError):
    return jsonify({"error": error.message}), error.status_code


@room_bp.post("/rooms")
def create_room_route():
    data = request.get_json(silent=True) or {}
    max_players = data.get("max_players", 4)
    if not isinstance(max_players, int):
        return jsonify({"error": "max_players must be an integer"}), 400

    room = create_room(
        is_public=data.get("is_public", False),
        max_players=max_players,
    )
    store.add_room(room)
    return jsonify({
        "room_id": room.room_id,
        "room_code": room.room_code,
        "is_public": room.is_public,
        "status": room.status,
    }), 201


def _public_room_dict(room):
    return {
        "room_id": room.room_id,
        "room_code": room.room_code,
        "status": room.status,
        "players_count": len(room.players),
        "max_players": room.max_players,
        "created_at": room.created_at,
        "join_url": f"/rooms/{room.room_id}/join",
        "join_by_code_url": f"/rooms/{room.room_code}/join",
    }


@room_bp.get("/rooms")
def list_rooms_route():
    rooms = [_public_room_dict(room) for room in store.public_waiting_rooms()]
    return jsonify({"rooms": rooms})


@room_bp.get("/rooms/public")
def list_public_rooms_route():
    rooms = [_public_room_dict(room) for room in store.public_waiting_rooms()]
    return jsonify({"rooms": rooms})


@room_bp.post("/rooms/<room_id>/join")
def join_room_route(room_id: str):
    room = store.get_room(room_id)
    if not room:
        return jsonify({"error": "room not found"}), 404

    data = request.get_json(silent=True) or {}
    player = join_room(room, data.get("nickname", ""))
    return jsonify({
        "room_id": room.room_id,
        "player_id": player.player_id,
        "nickname": player.nickname,
    }), 201


@room_bp.get("/rooms/<room_id>/lobby")
def lobby_route(room_id: str):
    room = store.get_room(room_id)
    if not room:
        return jsonify({"error": "room not found"}), 404
    return jsonify(room.lobby_dict())


@room_bp.post("/rooms/<room_id>/start")
def start_room_route(room_id: str):
    room = store.get_room(room_id)
    if not room:
        return jsonify({"error": "room not found"}), 404

    data = request.get_json(silent=True) or {}
    player_id = data.get("player_id")
    if not player_id:
        return jsonify({"error": "player_id is required"}), 400

    return jsonify(start_game(room, player_id))
