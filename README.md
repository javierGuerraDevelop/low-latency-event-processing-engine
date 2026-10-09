# Shot-Caller-WoW

Real-time WoW Mythic+ shotcaller. It parses the combat log, predicts enemy casts, assigns an
available interrupter or crowd controller to each dangerous ability, and speaks the callout in
Discord voice chat through ElevenLabs TTS.

## Architecture

```
WoWCombatLog.txt → C++ engine → newline-framed JSON over TCP 9999 → Discord bot → ElevenLabs → voice
```

- **C++ engine** — tail-follows the newest combat log, reacts to run boundaries, identifies players
  and enemies, schedules each enemy ability's next occurrence, resyncs from real casts, and
  dispatches callouts with the best available assignee.
- **Discord bot** — reads framed JSON, drops stale shotcalls, generates TTS in a worker thread, and
  plays audio in the voice channel. Party status updates are posted as channel text.

## Prerequisites

- CMake 3.20 or newer
- A C++20 compiler (GCC or Clang)
- Python 3.10 or newer for the bot
- FFmpeg on `PATH` for voice playback

## Build and test

```bash
cmake -B build -S . -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build -j"$(nproc)"
ctest --test-dir build --output-on-failure

# Sanitizers, warnings as errors
cmake -B build-san -S . -DSHOTCALLER_ENABLE_SANITIZERS=ON -DSHOTCALLER_WARNINGS_AS_ERRORS=ON
cmake --build build-san -j"$(nproc)"
ctest --test-dir build-san --output-on-failure

# Python bot
python3 -m venv .venv && . .venv/bin/activate
pip install -r discord_bot/requirements-dev.txt
python -m pytest discord_bot/tests
```

## Run

```bash
# C++ engine (pass your WoW logs directory)
./build/ShotCallerWow [--replay] [--strict-party-size] /path/to/WoW/Logs

# Discord bot (requires ELEVENLABS_API_KEY and DISCORD_BOT_TOKEN)
pip install -r discord_bot/requirements.txt
python discord_bot/main.py
```

Engine options:

| Option                | Purpose                                                                                 |
| --------------------- | --------------------------------------------------------------------------------------- |
| `--replay`            | Read the initial log from the beginning before tailing, warming the roster immediately  |
| `--strict-party-size` | Exit with code `2` when a boundary or snapshot reports fewer than 5 players             |

The engine exits cleanly on `SIGINT`/`SIGTERM`, follows log rotation to the newest `WoWCombatLog*`
file within about two seconds, and resolves the log before starting any worker thread so a missing
file exits `1` without hanging.

## Timing model

Each tracked ability gets a predicted cast time when its enemy is first seen. Real
`SPELL_CAST_START`, `SPELL_CAST_SUCCESS`, and `SPELL_INTERRUPT` records resynchronize that
prediction to the actual cast plus one cooldown. Calls are announced 2.5 seconds before the
predicted cast (`call_lead`), may still fire up to one second late (`late_grace`), and are dropped
instead of spoken once that grace expires. The bot applies the same deadline one hop later:
shotcalls that arrive more than `SHOTCALL_STALE_GRACE_MS` (default 2000 ms) after `due_ms` are
never played.

## Boundaries and validation

The engine reacts to the run boundaries produced by the game:

- `CHALLENGE_MODE_START` / `CHALLENGE_MODE_END` — start and end a Mythic+ run. Starting a run
  clears tracked enemies and queued calls and begins a fresh roster snapshot; known classes persist
  across runs, and an end before any start is ignored.
- `ENCOUNTER_START` / `ENCOUNTER_END` — start and end a boss encounter. A group size other than 5
  pauses callouts until a valid encounter starts; `--strict-party-size` turns a size below 5 into
  exit code `2`.
- `ZONE_CHANGE` — leaving the zone while an encounter is active ends the run.
- `COMBAT_LOG_VERSION` — records whether advanced combat logging is enabled.

Enemy timelines only start from hostile actions (`SPELL_CAST_START`, `SPELL_CAST_SUCCESS`, damage
and swing events) while a challenge or encounter is active, so pre-pull auras and idle casts do not
engage.

The log has no party-join, ready-check, or pull-timer events. Party membership comes from
`COMBATANT_INFO` records, written one per player at each boundary when advanced combat logging is
enabled. The engine collects them into a current-run roster for 500 ms (or until the next
non-`COMBATANT_INFO` event) and reports the result through `PartyStatus`; without
`COMBATANT_INFO` it falls back to action-based identification and the `ENCOUNTER_START` group size.

## Player identification

Players are identified in this order:

1. `COMBATANT_INFO` — GUID, class, and spec without any action (requires advanced combat logging).
2. Any party-flagged event — learns the name and refreshes `last_seen`.
3. Class-identifying spell from `Constants::identifying_spells`.
4. Interrupt or crowd-control spell from the class ability tables.

A player known only from `COMBATANT_INFO` has no name yet; they become assignable once any event
mentions their GUID. The `!identify` bot command lists the class-identifying spells.

## Party status notifications

While a run is active and fewer players are identified than expected, the engine emits a
`party_status` message at most once per interval (clamped to 20–30 seconds, default 25). The bot
posts the text in `SHOTCALL_STATUS_CHANNEL_ID`, or in the channel used by `!join`, and prints it to
the console when neither is set. Status messages have no deadline and are never spoken unless
`SHOTCALL_STATUS_TTS=1`.

## Protocol

The engine sends one JSON object per line over TCP (default port 9999):

```json
{"v":1,"type":"shotcall","text":"Lilrawb kick DoT soon","enemy":"Creature-...","spell":463218,"mechanic":"kick","due_ms":1750000000000,"call_id":42}
{"v":1,"type":"party_status","text":"3/5 players identified. Use your class ability to identify yourself."}
```

`due_ms` is the predicted cast time in Unix milliseconds; `mechanic` is one of `kick`, `stop`,
`dispel`, `tank`, `movement`, or `awareness`. The bot drops shotcalls that arrive past their
deadline and routes status messages to the channel.

## Bot environment variables

| Variable                     | Default     | Purpose                                                          |
| ---------------------------- | ----------- | ---------------------------------------------------------------- |
| `DISCORD_BOT_TOKEN`          | required    | Discord bot token                                                 |
| `ELEVENLABS_API_KEY`         | required    | ElevenLabs API key                                                |
| `SHOTCALL_HOST`              | `127.0.0.1` | Engine host                                                       |
| `SHOTCALL_PORT`              | `9999`      | Engine port                                                       |
| `SHOTCALL_STALE_GRACE_MS`    | `2000`      | Drop shotcalls later than this after their predicted cast          |
| `SHOTCALL_STATUS_CHANNEL_ID` | unset       | Channel for party status text; falls back to the `!join` channel   |
| `SHOTCALL_STATUS_TTS`        | unset       | Set to `1` to also speak party status updates                      |

## Discord bot commands

| Command         | Description                                   |
| --------------- | --------------------------------------------- |
| `!join`         | Join your voice channel                       |
| `!leave`        | Leave voice channel                           |
| `!say <text>`   | Speak text via TTS                            |
| `!voice [name]` | Change TTS voice or list available voices     |
| `!identify`     | List class-identifying spells for the party   |

## How to update enemy data

`include/constants.h` holds all static game data. Enemy abilities live in the `enemy_data` table:
one `EnemySpellProfile` per `(enemy_id, spell_id)` pair with first cast, cooldown, callout, and a
`Mechanic`. Only `Kick` assigns an interrupter and only `Stun` assigns a crowd controller;
`Dispel`, `TankHit`, `Movement`, and `Awareness` announce text without an assignee. After editing
the table, run the validation tests, which check unique pairs, positive cooldowns, non-negative
first casts, non-empty callouts, and coverage of every mechanic:

```bash
ctest --test-dir build -R Constants.EnemyData --output-on-failure
```

## Troubleshooting

- **Bot unreachable / no voice** — the engine logs one rate-limited warning and drops messages.
  Check that the bot is running, `SHOTCALL_PORT` matches, the bot has joined a voice channel
  (`!join`), and FFmpeg is installed.
- **No log found** — pass the directory containing `WoWCombatLog*.txt`; the engine exits `1` when
  none exists. The retail path is typically `<game>/_retail_/Logs`.
- **No callouts** — confirm the run is active (`CHALLENGE_MODE_START` or `ENCOUNTER_START` seen),
  the NPC is in `enemy_data`, and advanced combat logging is enabled for identification.
- **Everything is dropped as stale** — announcements expire shortly after their predicted cast.
  Keep the game machine and the bot host clocks in sync; a large skew drops every call.

## Known limitations

- No party-join/leave, ready-check, or pull-timer events exist in the combat log; the roster is
  refreshed from `COMBATANT_INFO`, and a mid-key change is only visible at the next snapshot.
- Spec IDs are known, but talents are not parsed, so ability availability stays class-level.
- Mob crowd-control immunity is not modeled; tracked mobs are assumed controllable.
- Dispel, tank-hit, movement, and awareness calls are announcements only, without assignees.
- Encounter phases are not modeled; resync corrects drift but not scripted phase changes.
