#if EDITOR

#include "ClaudeCodeParser.h"

#include "Log.h"

#include <algorithm>
#include <cstdint>
#include <cstring>

namespace
{
    bool IsBlank(const char* data, size_t len)
    {
        for (size_t i = 0; i < len; ++i)
        {
            char c = data[i];
            if (c != ' ' && c != '\t')
            {
                return false;
            }
        }
        return true;
    }

    /**
     * ASCII fallback for a single Unicode codepoint that the editor's bundled
     * ImGui font likely doesn't have. Returns nullptr if no substitution is
     * known (caller should leave the original UTF-8 bytes alone).
     *
     * Coverage focuses on the glyph blocks Ink-rendered apps lean on:
     *   - U+2500..U+257F  Box Drawing
     *   - U+2580..U+259F  Block Elements (subset)
     *   - U+25A0..U+25FF  Geometric Shapes (subset)
     *   - U+2190..U+21FF  Arrows (common ones)
     *   - U+2022          Bullet
     *   - U+2026          Horizontal ellipsis
     *   - U+2713/U+2717   Check / cross marks
     */
    const char* AsciiFallbackForCodepoint(uint32_t cp)
    {
        // Box drawing — light & heavy & double, all collapsed to ASCII
        // line-art so a frame around content stays visually distinguishable.
        switch (cp)
        {
            // Horizontals
            case 0x2500: case 0x2501: case 0x2504: case 0x2505:
            case 0x2508: case 0x2509: case 0x254C: case 0x254D:
            case 0x2550:
                return "-";

            // Verticals
            case 0x2502: case 0x2503: case 0x2506: case 0x2507:
            case 0x250A: case 0x250B: case 0x254E: case 0x254F:
            case 0x2551:
                return "|";

            // Corners (square + rounded + double)
            case 0x250C: case 0x250D: case 0x250E: case 0x250F: // top-left
            case 0x2510: case 0x2511: case 0x2512: case 0x2513: // top-right
            case 0x2514: case 0x2515: case 0x2516: case 0x2517: // bot-left
            case 0x2518: case 0x2519: case 0x251A: case 0x251B: // bot-right
            case 0x256D: case 0x256E: case 0x256F: case 0x2570: // rounded
            case 0x2552: case 0x2553: case 0x2554:              // double
            case 0x2555: case 0x2556: case 0x2557:
            case 0x2558: case 0x2559: case 0x255A:
            case 0x255B: case 0x255C: case 0x255D:
                return "+";

            // T-junctions and crosses
            case 0x251C: case 0x251D: case 0x251E: case 0x251F:
            case 0x2520: case 0x2521: case 0x2522: case 0x2523:
            case 0x2524: case 0x2525: case 0x2526: case 0x2527:
            case 0x2528: case 0x2529: case 0x252A: case 0x252B:
            case 0x252C: case 0x252D: case 0x252E: case 0x252F:
            case 0x2530: case 0x2531: case 0x2532: case 0x2533:
            case 0x2534: case 0x2535: case 0x2536: case 0x2537:
            case 0x2538: case 0x2539: case 0x253A: case 0x253B:
            case 0x253C: case 0x253D: case 0x253E: case 0x253F:
            case 0x2540: case 0x2541: case 0x2542: case 0x2543:
            case 0x2544: case 0x2545: case 0x2546: case 0x2547:
            case 0x2548: case 0x2549: case 0x254A: case 0x254B:
            case 0x255E: case 0x255F: case 0x2560:
            case 0x2561: case 0x2562: case 0x2563:
            case 0x2564: case 0x2565: case 0x2566:
            case 0x2567: case 0x2568: case 0x2569:
            case 0x256A: case 0x256B: case 0x256C:
                return "+";

            // Block elements (full / half / shading / quadrants) — solid block
            case 0x2580: case 0x2581: case 0x2582: case 0x2583:
            case 0x2584: case 0x2585: case 0x2586: case 0x2587:
            case 0x2588: case 0x2589: case 0x258A: case 0x258B:
            case 0x258C: case 0x258D: case 0x258E: case 0x258F:
            case 0x2590: case 0x2594: case 0x2595:
            case 0x2591: case 0x2592: case 0x2593:
            // Quadrant blocks (used by Ink for the claude logo)
            // U+2596..U+259F covers all combinations of corner quadrants.
            case 0x2596: case 0x2597: case 0x2598: case 0x2599:
            case 0x259A: case 0x259B: case 0x259C: case 0x259D:
            case 0x259E: case 0x259F:
                return "#";

            // Geometric shapes
            case 0x25A0: case 0x25A1:           // black/white square
                return "[]";
            case 0x25CF: case 0x25CB: case 0x25C9: case 0x25CE:
                return "o";
            case 0x25B6: case 0x25BA: case 0x25B8:  // right triangle
                return ">";
            case 0x25C0: case 0x25C4: case 0x25C2:  // left triangle
                return "<";
            case 0x25B2: case 0x25B4:               // up triangle
                return "^";
            case 0x25BC: case 0x25BE:               // down triangle
                return "v";

            // Arrows
            case 0x2190: return "<-";   // left
            case 0x2192: return "->";   // right
            case 0x2191: return "^";    // up
            case 0x2193: return "v";    // down
            case 0x2194: return "<->";
            case 0x21D0: return "<=";
            case 0x21D2: return "=>";
            case 0x21B5: return "\\n";  // return symbol

            // Bullets and dots
            case 0x2022: return "*";    // bullet
            case 0x00B7: return ".";    // middle dot
            case 0x2027: return ".";    // hyphenation point
            case 0x2218: return ".";    // ring operator
            case 0x2219: return ".";    // bullet operator

            // Ellipsis
            case 0x2026: return "...";

            // Check / cross marks
            case 0x2713: case 0x2714: return "v"; // check
            case 0x2717: case 0x2718: return "x"; // ballot x

            // Dingbats — Ink uses the sparkle/asterisk subset (U+2736..U+273B)
            // for its idle/thinking spinner animation. Collapse them all to
            // a single ASCII '*' so the spinner appears as a static '*'
            // marker that the consecutive-line dedupe can then collapse to
            // a single row.
            case 0x2731: case 0x2732: case 0x2733: case 0x2734: case 0x2735:
            case 0x2736: case 0x2737: case 0x2738: case 0x2739:
            case 0x273A: case 0x273B: case 0x273C: case 0x273D:
            case 0x2742: case 0x2743: case 0x2744: case 0x2745: case 0x2746:
            case 0x2747: case 0x2748: case 0x2749: case 0x274A: case 0x274B:
                return "*";

            // Heavy / dotted variants of the asterisk family
            case 0x271A: return "+"; // heavy plus
            case 0x274C: return "x"; // cross mark
            case 0x274E: return "x"; // negative squared cross
            case 0x2756: return "*"; // diamond minus white x
            case 0x2762: return "!"; // heavy exclamation

            // Common quotes / dashes that some fonts also miss
            case 0x2018: case 0x2019: return "'";
            case 0x201C: case 0x201D: return "\"";
            case 0x2013: case 0x2014: return "-";
            case 0x00A0:              return " "; // nbsp

            default: break;  // fall through to range checks below
        }

        // ---- Range catch-alls ----
        // Dingbats block (U+2700..U+27BF). Ink uses many of these as spinner
        // animation frames. Anything not matched explicitly above (which has
        // a more specific ASCII equivalent like check/cross marks) is
        // collapsed to '*' so the consecutive-line dedupe can suppress
        // spinner storms cleanly.
        if (cp >= 0x2700 && cp <= 0x27BF)
        {
            return "*";
        }

        // Miscellaneous Symbols and Arrows (U+2B00..U+2BFF) — also used by
        // Ink for arrow / triangle decorations. Map to nearest ASCII.
        if (cp >= 0x2B00 && cp <= 0x2B0F) return "^";  // up arrows
        if (cp >= 0x2B10 && cp <= 0x2B1F) return "v";  // down arrows
        if (cp >= 0x2B05 && cp <= 0x2B05) return "<";
        if (cp >= 0x2B06 && cp <= 0x2B06) return "^";
        if (cp >= 0x2B07 && cp <= 0x2B07) return "v";
        if (cp >= 0x2B95 && cp <= 0x2B95) return ">";

        return nullptr;
    }

    /**
     * Decode the next UTF-8 codepoint starting at `data[i]`. Returns the
     * codepoint and writes the number of bytes consumed to `outLen`. On
     * malformed input, returns the lone byte and outLen=1 so the caller
     * can pass it through verbatim.
     */
    uint32_t DecodeUtf8(const char* data, size_t len, size_t i, size_t& outLen)
    {
        unsigned char b0 = static_cast<unsigned char>(data[i]);
        if (b0 < 0x80)
        {
            outLen = 1;
            return b0;
        }
        auto cont = [&](size_t j) -> int {
            if (j >= len) return -1;
            unsigned char b = static_cast<unsigned char>(data[j]);
            if ((b & 0xC0) != 0x80) return -1;
            return b & 0x3F;
        };
        if ((b0 & 0xE0) == 0xC0)
        {
            int c1 = cont(i + 1);
            if (c1 < 0) { outLen = 1; return b0; }
            outLen = 2;
            return ((b0 & 0x1Fu) << 6) | static_cast<uint32_t>(c1);
        }
        if ((b0 & 0xF0) == 0xE0)
        {
            int c1 = cont(i + 1);
            int c2 = cont(i + 2);
            if (c1 < 0 || c2 < 0) { outLen = 1; return b0; }
            outLen = 3;
            return ((b0 & 0x0Fu) << 12)
                 | (static_cast<uint32_t>(c1) << 6)
                 |  static_cast<uint32_t>(c2);
        }
        if ((b0 & 0xF8) == 0xF0)
        {
            int c1 = cont(i + 1);
            int c2 = cont(i + 2);
            int c3 = cont(i + 3);
            if (c1 < 0 || c2 < 0 || c3 < 0) { outLen = 1; return b0; }
            outLen = 4;
            return ((b0 & 0x07u) << 18)
                 | (static_cast<uint32_t>(c1) << 12)
                 | (static_cast<uint32_t>(c2) << 6)
                 |  static_cast<uint32_t>(c3);
        }
        outLen = 1;
        return b0;
    }
}

ClaudeCodeParser::ClaudeCodeParser() = default;

void ClaudeCodeParser::Reset()
{
    mStripper.Reset();
    mStripper.SetSynthesizeFrameNewlines(true);
    mAltScreen = false;
    mPeephole.clear();
    mEndsWithNewline = true;
    mRecentEmittedLines.clear();
}

void ClaudeCodeParser::ScanAltScreenToggles(const char* data, size_t len)
{
    static const char kEnter[] = "\x1b[?1049h";
    static const char kLeave[] = "\x1b[?1049l";
    constexpr size_t kSeqLen = sizeof(kEnter) - 1;  // 8

    for (size_t i = 0; i < len; ++i)
    {
        char c = data[i];
        mPeephole.push_back(c);
        if (mPeephole.size() > kSeqLen)
        {
            mPeephole.erase(mPeephole.begin(),
                            mPeephole.begin() + (mPeephole.size() - kSeqLen));
        }
        if (mPeephole.size() == kSeqLen)
        {
            if (std::memcmp(mPeephole.data(), kEnter, kSeqLen) == 0)
            {
                if (!mAltScreen)
                {
                    LogDebug("[CLI][claude] Alt-screen ENTER (ESC[?1049h)");
                }
                mAltScreen = true;
                // Stop the inner stripper from synthesising a newline on
                // every cursor-position sequence. In Ink-rendered apps the
                // spinner / progress text updates individual cells via
                // ESC[<row>;<col>H, and the synthetic newlines were turning
                // each updated character into its own log row.
                mStripper.SetSynthesizeFrameNewlines(false);
                // Fresh dedupe slate for the new alt-screen session.
                mRecentEmittedLines.clear();
            }
            else if (std::memcmp(mPeephole.data(), kLeave, kSeqLen) == 0)
            {
                if (mAltScreen)
                {
                    LogDebug("[CLI][claude] Alt-screen LEAVE (ESC[?1049l)");
                }
                mAltScreen = false;
                mStripper.SetSynthesizeFrameNewlines(true);
                mRecentEmittedLines.clear();
            }
        }
    }
}

void ClaudeCodeParser::RewriteUnrenderableGlyphs(std::string& s)
{
    if (s.empty())
    {
        return;
    }

    // Single pass: walk UTF-8 codepoints, rebuild into a new buffer.
    // Skipped only when we know the input is pure ASCII (fast path).
    bool anyHigh = false;
    for (char c : s)
    {
        if (static_cast<unsigned char>(c) >= 0x80)
        {
            anyHigh = true;
            break;
        }
    }
    if (!anyHigh)
    {
        return;
    }

    std::string out;
    out.reserve(s.size());

    for (size_t i = 0; i < s.size(); )
    {
        size_t consumed = 0;
        uint32_t cp = DecodeUtf8(s.data(), s.size(), i, consumed);
        if (consumed == 0) consumed = 1;

        if (cp >= 0x80)
        {
            const char* repl = AsciiFallbackForCodepoint(cp);
            if (repl != nullptr)
            {
                out.append(repl);
                i += consumed;
                continue;
            }
        }

        out.append(s, i, consumed);
        i += consumed;
    }

    s.swap(out);
}

std::string ClaudeCodeParser::Process(const char* data, size_t len)
{
    if (len == 0)
    {
        return {};
    }

    ScanAltScreenToggles(data, len);

    // Run through the generic stripper. The stripper already drops the
    // ESC[?1049h/l, ESC[?25h/l, ESC[?2004h/l, ESC[?1006h/l, ESC[6n etc.
    // sequences via its generic CSI handler.
    std::string stripped = mStripper.Process(data, len);
    if (stripped.empty())
    {
        return {};
    }

    return PostProcess(std::move(stripped));
}

std::string ClaudeCodeParser::PostProcess(std::string stripped)
{
    // 1) Glyph fallback for box drawing / arrows / bullets / etc. so the
    //    editor's bundled ImGui font (which lacks these blocks) doesn't
    //    render them as "?".
    RewriteUnrenderableGlyphs(stripped);

    // 2) Blank-line collapse — at most one blank line in a row. State is
    //    carried across chunks via mEndsWithNewline so a blank line that
    //    straddles a chunk boundary is still detected.
    std::string out;
    out.reserve(stripped.size());

    int  consecutiveBlanks = mEndsWithNewline ? 1 : 0;
    size_t start = 0;

    while (start <= stripped.size())
    {
        size_t nl = stripped.find('\n', start);
        bool   complete = (nl != std::string::npos);
        size_t end = complete ? nl : stripped.size();
        size_t lineLen = end - start;

        if (IsBlank(stripped.data() + start, lineLen))
        {
            if (complete)
            {
                ++consecutiveBlanks;
                if (consecutiveBlanks <= 1)
                {
                    out.push_back('\n');
                }
            }
            else
            {
                // Trailing partial blank — pass it through so any pending
                // text we might add later in the next chunk lines up.
                out.append(stripped, start, lineLen);
            }
        }
        else
        {
            // Move-to-front LRU dedupe in alt-screen mode. Each Ink frame
            // redraws the same chrome (header, separators, prompt, status)
            // every animation tick. The first frame populates the LRU; on
            // subsequent frames each repeated row is found in the LRU,
            // dropped, and bumped to the front so it never ages out. Only
            // genuinely new lines (a spinner glyph rotating, a typed
            // character, etc.) are emitted.
            //
            // Disabled outside alt-screen so plain pipe-mode output is
            // not touched. Disabled for partial trailing lines (incomplete
            // chunk) so we don't dedupe a half-line.
            bool dropped = false;
            if (complete && mAltScreen)
            {
                std::string lineStr(stripped.data() + start, lineLen);
                auto it = std::find(mRecentEmittedLines.begin(),
                                    mRecentEmittedLines.end(), lineStr);
                if (it != mRecentEmittedLines.end())
                {
                    // Hit — drop the line and bump it to the front so the
                    // hot frame chrome stays warm. consecutiveBlanks is
                    // intentionally not reset so adjacent blank-line runs
                    // still collapse cleanly via the blank rule above.
                    if (it != mRecentEmittedLines.begin())
                    {
                        std::string moved = std::move(*it);
                        mRecentEmittedLines.erase(it);
                        mRecentEmittedLines.push_front(std::move(moved));
                    }
                    dropped = true;
                }
                else
                {
                    // Miss — emit and remember at front.
                    mRecentEmittedLines.push_front(std::move(lineStr));
                    while (mRecentEmittedLines.size() > kRecentEmittedMax)
                    {
                        mRecentEmittedLines.pop_back();
                    }
                }
            }

            if (!dropped)
            {
                consecutiveBlanks = 0;
                out.append(stripped, start, lineLen);
                if (complete)
                {
                    out.push_back('\n');
                }
            }
        }

        if (!complete)
        {
            break;
        }
        start = end + 1;
    }

    if (!out.empty())
    {
        mEndsWithNewline = (out.back() == '\n');
    }

    return out;
}

#endif
