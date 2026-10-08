# Shot-Caller-WoW

Real-time WoW Mythic+ shotcaller. Parses the combat log, tracks enemy ability timers, and assigns interrupts/CC to available party members. Callouts are spoken in Discord voice chat via a TTS bot.

## Architecture

```
WoW Combat Log → C++ Engine → TCP Socket → Discord Bot → ElevenLabs TTS → Voice Channel
```

- **C++ engine** — Tail-follows the combat log, identifies players/enemies from spells, schedules each enemy ability's next occurrence lazily, and dispatches callouts from an event-driven loop assigning the best available interrupter or CCer.
- **Discord bot** — Python bot that receives callouts over TCP (port 9999) and plays them as TTS audio using ElevenLabs.

## Run boundaries and roster

The engine reacts to the run boundaries present in `WoWCombatLog.txt`:

- `CHALLENGE_MODE_START` / `CHALLENGE_MODE_END` — start and end a Mythic+ run. Starting a run clears tracked enemies and queued calls and begins a fresh roster snapshot; known player classes persist across runs.
- `ENCOUNTER_START` / `ENCOUNTER_END` — start and end a boss encounter. A group size other than 5 pauses shotcalls until a valid encounter starts.
- `ZONE_CHANGE` — leaving the zone while an encounter is active ends the run.
- `COMBAT_LOG_VERSION` — records whether advanced combat logging is enabled.

Enemy timelines only start from hostile actions (`SPELL_CAST_START`, `SPELL_CAST_SUCCESS`, damage and swing events) during an active run; auras and idle casts do not engage.

The log has no party-join, ready-check, or pull-timer events. Party membership comes from `COMBATANT_INFO` records, which are written one per player at each boundary when advanced combat logging is enabled. The engine collects them into a current-run roster for 500 ms (or until the next non-`COMBATANT_INFO` event) and reports the result through `PartyStatus`; without `COMBATANT_INFO` it falls back to action-based identification and `ENCOUNTER_START` group size.

While a run is active and fewer than the expected players are identified, the engine emits a party status message every 20–30 seconds asking the group to use an identifying ability.

## Protocol

The engine sends one JSON object per line over TCP (default port 9999):

```json
{"v":1,"type":"shotcall","text":"Lilrawb kick DoT soon","enemy":"Creature-...","spell":463218,"mechanic":"kick","due_ms":1750000000000,"call_id":42}
{"v":1,"type":"party_status","text":"3/5 players identified. Use your class ability to identify yourself."}
```

`due_ms` is the predicted cast time in Unix milliseconds. The bot drops shotcalls that arrive more than `SHOTCALL_STALE_GRACE_MS` (default 2000) after `due_ms` instead of speaking them late; status messages have no deadline.

## Build

```bash
cmake -B build && cmake --build build
```

## Run

```bash
# C++ engine (pass your WoW logs directory)
./build/ShotCallerWow /path/to/WoW/Logs

# Discord bot (requires ELEVENLABS_API_KEY and DISCORD_BOT_TOKEN)
python3 -m venv .venv && . .venv/bin/activate
pip install -r discord_bot/requirements.txt
python discord_bot/main.py
```

FFmpeg must be installed and on `PATH` for voice playback.

### Bot environment variables

| Variable                    | Default     | Purpose                                                        |
| --------------------------- | ----------- | -------------------------------------------------------------- |
| `DISCORD_BOT_TOKEN`         | required    | Discord bot token                                              |
| `ELEVENLABS_API_KEY`        | required    | ElevenLabs API key                                             |
| `SHOTCALL_HOST`             | `127.0.0.1` | Engine host                                                    |
| `SHOTCALL_PORT`             | `9999`      | Engine port                                                    |
| `SHOTCALL_STALE_GRACE_MS`   | `2000`      | Drop shotcalls later than this after their predicted cast      |
| `SHOTCALL_STATUS_CHANNEL_ID` | unset      | Channel for party status text; falls back to the `!join` channel |
| `SHOTCALL_STATUS_TTS`       | unset       | Set to `1` to also speak party status updates                  |

## Tests

```bash
cmake --build build && cd build && ctest --output-on-failure

pip install -r discord_bot/requirements-dev.txt
python -m pytest discord_bot/tests
```

## Discord Bot Commands

| Command         | Description                                   |
| --------------- | --------------------------------------------- |
| `!join`         | Join your voice channel                       |
| `!leave`        | Leave voice channel                           |
| `!say <text>`   | Speak text via TTS                            |
| `!voice [name]` | Change TTS voice or list available voices     |
| `!identify`     | List class-identifying spells for the party   |
