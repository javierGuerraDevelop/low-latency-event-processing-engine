"""Protocol helpers: framing, staleness, routing, and the identify map."""

import protocol


def test_parse_message_accepts_both_types():
    shotcall = protocol.parse_message(
        '{"v":1,"type":"shotcall","text":"kick","due_ms":1000,"call_id":7}'
    )
    assert shotcall == {
        "v": 1,
        "type": "shotcall",
        "text": "kick",
        "due_ms": 1000,
        "call_id": 7,
    }

    status = protocol.parse_message('{"v":1,"type":"party_status","text":"3/5"}')
    assert status is not None
    assert status["type"] == "party_status"


def test_parse_message_rejects_malformed_frames():
    assert protocol.parse_message("") is None
    assert protocol.parse_message("   ") is None
    assert protocol.parse_message("{not json") is None
    assert protocol.parse_message("[1, 2, 3]") is None
    assert protocol.parse_message('"a string"') is None
    assert protocol.parse_message('{"type":"unknown","text":"x"}') is None
    assert protocol.parse_message('{"type":"shotcall"}') is None
    assert protocol.parse_message('{"type":"shotcall","text":5}') is None


def test_is_stale_shotcall_boundary():
    message = {"type": "shotcall", "text": "x", "due_ms": 10_000}
    assert protocol.is_stale(message, 11_999, 2_000) is False
    assert protocol.is_stale(message, 12_000, 2_000) is False
    assert protocol.is_stale(message, 12_001, 2_000) is True
    assert protocol.is_stale({"type": "shotcall", "text": "x"}, 1, 2_000) is True


def test_status_messages_are_never_stale():
    message = {"type": "party_status", "text": "3/5"}
    assert protocol.is_stale(message, 10**15, 0) is False


def test_route_by_type():
    assert protocol.route({"type": "shotcall"}) == "tts"
    assert protocol.route({"type": "party_status"}) == "channel"


def test_select_queueable_drops_stale_shotcalls_keeps_status():
    messages = [
        {"type": "shotcall", "text": "fresh", "due_ms": 5_000},
        {"type": "shotcall", "text": "stale", "due_ms": 1_000},
        {"type": "party_status", "text": "3/5"},
    ]
    queued = protocol.select_queueable(messages, now_ms=2_500, grace_ms=1_000)
    assert [message["text"] for message in queued] == ["fresh", "3/5"]


def test_identify_spells_cover_every_class():
    assert len(protocol.IDENTIFY_SPELLS) == 13
    assert all(spells for spells in protocol.IDENTIFY_SPELLS.values())
    assert "Warrior" in protocol.IDENTIFY_SPELLS
    assert any("Battle Shout" in spell for spell in protocol.IDENTIFY_SPELLS["Warrior"])
    assert any("Mark of the Wild" in spell for spell in protocol.IDENTIFY_SPELLS["Druid"])
