from __future__ import annotations

from flask import Blueprint, render_template

from storage.memory_store import store

debug_bp = Blueprint("debug_routes", __name__, url_prefix="/debug")


@debug_bp.get("/rooms")
def debug_rooms_route():
    rooms = sorted(store.all_rooms(), key=lambda room: room.created_at, reverse=True)
    return render_template("debug_rooms.html", rooms=rooms)


@debug_bp.get("/rooms/<room_id>")
def debug_room_route(room_id: str):
    room = store.get_room(room_id)
    if not room:
        return render_template("debug_room_not_found.html", room_id=room_id), 404

    return render_template("debug_room.html", room=room)
