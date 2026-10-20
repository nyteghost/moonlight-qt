#pragma once

// Small pieces of Vibelight logic with no Qt or SDL dependencies, so they can be unit tested
// on their own (see tests/logic_tests.cpp, run by CI).

#include <algorithm>
#include <cmath>

namespace vibelight {

// Maps one ASCII character to the Windows virtual-key code that types it on a US keyboard.
// Returns false for characters that have no key (accents, emoji, other control characters).
inline bool asciiToKey(char c, unsigned short& vk, bool& shift)
{
    shift = false;

    if (c >= 'a' && c <= 'z') { vk = (unsigned short)(0x41 + (c - 'a')); return true; }
    if (c >= 'A' && c <= 'Z') { vk = (unsigned short)(0x41 + (c - 'A')); shift = true; return true; }
    if (c >= '0' && c <= '9') { vk = (unsigned short)(0x30 + (c - '0')); return true; }

    static const char kShiftedDigits[] = ")!@#$%^&*(";
    for (int i = 0; i < 10; i++) {
        if (c == kShiftedDigits[i]) { vk = (unsigned short)(0x30 + i); shift = true; return true; }
    }

    static const struct { char plain; char shifted; unsigned short vk; } kPunct[] = {
        {'-', '_', 0xBD}, {'=', '+', 0xBB}, {'[', '{', 0xDB}, {']', '}', 0xDD},
        {'\\', '|', 0xDC}, {';', ':', 0xBA}, {'\'', '"', 0xDE}, {',', '<', 0xBC},
        {'.', '>', 0xBE}, {'/', '?', 0xBF}, {'`', '~', 0xC0},
    };
    for (const auto& p : kPunct) {
        if (c == p.plain) { vk = p.vk; return true; }
        if (c == p.shifted) { vk = p.vk; shift = true; return true; }
    }

    if (c == ' ') { vk = 0x20; return true; }
    if (c == '\n' || c == '\r') { vk = 0x0D; return true; }
    if (c == '\t') { vk = 0x09; return true; }

    return false;
}

// The bitrate (in whole Mbps) to suggest after a connection check.
//   dropPercent  - share of frames the network dropped during the check
//   rttMs        - round-trip time, 0 if unknown
//   receivedMbps - video bitrate the stream actually received during the check
inline double suggestBitrateMbps(double currentMbps, double dropPercent, unsigned int rttMs, double receivedMbps)
{
    double suggested = currentMbps;

    if (dropPercent >= 3.0) {
        suggested = currentMbps * 0.6;
    }
    else if (dropPercent >= 1.0) {
        suggested = currentMbps * 0.8;
    }
    else if (dropPercent < 0.3 && (rttMs == 0 || rttMs < 30) && receivedMbps >= currentMbps * 0.8) {
        // Clean, and the stream is actually using what it was given: there may be room to grow
        suggested = std::min(currentMbps * 1.25, 150.0);
    }

    return std::max(5.0, (double)(int)(suggested + 0.5));
}

} // namespace vibelight
