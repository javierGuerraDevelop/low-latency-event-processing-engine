"""Puts discord_bot/ on sys.path so tests can import protocol."""

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
