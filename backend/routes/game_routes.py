from __future__ import annotations

from flask import Blueprint, jsonify, request

from game.engine import GameError, apply_action, visible_state
from storage.memory_store import store

game_bp = Blueprint("game_routes", __name__)


@game_bp.errorhandler(GameError)
def handle_game_error(error: GameError):
    return jsonify({"error": error.message}), error.status_code


@game_bp.get("/rooms/<room_id>/state")
def get_state_route(room_id: str):
    room = store.get_room(room_id)
    if not room:
        return jsonify({"error": "room not found"}), 404

    player_id = request.args.get("player_id")
    if not player_id:
        return jsonify({"error": "player_id is required"}), 400

    return jsonify(visible_state(room, player_id))


@game_bp.post("/rooms/<room_id>/actions")
def action_route(room_id: str):
    room = store.get_room(room_id)
    if not room:
        return jsonify({"error": "room not found"}), 404

    data = request.get_json(silent=True) or {}
    player_id = data.get("player_id")
    action = data.get("action")
    payload = data.get("payload", {})

    if not player_id:
        return jsonify({"error": "player_id is required"}), 400
    if not action:
        return jsonify({"error": "action is required"}), 400
    if not isinstance(payload, dict):
        return jsonify({"error": "payload must be an object"}), 400

    result = apply_action(room, player_id, action, payload)
    return jsonify(result.to_dict())
