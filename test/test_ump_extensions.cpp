#include <catch2/catch_test_macros.hpp>
#include <pulp/midi/ump.hpp>

#include <cmath>

using namespace pulp::midi;

// ────────────────────────────────────────────────────────────────────────
// Per-note management (status 0xF0) + assignable per-note CC (status 0x10)
// ────────────────────────────────────────────────────────────────────────

TEST_CASE("UmpPacket::per_note_management builds correct status nibble",
          "[midi][ump][per-note]") {
    auto p = UmpPacket::per_note_management(
        /*group*/ 2, /*channel*/ 3, /*note*/ 60,
        UmpPacket::kPerNoteResetControllers);
    REQUIRE(p.word_count == 2);
    REQUIRE(p.message_type() == UmpMessageType::Midi2ChannelVoice);
    REQUIRE(p.group() == 2);
    REQUIRE(p.channel() == 3);
    REQUIRE(p.status() == 0xF3);
    REQUIRE(p.note_number() == 60);
    // Flags live in the low byte of word 0.
    REQUIRE((p.words[0] & 0xFF) == UmpPacket::kPerNoteResetControllers);
    REQUIRE(p.words[1] == 0u);
}

TEST_CASE("UmpPacket::per_note_management supports combined flags",
          "[midi][ump][per-note]") {
    const uint8_t both = UmpPacket::kPerNoteResetControllers
                       | UmpPacket::kPerNoteDetachControllers;
    auto p = UmpPacket::per_note_management(0, 0, 64, both);
    REQUIRE((p.words[0] & 0xFF) == 0x03);
}

TEST_CASE("UmpPacket::assignable_per_note_cc encodes status 0x10",
          "[midi][ump][per-note]") {
    auto p = UmpPacket::assignable_per_note_cc(
        /*group*/ 0, /*channel*/ 1,
        /*note*/ 72, /*cc_index*/ 5, /*value*/ 0x80000000);
    REQUIRE(p.word_count == 2);
    REQUIRE(p.message_type() == UmpMessageType::Midi2ChannelVoice);
    REQUIRE(p.group() == 0);
    REQUIRE(p.channel() == 1);
    REQUIRE(p.status() == 0x11);
    REQUIRE(p.note_number() == 72);
    REQUIRE((p.words[0] & 0xFF) == 5);
    REQUIRE(p.words[1] == 0x80000000u);
}

TEST_CASE("UmpPacket::registered_per_note_cc and assignable distinguish status",
          "[midi][ump][per-note]") {
    auto reg = UmpPacket::registered_per_note_cc(0, 0, 60, 7, 100);
    auto asn = UmpPacket::assignable_per_note_cc(0, 0, 60, 7, 100);
    REQUIRE((reg.words[0] >> 20) != (asn.words[0] >> 20));
    REQUIRE(reg.status() == 0x00);
    REQUIRE(asn.status() == 0x10);
}

// ────────────────────────────────────────────────────────────────────────
// JR Clock / JR Timestamp (UMP Utility type 0x0)
// ────────────────────────────────────────────────────────────────────────

TEST_CASE("make_jr_clock builds a type-0 packet with status 0x1",
          "[midi][ump][jr-timing]") {
    auto p = make_jr_clock(/*group*/ 0, /*value*/ 0x1234);
    REQUIRE(p.word_count == 1);
    REQUIRE(p.message_type() == UmpMessageType::Utility);
    REQUIRE(utility_status(p) == UtilityStatus::JrClock);
    REQUIRE(jr_value_16(p) == 0x1234);
}

TEST_CASE("make_jr_timestamp builds a type-0 packet with status 0x2",
          "[midi][ump][jr-timing]") {
    auto p = make_jr_timestamp(/*group*/ 3, /*value*/ 31250);
    REQUIRE(p.word_count == 1);
    REQUIRE(p.message_type() == UmpMessageType::Utility);
    REQUIRE(p.group() == 3);
    REQUIRE(utility_status(p) == UtilityStatus::JrTimestamp);
    REQUIRE(jr_value_16(p) == 31250);
}

TEST_CASE("JR tick conversions match the 1/31250 s spec",
          "[midi][ump][jr-timing]") {
    // 31250 ticks = 1 s exactly.
    REQUIRE(jr_value_seconds(31250) == 1.0);
    // 1 tick = 32 microseconds (1e6 / 31250).
    REQUIRE(std::abs(jr_value_microseconds(1) - 32.0) < 1e-9);
    // Round-trip a few selected values.
    for (uint16_t v : {0, 1, 1000, 15625, 31249, 65535}) {
        const double s = jr_value_seconds(v);
        REQUIRE(std::abs(s * 31250.0 - static_cast<double>(v)) < 1e-6);
    }
}

TEST_CASE("utility_status of non-JR utility packets is correctly extracted",
          "[midi][ump][jr-timing]") {
    UmpPacket noop;
    noop.word_count = 1;
    noop.words[0] = 0x0u << 28; // status 0x0 = Noop
    REQUIRE(utility_status(noop) == UtilityStatus::Noop);
    REQUIRE(jr_value_16(noop) == 0);

    UmpPacket dc_ticks;
    dc_ticks.word_count = 1;
    dc_ticks.words[0] = (0x0u << 28) | (0x3u << 20) | 0x0080;
    REQUIRE(utility_status(dc_ticks) == UtilityStatus::DeltaClockstampTicksPerQuarter);
    REQUIRE(jr_value_16(dc_ticks) == 0x0080);
}
