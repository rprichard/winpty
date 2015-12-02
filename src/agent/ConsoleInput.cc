// Copyright (c) 2011-2015 Ryan Prichard
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to
// deal in the Software without restriction, including without limitation the
// rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
// sell copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.

#include "ConsoleInput.h"

#include <stdio.h>
#include <string.h>

#include <string>

#include "Win32Console.h"
#include "DsrSender.h"
#include "../shared/DebugClient.h"
#include "../shared/UnixCtrlChars.h"

#ifndef MAPVK_VK_TO_VSC
#define MAPVK_VK_TO_VSC 0
#endif

const int kIncompleteEscapeTimeoutMs = 1000;

#define ESC "\x1B"
#define CSI ESC"["
#define DIM(x) (sizeof(x) / sizeof((x)[0]))

ConsoleInput::ConsoleInput(DsrSender *dsrSender) :
    m_console(new Win32Console),
    m_dsrSender(dsrSender),
    m_dsrSent(false),
    lastWriteTick(0)
{
    struct KeyDescriptor {
        const char *encoding;
        InputMap::Key key;
    };
    const int vkLB = VkKeyScan('[') & 0xFF;
    static const KeyDescriptor keyDescriptorTable[] = {
        // Ctrl-<letter/digit> seems to be handled OK by the default code path.
        // TODO: Alt-ESC is encoded as ESC ESC.  Can it be handled?

        {   ESC,        { VK_ESCAPE,    '\x1B', 0,                  } },

        // Alt-<letter/digit>
        {   ESC"O",     { 'O',          'O',    LEFT_ALT_PRESSED | SHIFT_PRESSED    } },
        {   ESC"[",     { vkLB,         '[',    LEFT_ALT_PRESSED                    } },

        // F1-F4 function keys.  F5-F12 seem to be handled more consistently among
        // various TERM=xterm terminals (gnome-terminal, konsole, xterm, mintty),
        // using a CSI-prefix with an optional extra modifier digit.  (putty is
        // also TERM=xterm, though, and has completely different modified F5-F12
        // encodings.)
        {   ESC"OP",    { VK_F1,        '\0',   0,                  } }, // xt gt kon
        {   ESC"OQ",    { VK_F2,        '\0',   0,                  } }, // xt gt kon
        {   ESC"OR",    { VK_F3,        '\0',   0,                  } }, // xt gt kon
        {   ESC"OS",    { VK_F4,        '\0',   0,                  } }, // xt gt kon

        {   "\x7F",     { VK_BACK,      '\x08', 0,                  } },
        {   ESC"\x7F",  { VK_BACK,      '\x08', LEFT_ALT_PRESSED,   } },
        {   ESC"OH",    { VK_HOME,      '\0',   0,                  } }, // gt
        {   ESC"OF",    { VK_END,       '\0',   0,                  } }, // gt
        {   ESC"[Z",    { VK_TAB,       '\t',   SHIFT_PRESSED       } },
    };

    struct CsiEncoding {
        int id;
        char letter;
        int virtualKey;
    };
    static const CsiEncoding csiEncodings[] = {
        {   0,  'A',    VK_UP       },
        {   0,  'B',    VK_DOWN     },
        {   0,  'C',    VK_RIGHT    },
        {   0,  'D',    VK_LEFT     },
        {   0,  'E',    VK_NUMPAD5  },
        {   0,  'F',    VK_END      },
        {   0,  'H',    VK_HOME     },
        {   0,  'P',    VK_F1       },  // mod+F1 for xterm and mintty
        {   0,  'Q',    VK_F2       },  // mod+F2 for xterm and mintty
        {   0,  'R',    VK_F3       },  // mod+F3 for xterm and mintty
        {   0,  'S',    VK_F4       },  // mod+F4 for xterm and mintty
        {   1,  '~',    VK_HOME     },
        {   2,  '~',    VK_INSERT   },
        {   3,  '~',    VK_DELETE   },
        {   4,  '~',    VK_END      },  // gnome-terminal keypad home/end
        {   5,  '~',    VK_PRIOR    },
        {   6,  '~',    VK_NEXT     },
        {   7,  '~',    VK_HOME     },
        {   8,  '~',    VK_END      },
        {   15, '~',    VK_F5       },
        {   17, '~',    VK_F6       },
        {   18, '~',    VK_F7       },
        {   19, '~',    VK_F8       },
        {   20, '~',    VK_F9       },
        {   21, '~',    VK_F10      },
        {   23, '~',    VK_F11      },
        {   24, '~',    VK_F12      },
    };

    const int kCsiShiftModifier = 1;
    const int kCsiAltModifier   = 2;
    const int kCsiCtrlModifier  = 4;
    char encoding[32];
    for (size_t i = 0; i < DIM(csiEncodings); ++i) {
        const CsiEncoding *e = &csiEncodings[i];
        if (e->id == 0)
            sprintf(encoding, CSI"%c", e->letter);
        else
            sprintf(encoding, CSI"%d%c", e->id, e->letter);
        InputMap::Key k = { csiEncodings[i].virtualKey, 0, 0 };
        m_inputMap.set(encoding, k);
        int id = !e->id ? 1 : e->id;
        for (int mod = 2; mod <= 8; ++mod) {
            sprintf(encoding, CSI"%d;%d%c", id, mod, e->letter);
            k.keyState = 0;
            if ((mod - 1) & kCsiShiftModifier)  k.keyState |= SHIFT_PRESSED;
            if ((mod - 1) & kCsiAltModifier)    k.keyState |= LEFT_ALT_PRESSED;
            if ((mod - 1) & kCsiCtrlModifier)   k.keyState |= LEFT_CTRL_PRESSED;
            m_inputMap.set(encoding, k);
        }
    }

    // Modified F1-F4 on gnome-terminal and konsole.
    for (int mod = 2; mod <= 8; ++mod) {
        for (int fn = 0; fn < 4; ++fn) {
            for (int fmt = 0; fmt < 1; ++fmt) {
                if (fmt == 0) {
                    // gnome-terminal
                    sprintf(encoding, ESC"O1;%d%c", mod, 'P' + fn);
                } else {
                    // konsole
                    sprintf(encoding, ESC"O%d%c", mod, 'P' + fn);
                }
                InputMap::Key k = { VK_F1 + fn, 0, 0 };
                if ((mod - 1) & kCsiShiftModifier)  k.keyState |= SHIFT_PRESSED;
                if ((mod - 1) & kCsiAltModifier)    k.keyState |= LEFT_ALT_PRESSED;
                if ((mod - 1) & kCsiCtrlModifier)   k.keyState |= LEFT_CTRL_PRESSED;
                m_inputMap.set(encoding, k);
            }
        }
    }

    // Static key encodings.
    for (size_t i = 0; i < DIM(keyDescriptorTable); ++i) {
        m_inputMap.set(keyDescriptorTable[i].encoding,
                       keyDescriptorTable[i].key);
    }
}

ConsoleInput::~ConsoleInput()
{
    delete m_console;
}

void ConsoleInput::writeInput(const std::string &input)
{
    if (input.size() == 0) {
        return;
    }

    if (isTracingEnabled()) {
        static bool debugInput = hasDebugFlag("input");
        if (debugInput) {
            std::string dumpString;
            for (size_t i = 0; i < input.size(); ++i) {
                const char ch = input[i];
                const char ctrl = decodeUnixCtrlChar(ch);
                if (ctrl != '\0') {
                    dumpString += '^';
                    dumpString += ctrl;
                } else {
                    dumpString += ch;
                }
            }
            dumpString += " (";
            for (size_t i = 0; i < input.size(); ++i) {
                if (i > 0) {
                    dumpString += ' ';
                }
                const unsigned char uch = input[i];
                char buf[32];
                sprintf(buf, "%02X", uch);
                dumpString += buf;
            }
            dumpString += ')';
            trace("input chars: %s", dumpString.c_str());
        }
    }

    m_byteQueue.append(input);
    doWrite(false);
    if (!m_byteQueue.empty() && !m_dsrSent) {
        trace("send DSR");
        m_dsrSender->sendDsr();
        m_dsrSent = true;
    }
    lastWriteTick = GetTickCount();
}

void ConsoleInput::flushIncompleteEscapeCode()
{
    if (!m_byteQueue.empty() &&
            (int)(GetTickCount() - lastWriteTick) > kIncompleteEscapeTimeoutMs) {
        doWrite(true);
        m_byteQueue.clear();
    }
}

void ConsoleInput::doWrite(bool isEof)
{
    const char *data = m_byteQueue.c_str();
    std::vector<INPUT_RECORD> records;
    size_t idx = 0;
    while (idx < m_byteQueue.size()) {
        int charSize = scanKeyPress(records, &data[idx], m_byteQueue.size() - idx, isEof);
        if (charSize == -1)
            break;
        idx += charSize;
    }
    m_byteQueue.erase(0, idx);
    m_console->writeInput(records.data(), records.size());
}

int ConsoleInput::scanKeyPress(std::vector<INPUT_RECORD> &records,
                               const char *input,
                               int inputSize,
                               bool isEof)
{
    //trace("scanKeyPress: %d bytes", inputSize);

    // Ctrl-C.
    if (input[0] == '\x03' && m_console->processedInputMode()) {
        trace("Ctrl-C");
        BOOL ret = GenerateConsoleCtrlEvent(CTRL_C_EVENT, 0);
        trace("GenerateConsoleCtrlEvent: %d", ret);
        return 1;
    }

    // Attempt to match the Device Status Report (DSR) reply.
    int dsrLen = matchDsr(input, inputSize);
    if (dsrLen > 0) {
        trace("Received a DSR reply");
        m_dsrSent = false;
        return dsrLen;
    } else if (!isEof && dsrLen == -1) {
        // Incomplete DSR match.
        trace("Incomplete DSR match");
        return -1;
    }

    // Recognize Alt-<character>.
    InputMap *const escapeSequences = m_inputMap.getChild('\x1B');
    if (input[0] == '\x1B' && inputSize >= 2 && input[1] != '\x1B' &&
            (escapeSequences == NULL ||
                escapeSequences->getChild(input[1]) == NULL)) {
        int len = utf8CharLength(input[1]);
        if (1 + len > inputSize) {
            // Incomplete character.
            trace("Incomplete Alt-char match");
            return -1;
        }
        appendUtf8Char(records, &input[1], len, LEFT_ALT_PRESSED);
        return 1 + len;
    }

    // Recognize an ESC-encoded keypress.
    bool incomplete;
    int matchLen;
    const InputMap::Key *match =
        lookupKey(input, inputSize, isEof, incomplete, matchLen);
    if (incomplete) {
        // Incomplete match -- need more characters (or wait for a
        // timeout to signify flushed input).
        trace("Incomplete ESC-keypress match");
        return -1;
    } else if (match != NULL) {
        appendKeyPress(records,
                       match->virtualKey,
                       match->unicodeChar,
                       match->keyState);
        return matchLen;
    }

    // A UTF-8 character.
    int len = utf8CharLength(input[0]);
    if (len > inputSize) {
        // Incomplete character.
        trace("Incomplete UTF-8 character");
        return -1;
    }
    appendUtf8Char(records, &input[0], len, 0);
    return len;
}

void ConsoleInput::appendUtf8Char(std::vector<INPUT_RECORD> &records,
                                  const char *charBuffer,
                                  const int charLen,
                                  const int keyState)
{
    WCHAR wideInput[2];
    int wideLen = MultiByteToWideChar(CP_UTF8,
                                      0,
                                      charBuffer,
                                      charLen,
                                      wideInput,
                                      sizeof(wideInput) / sizeof(wideInput[0]));
    for (int i = 0; i < wideLen; ++i) {
        short charScan = VkKeyScan(wideInput[i]);
        int virtualKey = 0;
        int charKeyState = keyState;
        if (charScan != -1) {
            virtualKey = charScan & 0xFF;
            if (charScan & 0x100)
                charKeyState |= SHIFT_PRESSED;
            else if (charScan & 0x200)
                charKeyState |= LEFT_CTRL_PRESSED;
            else if (charScan & 0x400)
                charKeyState |= LEFT_ALT_PRESSED;
        }
        appendKeyPress(records, virtualKey, wideInput[i], charKeyState);
    }
}

static const char *getVirtualKeyString(int virtualKey)
{
    switch (virtualKey) {
#define WINPTY_GVKS_KEY(x) case VK_##x: return #x;
        WINPTY_GVKS_KEY(RBUTTON)    WINPTY_GVKS_KEY(F9)
        WINPTY_GVKS_KEY(CANCEL)     WINPTY_GVKS_KEY(F10)
        WINPTY_GVKS_KEY(MBUTTON)    WINPTY_GVKS_KEY(F11)
        WINPTY_GVKS_KEY(XBUTTON1)   WINPTY_GVKS_KEY(F12)
        WINPTY_GVKS_KEY(XBUTTON2)   WINPTY_GVKS_KEY(F13)
        WINPTY_GVKS_KEY(BACK)       WINPTY_GVKS_KEY(F14)
        WINPTY_GVKS_KEY(TAB)        WINPTY_GVKS_KEY(F15)
        WINPTY_GVKS_KEY(CLEAR)      WINPTY_GVKS_KEY(F16)
        WINPTY_GVKS_KEY(RETURN)     WINPTY_GVKS_KEY(F17)
        WINPTY_GVKS_KEY(SHIFT)      WINPTY_GVKS_KEY(F18)
        WINPTY_GVKS_KEY(CONTROL)    WINPTY_GVKS_KEY(F19)
        WINPTY_GVKS_KEY(MENU)       WINPTY_GVKS_KEY(F20)
        WINPTY_GVKS_KEY(PAUSE)      WINPTY_GVKS_KEY(F21)
        WINPTY_GVKS_KEY(CAPITAL)    WINPTY_GVKS_KEY(F22)
        WINPTY_GVKS_KEY(HANGUL)     WINPTY_GVKS_KEY(F23)
        WINPTY_GVKS_KEY(JUNJA)      WINPTY_GVKS_KEY(F24)
        WINPTY_GVKS_KEY(FINAL)      WINPTY_GVKS_KEY(NUMLOCK)
        WINPTY_GVKS_KEY(KANJI)      WINPTY_GVKS_KEY(SCROLL)
        WINPTY_GVKS_KEY(ESCAPE)     WINPTY_GVKS_KEY(LSHIFT)
        WINPTY_GVKS_KEY(CONVERT)    WINPTY_GVKS_KEY(RSHIFT)
        WINPTY_GVKS_KEY(NONCONVERT) WINPTY_GVKS_KEY(LCONTROL)
        WINPTY_GVKS_KEY(ACCEPT)     WINPTY_GVKS_KEY(RCONTROL)
        WINPTY_GVKS_KEY(MODECHANGE) WINPTY_GVKS_KEY(LMENU)
        WINPTY_GVKS_KEY(SPACE)      WINPTY_GVKS_KEY(RMENU)
        WINPTY_GVKS_KEY(PRIOR)      WINPTY_GVKS_KEY(BROWSER_BACK)
        WINPTY_GVKS_KEY(NEXT)       WINPTY_GVKS_KEY(BROWSER_FORWARD)
        WINPTY_GVKS_KEY(END)        WINPTY_GVKS_KEY(BROWSER_REFRESH)
        WINPTY_GVKS_KEY(HOME)       WINPTY_GVKS_KEY(BROWSER_STOP)
        WINPTY_GVKS_KEY(LEFT)       WINPTY_GVKS_KEY(BROWSER_SEARCH)
        WINPTY_GVKS_KEY(UP)         WINPTY_GVKS_KEY(BROWSER_FAVORITES)
        WINPTY_GVKS_KEY(RIGHT)      WINPTY_GVKS_KEY(BROWSER_HOME)
        WINPTY_GVKS_KEY(DOWN)       WINPTY_GVKS_KEY(VOLUME_MUTE)
        WINPTY_GVKS_KEY(SELECT)     WINPTY_GVKS_KEY(VOLUME_DOWN)
        WINPTY_GVKS_KEY(PRINT)      WINPTY_GVKS_KEY(VOLUME_UP)
        WINPTY_GVKS_KEY(EXECUTE)    WINPTY_GVKS_KEY(MEDIA_NEXT_TRACK)
        WINPTY_GVKS_KEY(SNAPSHOT)   WINPTY_GVKS_KEY(MEDIA_PREV_TRACK)
        WINPTY_GVKS_KEY(INSERT)     WINPTY_GVKS_KEY(MEDIA_STOP)
        WINPTY_GVKS_KEY(DELETE)     WINPTY_GVKS_KEY(MEDIA_PLAY_PAUSE)
        WINPTY_GVKS_KEY(HELP)       WINPTY_GVKS_KEY(LAUNCH_MAIL)
        WINPTY_GVKS_KEY(LWIN)       WINPTY_GVKS_KEY(LAUNCH_MEDIA_SELECT)
        WINPTY_GVKS_KEY(RWIN)       WINPTY_GVKS_KEY(LAUNCH_APP1)
        WINPTY_GVKS_KEY(APPS)       WINPTY_GVKS_KEY(LAUNCH_APP2)
        WINPTY_GVKS_KEY(SLEEP)      WINPTY_GVKS_KEY(OEM_1)
        WINPTY_GVKS_KEY(NUMPAD0)    WINPTY_GVKS_KEY(OEM_PLUS)
        WINPTY_GVKS_KEY(NUMPAD1)    WINPTY_GVKS_KEY(OEM_COMMA)
        WINPTY_GVKS_KEY(NUMPAD2)    WINPTY_GVKS_KEY(OEM_MINUS)
        WINPTY_GVKS_KEY(NUMPAD3)    WINPTY_GVKS_KEY(OEM_PERIOD)
        WINPTY_GVKS_KEY(NUMPAD4)    WINPTY_GVKS_KEY(OEM_2)
        WINPTY_GVKS_KEY(NUMPAD5)    WINPTY_GVKS_KEY(OEM_3)
        WINPTY_GVKS_KEY(NUMPAD6)    WINPTY_GVKS_KEY(OEM_4)
        WINPTY_GVKS_KEY(NUMPAD7)    WINPTY_GVKS_KEY(OEM_5)
        WINPTY_GVKS_KEY(NUMPAD8)    WINPTY_GVKS_KEY(OEM_6)
        WINPTY_GVKS_KEY(NUMPAD9)    WINPTY_GVKS_KEY(OEM_7)
        WINPTY_GVKS_KEY(MULTIPLY)   WINPTY_GVKS_KEY(OEM_8)
        WINPTY_GVKS_KEY(ADD)        WINPTY_GVKS_KEY(OEM_102)
        WINPTY_GVKS_KEY(SEPARATOR)  WINPTY_GVKS_KEY(PROCESSKEY)
        WINPTY_GVKS_KEY(SUBTRACT)   WINPTY_GVKS_KEY(PACKET)
        WINPTY_GVKS_KEY(DECIMAL)    WINPTY_GVKS_KEY(ATTN)
        WINPTY_GVKS_KEY(DIVIDE)     WINPTY_GVKS_KEY(CRSEL)
        WINPTY_GVKS_KEY(F1)         WINPTY_GVKS_KEY(EXSEL)
        WINPTY_GVKS_KEY(F2)         WINPTY_GVKS_KEY(EREOF)
        WINPTY_GVKS_KEY(F3)         WINPTY_GVKS_KEY(PLAY)
        WINPTY_GVKS_KEY(F4)         WINPTY_GVKS_KEY(ZOOM)
        WINPTY_GVKS_KEY(F5)         WINPTY_GVKS_KEY(NONAME)
        WINPTY_GVKS_KEY(F6)         WINPTY_GVKS_KEY(PA1)
        WINPTY_GVKS_KEY(F7)         WINPTY_GVKS_KEY(OEM_CLEAR)
        WINPTY_GVKS_KEY(F8)
#undef WINPTY_GVKS_KEY
        default:                        return NULL;
    }
}

void ConsoleInput::appendKeyPress(std::vector<INPUT_RECORD> &records,
                                  int virtualKey,
                                  int unicodeChar,
                                  int keyState)
{
    const bool ctrl = keyState & LEFT_CTRL_PRESSED;
    const bool alt = keyState & LEFT_ALT_PRESSED;
    const bool shift = keyState & SHIFT_PRESSED;

    if (isTracingEnabled()) {
        static bool debugInput = hasDebugFlag("input");
        if (debugInput) {
            std::string dumpString;
            if (shift)  {    dumpString += "Shift-";    }
            if (ctrl)   {    dumpString += "Ctrl-";     }
            if (alt)    {    dumpString += "Alt-";      }
            char buf[256];
            const char *vkString = getVirtualKeyString(virtualKey);
            if (vkString != NULL) {
                dumpString += vkString;
            } else if ((virtualKey >= 'A' && virtualKey <= 'Z') ||
                       (virtualKey >= '0' && virtualKey <= '9')) {
                dumpString += static_cast<char>(virtualKey);
            } else {
                sprintf(buf, "0x%x", virtualKey);
                dumpString += buf;
            }
            if (unicodeChar >= 32 && unicodeChar <= 126) {
                sprintf(buf, " ch='%c'", unicodeChar);
            } else {
                sprintf(buf, " ch=%#x", unicodeChar);
            }
            dumpString += buf;
            trace("keypress: %s", dumpString.c_str());
        }
    }

    int stepKeyState = 0;
    if (ctrl) {
        stepKeyState |= LEFT_CTRL_PRESSED;
        appendInputRecord(records, TRUE, VK_CONTROL, 0, stepKeyState);
    }
    if (alt) {
        stepKeyState |= LEFT_ALT_PRESSED;
        appendInputRecord(records, TRUE, VK_MENU, 0, stepKeyState);
    }
    if (shift) {
        stepKeyState |= SHIFT_PRESSED;
        appendInputRecord(records, TRUE, VK_SHIFT, 0, stepKeyState);
    }
    if (ctrl && alt) {
        // This behavior seems arbitrary, but it's what I see in the Windows 7
        // console.
        unicodeChar = 0;
    }
    appendInputRecord(records, TRUE, virtualKey, unicodeChar, stepKeyState);
    if (alt) {
        // This behavior seems arbitrary, but it's what I see in the Windows 7
        // console.
        unicodeChar = 0;
    }
    appendInputRecord(records, FALSE, virtualKey, unicodeChar, stepKeyState);
    if (shift) {
        stepKeyState &= ~SHIFT_PRESSED;
        appendInputRecord(records, FALSE, VK_SHIFT, 0, stepKeyState);
    }
    if (alt) {
        stepKeyState &= ~LEFT_ALT_PRESSED;
        appendInputRecord(records, FALSE, VK_MENU, 0, stepKeyState);
    }
    if (ctrl) {
        stepKeyState &= ~LEFT_CTRL_PRESSED;
        appendInputRecord(records, FALSE, VK_CONTROL, 0, stepKeyState);
    }
}

void ConsoleInput::appendInputRecord(std::vector<INPUT_RECORD> &records,
                                     BOOL keyDown,
                                     int virtualKey,
                                     int unicodeChar,
                                     int keyState)
{
    INPUT_RECORD ir;
    memset(&ir, 0, sizeof(ir));
    ir.EventType = KEY_EVENT;
    ir.Event.KeyEvent.bKeyDown = keyDown;
    ir.Event.KeyEvent.wRepeatCount = 1;
    ir.Event.KeyEvent.wVirtualKeyCode = virtualKey;
    ir.Event.KeyEvent.wVirtualScanCode =
            MapVirtualKey(virtualKey, MAPVK_VK_TO_VSC);
    ir.Event.KeyEvent.uChar.UnicodeChar = unicodeChar;
    ir.Event.KeyEvent.dwControlKeyState = keyState;
    records.push_back(ir);
}

// Return the byte size of a UTF-8 character using the value of the first
// byte.
int ConsoleInput::utf8CharLength(char firstByte)
{
    // This code would probably be faster if it used __builtin_clz.
    if ((firstByte & 0x80) == 0) {
        return 1;
    } else if ((firstByte & 0xE0) == 0xC0) {
        return 2;
    } else if ((firstByte & 0xF0) == 0xE0) {
        return 3;
    } else if ((firstByte & 0xF8) == 0xF0) {
        return 4;
    } else if ((firstByte & 0xFC) == 0xF8) {
        return 5;
    } else if ((firstByte & 0xFE) == 0xFC) {
        return 6;
    } else {
        // Malformed UTF-8.
        return 1;
    }
}

// Find the longest matching key and node.
const InputMap::Key *
ConsoleInput::lookupKey(const char *input, int inputSize, bool isEof,
                        bool &incompleteOut, int &matchLenOut)
{
    incompleteOut = false;
    matchLenOut = 0;

    InputMap *node = &m_inputMap;
    const InputMap::Key *longestMatch = NULL;
    int longestMatchLen = 0;

    for (int i = 0; i < inputSize; ++i) {
        unsigned char ch = input[i];
        node = node->getChild(ch);
        //trace("ch: %d --> node:%p", ch, node);
        if (node == NULL) {
            matchLenOut = longestMatchLen;
            return longestMatch;
        } else if (node->getKey() != NULL) {
            longestMatchLen = i + 1;
            longestMatch = node->getKey();
        }
    }
    if (isEof) {
        matchLenOut = longestMatchLen;
        return longestMatch;
    } else if (node->hasChildren()) {
        incompleteOut = true;
        return NULL;
    } else {
        matchLenOut = longestMatchLen;
        return longestMatch;
    }
}

// Match the Device Status Report console input:  ESC [ nn ; mm R
// Returns:
// 0   no match
// >0  match, returns length of match
// -1  incomplete match
int ConsoleInput::matchDsr(const char *input, int inputSize)
{
    const char *pch = input;
    const char *stop = input + inputSize;

    if (pch == stop) { return -1; }

#define CHECK(cond) \
        do { \
            if (!(cond)) { return 0; } \
        } while(0)

#define ADVANCE() \
        do { \
            pch++; \
            if (pch == stop) { return -1; } \
        } while(0)

    CHECK(*pch == '\x1B');  ADVANCE();
    CHECK(*pch == '[');     ADVANCE();
    CHECK(isdigit(*pch));   ADVANCE();
    while (isdigit(*pch)) {
        ADVANCE();
    }
    CHECK(*pch == ';');     ADVANCE();
    CHECK(isdigit(*pch));   ADVANCE();
    while (isdigit(*pch)) {
        ADVANCE();
    }
    CHECK(*pch == 'R');
    return pch - input + 1;
#undef CHECK
#undef ADVANCE
}
