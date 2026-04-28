from __future__ import annotations

from game.room import Room


class MemoryStore:
    def __init__(self) -> None:
        self.rooms: dict[str, Room] = {}

    def add_room(self, room: Room) -> None:
        self.rooms[room.room_id] = room

    def get_room(self, room_id: str) -> Room | None:
        room = self.rooms.get(room_id)
        if room:
            return room

        room_code = room_id.upper()
        return next((room for room in self.rooms.values() if room.room_code == room_code), None)

    def all_rooms(self) -> list[Room]:
        return list(self.rooms.values())

    def public_waiting_rooms(self) -> list[Room]:
        return [
            room for room in self.rooms.values()
            if room.is_public and room.status == "waiting"
        ]


store = MemoryStore()
