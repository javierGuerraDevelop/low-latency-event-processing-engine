"""Discord bot that receives engine messages over TCP and plays shotcalls as
TTS audio in a voice channel using ElevenLabs.

Required environment variables:
    ELEVENLABS_API_KEY
    DISCORD_BOT_TOKEN

Optional environment variables:
    SHOTCALL_HOST (default 127.0.0.1)
    SHOTCALL_PORT (default 9999)
    SHOTCALL_STALE_GRACE_MS (default 2000)
    SHOTCALL_STATUS_CHANNEL_ID
    SHOTCALL_STATUS_TTS (set to 1 to also speak status updates)

FFmpeg must be installed and on PATH for voice playback.
"""

from __future__ import annotations

import asyncio
import os
import socket
import sys
import tempfile
import threading

import discord
from discord.ext import commands
from elevenlabs import VoiceSettings
from elevenlabs.client import ElevenLabs

import protocol


def require_env(name: str) -> str:
    """Returns an environment variable or exits with an actionable message."""
    value = os.environ.get(name, "").strip()
    if not value:
        print(f"Error: {name} is not set. Export it before starting the bot.", file=sys.stderr)
        sys.exit(1)
    return value


DISCORD_BOT_TOKEN = require_env("DISCORD_BOT_TOKEN")
ELEVENLABS_API_KEY = require_env("ELEVENLABS_API_KEY")
HOST = os.environ.get("SHOTCALL_HOST", "127.0.0.1")
PORT = int(os.environ.get("SHOTCALL_PORT", "9999"))
STALE_GRACE_MS = int(os.environ.get("SHOTCALL_STALE_GRACE_MS", str(protocol.DEFAULT_GRACE_MS)))
STATUS_CHANNEL_ID = os.environ.get("SHOTCALL_STATUS_CHANNEL_ID", "").strip()
STATUS_TTS = os.environ.get("SHOTCALL_STATUS_TTS", "0") == "1"

intents = discord.Intents.default()
intents.message_content = True
intents.voice_states = True
bot = commands.Bot(command_prefix="!", intents=intents)
client = ElevenLabs(api_key=ELEVENLABS_API_KEY)

# Popular voice IDs
VOICES = {
    "rachel": "EXAVITQu4vr4xnSDxMaL",
    "domi": "AZnzlk1XvdvUeBnXmlld",
    "bella": "MF3mGyEYCl7XYWbV9V6O",
    "antoni": "ErXwobaYiN019PkySvjV",
    "adam": "21m00Tcm4TlvDq8ikWAM",
    "sam": "yoZ06aMxZJJ28mfd3POQ",
    "josh": "TxGEqnHWrfWFTfGW9XjX",
    "arnold": "VR6AewLTigWG4xSOukaG",
    "callum": "N2lVS1w4EtoT3dr4eOWO",
    "charlie": "IKne3meq5aSn9XLyUdCD",
}

current_voice = "rachel"
tts_queue: asyncio.Queue = asyncio.Queue()
last_joined_channel_id: int | None = None
started = False


def generate_audio(text: str) -> bytes:
    """Generates MP3 audio for text; runs in a worker thread."""
    audio = client.generate(
        text=text,
        voice=VOICES[current_voice],
        model="eleven_turbo_v2",  # Faster model for real-time
        voice_settings=VoiceSettings(
            stability=0.5, similarity_boost=0.75, style=0.0, use_speaker_boost=True
        ),
    )
    return b"".join(audio)


def active_voice_client() -> discord.VoiceClient | None:
    """Returns the first connected voice client, if any."""
    for voice_client in bot.voice_clients:
        if voice_client.is_connected():
            return voice_client
    return None


async def play_tts(text: str) -> None:
    """Generates and plays one line, waiting for playback to finish."""
    voice_client = active_voice_client()
    if voice_client is None:
        print("Warning: bot is not connected to a voice channel; dropping callout")
        return

    audio_data = await asyncio.to_thread(generate_audio, text)

    with tempfile.NamedTemporaryFile(suffix=".mp3", delete=False) as temp_file:
        temp_file.write(audio_data)
        audio_path = temp_file.name

    try:
        finished = asyncio.Event()

        def after_playback(error: Exception | None) -> None:
            bot.loop.call_soon_threadsafe(finished.set)

        voice_client.play(discord.FFmpegPCMAudio(audio_path), after=after_playback)
        await finished.wait()
    finally:
        try:
            os.remove(audio_path)
        except OSError:
            pass


async def playback_consumer() -> None:
    """Plays queued shotcalls one at a time."""
    while True:
        message = await tts_queue.get()
        try:
            await play_tts(message["text"])
        finally:
            tts_queue.task_done()


def resolve_status_channel() -> discord.abc.Messageable | None:
    """Returns the configured status channel, or the last joined text channel."""
    if STATUS_CHANNEL_ID:
        try:
            channel = bot.get_channel(int(STATUS_CHANNEL_ID))
        except ValueError:
            channel = None
        if channel is not None:
            return channel
    if last_joined_channel_id is not None:
        return bot.get_channel(last_joined_channel_id)
    return None


async def publish_status(text: str) -> None:
    """Sends a status update to the channel, or prints it when unset."""
    channel = resolve_status_channel()
    if channel is None:
        print(f"STATUS {text}")
        return
    await channel.send(text)


def handle_incoming(message: dict) -> None:
    """Queues one fresh message; runs on the bot event loop."""
    if protocol.route(message) == "tts":
        tts_queue.put_nowait(message)
        return
    asyncio.create_task(publish_status(message["text"]))
    if STATUS_TTS:
        tts_queue.put_nowait(message)


def serve_socket(loop: asyncio.AbstractEventLoop) -> None:
    """Accepts engine connections and forwards framed messages to the loop."""
    server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    server.bind((HOST, PORT))
    server.listen(5)
    print(f"Listening for engine messages on {HOST}:{PORT}")

    while True:
        connection, _ = server.accept()
        with connection:
            buffer = ""
            while True:
                data = connection.recv(4096)
                if not data:
                    break
                buffer += data.decode("utf-8", errors="replace")
                lines = buffer.split("\n")
                buffer = lines.pop()
                parsed = [message for message in map(protocol.parse_message, lines) if message]
                fresh = protocol.select_queueable(parsed, protocol.now_ms(), STALE_GRACE_MS)
                for message in fresh:
                    loop.call_soon_threadsafe(handle_incoming, message)


@bot.event
async def on_ready() -> None:
    global started
    print(f"{bot.user} is now running!")
    print(f"Current voice: {current_voice}")
    print(f'Available voices: {", ".join(VOICES.keys())}')
    if started:
        return
    started = True
    asyncio.create_task(playback_consumer())
    threading.Thread(target=serve_socket, args=(asyncio.get_running_loop(),), daemon=True).start()


@bot.command()
async def join(ctx) -> None:
    """Join the voice channel you're in"""
    global last_joined_channel_id
    if ctx.author.voice:
        channel = ctx.author.voice.channel
        await channel.connect()
        last_joined_channel_id = ctx.channel.id
        await ctx.send(f"Joined {channel.name}!")
    else:
        await ctx.send("You need to be in a voice channel!")


@bot.command()
async def leave(ctx) -> None:
    """Leave the voice channel"""
    if ctx.voice_client:
        await ctx.voice_client.disconnect()
        await ctx.send("Left the voice channel!")
    else:
        await ctx.send("I'm not in a voice channel!")


@bot.command()
async def say(ctx, *, text: str) -> None:
    """Make the bot speak text in voice channel"""
    if not ctx.voice_client:
        await ctx.send("I'm not in a voice channel! Use !join first.")
        return

    if len(text) > 500:
        await ctx.send("Text too long! Max 500 characters.")
        return

    await ctx.send(f"Speaking: *{text[:50]}{'...' if len(text) > 50 else ''}*")
    tts_queue.put_nowait({"type": "shotcall", "text": text})


@bot.command()
async def identify(ctx) -> None:
    """List the spells that identify each class"""
    lines = [
        f"**{class_name}**: {', '.join(spells)}"
        for class_name, spells in protocol.IDENTIFY_SPELLS.items()
    ]
    await ctx.send(
        "Class-identifying spells:\n"
        + "\n".join(lines)
        + "\nEnable advanced combat logging (`ADVANCED_LOG_ENABLED,1`) for automatic detection."
    )


@bot.command()
async def voice(ctx, voice_name: str = None) -> None:
    """Change the voice. Usage: !voice [voice_name] or !voice to list"""
    global current_voice

    if voice_name is None:
        voice_list = "\n".join(
            [f"**{name}** - {get_voice_description(name)}" for name in VOICES.keys()]
        )
        await ctx.send(f"**Current voice:** {current_voice}\n\n**Available voices:**\n{voice_list}")
        return

    voice_name = voice_name.lower()
    if voice_name in VOICES:
        current_voice = voice_name
        await ctx.send(f"Voice changed to: **{current_voice}**")
    else:
        await ctx.send("Invalid voice! Use `!voice` to see available voices.")


def get_voice_description(voice_name: str) -> str:
    """Get description for a voice"""
    descriptions = {
        "rachel": "Calm, young female",
        "domi": "Strong, confident female",
        "bella": "Soft, friendly female",
        "antoni": "Well-rounded male",
        "adam": "Deep male narrator",
        "sam": "Raspy young male",
        "josh": "Conversational male",
        "arnold": "Crisp male narrator",
        "callum": "Hoarse middle-aged male",
        "charlie": "Casual Australian male",
    }
    return descriptions.get(voice_name, "Unknown")


if __name__ == "__main__":
    bot.run(DISCORD_BOT_TOKEN)
