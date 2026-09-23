// Golden-vector unit test for the Stages SB20 handlebar-button decode (issue #4785).
//
// The byte strings below are the ACTUAL frames captured from a real Stages SB20 (bike session 3,
// 2026-06-19). They are recorded in the SB20-power-proxy project at
//   code/findings/captures/SHIFTER-probe-3-20260619-0838.jsonl
// and documented in code/findings/shifter-ble-protocol.md. Using the real captured bytes (rather than
// invented ones) is what makes this a meaningful test of the on-air behaviour.
//
// Frame model on vendor char 0c46be60 (stateless one-hot button events):
//   held        01 00 <bit:u16 LE>                  streamed ~10-20x while a button is held
//   commit      03 00 <bit:u16 LE> <bit:u16 LE>     the one action-per-press edge (both fields = the bit)
//   terminator  04 00 <bit> | 08 00 <bit>           end of burst
// Only the 0x03 commit frame carries an action; the held-stream + terminators must decode to None,
// which is what collapses the burst of a single press into exactly one in-app action.

#include <gtest/gtest.h>

#include <QByteArray>

#include "devices/ftmsbike/ftmsbike.h"

using Action = ftmsbike::Sb20ButtonAction;

static QByteArray frame(const char *hex) { return QByteArray::fromHex(hex); }

// Every real captured 0x03 commit frame maps to its intended in-app action.
// (counts from the session-3 capture, for provenance: 3/7/3/1/2/10 presses respectively)
TEST(Sb20Buttons, RealCommitFramesMapToActions) {
    EXPECT_EQ(ftmsbike::decodeSb20Button(frame("030001000100")), Action::TargetPowerUp);     // LEFT up
    EXPECT_EQ(ftmsbike::decodeSb20Button(frame("030002000200")), Action::TargetPowerDown);   // LEFT down
    EXPECT_EQ(ftmsbike::decodeSb20Button(frame("030004000400")), Action::GearDown);          // LEFT 3rd
    EXPECT_EQ(ftmsbike::decodeSb20Button(frame("030008000800")), Action::PelotonOffsetUp);   // RIGHT up
    EXPECT_EQ(ftmsbike::decodeSb20Button(frame("030010001000")), Action::PelotonOffsetDown); // RIGHT down
    EXPECT_EQ(ftmsbike::decodeSb20Button(frame("030020002000")), Action::GearUp);            // RIGHT 3rd
}

// The held-stream (0x01) and both terminators (0x04/0x08) carry the same button bit but no action,
// so a whole press-burst yields exactly one action. Real RIGHT-3rd (bit 0x0020) frames from the capture.
TEST(Sb20Buttons, HeldAndTerminatorFramesAreIgnored) {
    EXPECT_EQ(ftmsbike::decodeSb20Button(frame("01002000")), Action::None); // held
    EXPECT_EQ(ftmsbike::decodeSb20Button(frame("04002000")), Action::None); // terminator (gear changed)
    EXPECT_EQ(ftmsbike::decodeSb20Button(frame("08002000")), Action::None); // terminator (at limit)
}

// Malformed, too-short, or unmapped commit frames must never fire an action.
TEST(Sb20Buttons, ShortOrUnmappedFramesAreNone) {
    EXPECT_EQ(ftmsbike::decodeSb20Button(QByteArray()), Action::None);        // empty
    EXPECT_EQ(ftmsbike::decodeSb20Button(frame("0300")), Action::None);       // too short (2 bytes)
    EXPECT_EQ(ftmsbike::decodeSb20Button(frame("030000000000")), Action::None); // no bit set (idle)
    EXPECT_EQ(ftmsbike::decodeSb20Button(frame("030040004000")), Action::None); // 0x0040: no such button
    EXPECT_EQ(ftmsbike::decodeSb20Button(frame("030003000300")), Action::None); // multi-bit: not a valid press
}
