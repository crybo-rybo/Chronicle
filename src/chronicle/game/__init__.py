"""Game backends."""

from chronicle.game.cartridge_game import CartridgeGame
from chronicle.game.protocol import GameBackend
from chronicle.game.tiny import TinyRoomGame

__all__ = ["CartridgeGame", "GameBackend", "TinyRoomGame"]
