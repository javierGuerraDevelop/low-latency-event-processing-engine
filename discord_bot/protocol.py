"""Pure helpers for the engine delivery protocol.

The C++ engine sends newline-framed JSON over TCP. These helpers parse frames,
decide whether a shotcall is still fresh enough to speak, and route messages.
"""

from __future__ import annotations

import json
import time
from typing import Any, Final

DEFAULT_GRACE_MS: Final = 2000

# Mirror of Constants::identifying_spells, grouped by class.
IDENTIFY_SPELLS: Final[dict[str, tuple[str, ...]]] = {
    "Death Knight": ("Death's Advance (48743)", "Anti-Magic Shell (48707)"),
    "Demon Hunter": ("Immolation Aura (258920)", "Vengeful Retreat (198793)"),
    "Druid": ("Mark of the Wild (1126)", "Regrowth (8936)"),
    "Evoker": ("Blessing of the Bronze (364342)", "Living Flame (361469)"),
    "Hunter": ("Feign Death (5384)",),
    "Mage": ("Arcane Intellect (1459)", "Blink (1953)", "Shimmer (212653)"),
    "Monk": ("Vivify (116670)", "Roll (109132)", "Chi Torpedo (115008)"),
    "Paladin": ("Flash of Light (19750)", "Divine Steed (190784)"),
    "Priest": ("Power Word: Fortitude (21562)", "Flash Heal (2061)"),
    "Rogue": ("Stealth (1784)", "Shadowstep (36554)"),
    "Shaman": ("Healing Surge (8004)", "Skyfury (462854)"),
    "Warlock": ("Demonic Gateway (111771)", "Create Soulwell (29893)"),
    "Warrior": ("Battle Shout (6673)", "Heroic Leap (6544)"),
}

_VALID_TYPES: Final = frozenset({"shotcall", "party_status"})


def now_ms() -> int:
    """Returns the current wall-clock time in milliseconds since the epoch."""
    return time.time_ns() // 1_000_000


def parse_message(line: str) -> dict[str, Any] | None:
    """Parses one frame; returns None for malformed or unknown messages."""
    if not line.strip():
        return None
    try:
        message = json.loads(line)
    except json.JSONDecodeError:
        return None
    if not isinstance(message, dict):
        return None
    if message.get("type") not in _VALID_TYPES:
        return None
    if not isinstance(message.get("text"), str):
        return None
    return message


def is_stale(message: dict[str, Any], now_ms: int, grace_ms: int = DEFAULT_GRACE_MS) -> bool:
    """True when a shotcall's predicted cast is past now plus the grace window."""
    if message.get("type") != "shotcall":
        return False
    due_ms = message.get("due_ms")
    if isinstance(due_ms, bool) or not isinstance(due_ms, (int, float)):
        return True
    return now_ms > due_ms + grace_ms


def route(message: dict[str, Any]) -> str:
    """Returns "channel" for status updates and "tts" for everything else."""
    return "channel" if message.get("type") == "party_status" else "tts"


def select_queueable(
    messages: list[dict[str, Any]], now_ms: int, grace_ms: int = DEFAULT_GRACE_MS
) -> list[dict[str, Any]]:
    """Keeps status updates and shotcalls that are still inside the grace window."""
    return [message for message in messages if not is_stale(message, now_ms, grace_ms)]
