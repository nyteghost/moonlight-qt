// Tests for app/streaming/vibelight_logic.h. Build and run with:
//   g++ -std=c++17 -Wall -Wextra -I app tests/logic_tests.cpp -o logic_tests && ./logic_tests

#include "streaming/vibelight_logic.h"

#include <cstdio>
#include <cstdlib>
#include <string>

static int g_failures = 0;

#define CHECK(cond) \
    do { \
        if (!(cond)) { \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); \
            g_failures++; \
        } \
    } while (0)

static void expectKey(char c, unsigned short vk, bool shift)
{
    unsigned short gotVk = 0;
    bool gotShift = false;
    bool ok = vibelight::asciiToKey(c, gotVk, gotShift);
    if (!ok || gotVk != vk || gotShift != shift) {
        std::printf("FAIL asciiToKey('%c' 0x%02x): got ok=%d vk=0x%02X shift=%d, want vk=0x%02X shift=%d\n",
                    (c >= 32 && c < 127) ? c : '?', (unsigned char)c, ok, gotVk, gotShift, vk, shift);
        g_failures++;
    }
}

static void testKeys()
{
    // Letters and digits
    expectKey('a', 0x41, false);
    expectKey('z', 0x5A, false);
    expectKey('A', 0x41, true);
    expectKey('Z', 0x5A, true);
    expectKey('0', 0x30, false);
    expectKey('9', 0x39, false);

    // Shifted digits
    expectKey('!', 0x31, true);
    expectKey('@', 0x32, true);
    expectKey('#', 0x33, true);
    expectKey('(', 0x39, true);
    expectKey(')', 0x30, true);

    // Punctuation, plain and shifted
    expectKey('-', 0xBD, false);
    expectKey('_', 0xBD, true);
    expectKey('=', 0xBB, false);
    expectKey('+', 0xBB, true);
    expectKey('[', 0xDB, false);
    expectKey('{', 0xDB, true);
    expectKey(']', 0xDD, false);
    expectKey('}', 0xDD, true);
    expectKey('\\', 0xDC, false);
    expectKey('|', 0xDC, true);
    expectKey(';', 0xBA, false);
    expectKey(':', 0xBA, true);
    expectKey('\'', 0xDE, false);
    expectKey('"', 0xDE, true);
    expectKey(',', 0xBC, false);
    expectKey('<', 0xBC, true);
    expectKey('.', 0xBE, false);
    expectKey('>', 0xBE, true);
    expectKey('/', 0xBF, false);
    expectKey('?', 0xBF, true);
    expectKey('`', 0xC0, false);
    expectKey('~', 0xC0, true);

    // Whitespace
    expectKey(' ', 0x20, false);
    expectKey('\n', 0x0D, false);
    expectKey('\r', 0x0D, false);
    expectKey('\t', 0x09, false);

    // Everything printable on a US keyboard must be typeable, and nothing else should be
    for (int c = 0x20; c < 0x7F; c++) {
        unsigned short vk;
        bool shift;
        CHECK(vibelight::asciiToKey((char)c, vk, shift));
    }

    // No key for control characters and non-ASCII bytes
    unsigned short vk;
    bool shift;
    CHECK(!vibelight::asciiToKey('\0', vk, shift));
    CHECK(!vibelight::asciiToKey('\x07', vk, shift));
    CHECK(!vibelight::asciiToKey('\x7F', vk, shift));
    CHECK(!vibelight::asciiToKey((char)0xC3, vk, shift));
}

static void testBitrate()
{
    using vibelight::suggestBitrateMbps;

    // Heavy loss: back off hard. 5% -> 60%
    CHECK(suggestBitrateMbps(50, 5.0, 20, 50) == 30);
    // Moderate loss: 80%
    CHECK(suggestBitrateMbps(50, 1.5, 20, 50) == 40);
    // The 1% and 3% boundaries belong to the stronger reduction
    CHECK(suggestBitrateMbps(50, 1.0, 20, 50) == 40);
    CHECK(suggestBitrateMbps(50, 3.0, 20, 50) == 30);
    // Just under 1%: no reduction and not clean enough to raise
    CHECK(suggestBitrateMbps(50, 0.9, 20, 50) == 50);

    // Clean and saturated: raise by 25%
    CHECK(suggestBitrateMbps(40, 0.0, 10, 40) == 50);
    CHECK(suggestBitrateMbps(40, 0.2, 29, 33) == 50); // 33 >= 80% of 40
    // Unknown round-trip time still counts as fine
    CHECK(suggestBitrateMbps(40, 0.0, 0, 40) == 50);

    // Clean but the stream isn't using its bitrate: leave alone
    CHECK(suggestBitrateMbps(40, 0.0, 10, 20) == 40);
    // Clean but slow round trip: leave alone
    CHECK(suggestBitrateMbps(40, 0.0, 30, 40) == 40);
    // Not quite clean: leave alone
    CHECK(suggestBitrateMbps(40, 0.3, 10, 40) == 40);

    // Raised value is capped at 150 Mbps
    CHECK(suggestBitrateMbps(140, 0.0, 10, 140) == 150);
    CHECK(suggestBitrateMbps(150, 0.0, 10, 150) == 150);

    // Never below 5 Mbps
    CHECK(suggestBitrateMbps(6, 10.0, 10, 6) == 5);
    CHECK(suggestBitrateMbps(2, 0.0, 10, 0) == 5);

    // Rounds to whole Mbps
    CHECK(suggestBitrateMbps(25, 5.0, 10, 25) == 15);   // 15.0
    CHECK(suggestBitrateMbps(33, 1.5, 10, 33) == 26);   // 26.4 -> 26
    CHECK(suggestBitrateMbps(35, 1.5, 10, 35) == 28);   // 28.0
}

static void testHotkey()
{
    using vibelight::sanitizeMenuHotkey;

    CHECK(sanitizeMenuHotkey("H") == 'H');
    CHECK(sanitizeMenuHotkey("h") == 'H');
    CHECK(sanitizeMenuHotkey("t") == 'T');
    CHECK(sanitizeMenuHotkey("Y") == 'Y');

    // Anything invalid falls back to H
    CHECK(sanitizeMenuHotkey("") == 'H');
    CHECK(sanitizeMenuHotkey("HH") == 'H');
    CHECK(sanitizeMenuHotkey("1") == 'H');
    CHECK(sanitizeMenuHotkey("!") == 'H');

    // Letters already used by other actions are refused
    const char* taken = "QZXSMCDVLEK";
    for (const char* p = taken; *p != '\0'; p++) {
        CHECK(sanitizeMenuHotkey(std::string(1, *p)) == 'H');
        CHECK(sanitizeMenuHotkey(std::string(1, (char)(*p - 'A' + 'a'))) == 'H');
    }

    // The offered letters and the taken letters never overlap, and together they are the alphabet
    int offered = 0;
    for (const char* p = vibelight::menuHotkeyChoices(); *p != '\0'; p++) {
        offered++;
        CHECK(std::string(taken).find(*p) == std::string::npos);
    }
    CHECK(offered + (int)std::string(taken).size() == 26);
}

int main()
{
    testKeys();
    testBitrate();
    testHotkey();

    if (g_failures == 0) {
        std::printf("All logic tests passed\n");
        return 0;
    }
    std::printf("%d check(s) failed\n", g_failures);
    return 1;
}
