#include "ConsoleInput.h"
#include "Win32Console.h"
#include "DsrSender.h"
#include "../Shared/DebugClient.h"
#include <string.h>

#ifndef MAPVK_VK_TO_VSC
#define MAPVK_VK_TO_VSC 0
#endif

const int kIncompleteEscapeTimeoutMs = 1000;

#define ESC "\x1B"
#define CSI ESC"["

ConsoleInput::KeyDescriptor ConsoleInput::keyDescriptorTable[] = {
    // Ctrl-<letter/digit> seems to be handled OK by the default code path.
    // TODO: Alt-ESC is encoded as ESC ESC.  Can it be handled?

    {   ESC,            VK_ESCAPE,  '\x1B', 0,              },

    // Alt-<letter/digit>
    {   ESC"O",         'O',        0,  LEFT_ALT_PRESSED    },
    {   ESC"[",         '[',        0,  LEFT_ALT_PRESSED    },

    // Function keys
    {   ESC"OP",        VK_F1,      0,  0,                  }, // xt gt kon
    {   ESC"OQ",        VK_F2,      0,  0,                  }, // xt gt kon
    {   ESC"OR",        VK_F3,      0,  0,                  }, // xt gt kon
    {   ESC"OS",        VK_F4,      0,  0,                  }, // xt gt kon
    {   CSI"11~",       VK_F1,      0,  0,                  }, // rxvt
    {   CSI"12~",       VK_F2,      0,  0,                  }, // rxvt
    {   CSI"13~",       VK_F3,      0,  0,                  }, // rxvt
    {   CSI"14~",       VK_F4,      0,  0,                  }, // rxvt
    {   CSI"15~",       VK_F5,      0,  0,                  }, // xt gt kon rxvt
    {   CSI"17~",       VK_F6,      0,  0,                  }, // xt gt kon rxvt
    {   CSI"18~",       VK_F7,      0,  0,                  }, // xt gt kon rxvt
    {   CSI"19~",       VK_F8,      0,  0,                  }, // xt gt kon rxvt
    {   CSI"20~",       VK_F9,      0,  0,                  }, // xt gt kon rxvt
    {   CSI"21~",       VK_F10,     0,  0,                  }, // xt gt kon rxvt
    {   CSI"23~",       VK_F11,     0,  0,                  }, // xt gt kon rxvt
    {   CSI"24~",       VK_F12,     0,  0,                  }, // xt gt kon rxvt

    {   "\x7F",         VK_BACK,    '\x08', 0,              },
    {   ESC"\x7F",      VK_BACK,    '\x08', LEFT_ALT_PRESSED,   },

    // arrow keys
    {   CSI"A",         VK_UP,      0,  0,                  }, // xt gt kon rxvt
    {   CSI"B",         VK_DOWN,    0,  0,                  }, // xt gt kon rxvt
    {   CSI"C",         VK_RIGHT,   0,  0,                  }, // xt gt kon rxvt
    {   CSI"D",         VK_LEFT,    0,  0,                  }, // xt gt kon rxvt
    // ctrl-<arrow>
    {   CSI"1;5A",      VK_UP,      0,  LEFT_CTRL_PRESSED   }, // xt gt kon
    {   CSI"1;5B",      VK_DOWN,    0,  LEFT_CTRL_PRESSED   }, // xt gt kon
    {   CSI"1;5C",      VK_RIGHT,   0,  LEFT_CTRL_PRESSED   }, // xt gt kon
    {   CSI"1;5D",      VK_LEFT,    0,  LEFT_CTRL_PRESSED   }, // xt gt kon
    {   ESC"Oa",        VK_UP,      0,  LEFT_CTRL_PRESSED   }, // rxvt
    {   ESC"Ob",        VK_DOWN,    0,  LEFT_CTRL_PRESSED   }, // rxvt
    {   ESC"Oc",        VK_RIGHT,   0,  LEFT_CTRL_PRESSED   }, // rxvt
    {   ESC"Od",        VK_LEFT,    0,  LEFT_CTRL_PRESSED   }, // rxvt
    // alt-<arrow>
    {   CSI"1;3A",      VK_UP,      0,  LEFT_ALT_PRESSED    }, // xt gt kon
    {   CSI"1;3B",      VK_DOWN,    0,  LEFT_ALT_PRESSED    }, // xt gt kon
    {   CSI"1;3C",      VK_RIGHT,   0,  LEFT_ALT_PRESSED    }, // xt gt kon
    {   CSI"1;3D",      VK_LEFT,    0,  LEFT_ALT_PRESSED    }, // xt gt kon
    {   ESC CSI"A",     VK_UP,      0,  LEFT_ALT_PRESSED    }, // rxvt
    {   ESC CSI"B",     VK_DOWN,    0,  LEFT_ALT_PRESSED    }, // rxvt
    {   ESC CSI"C",     VK_RIGHT,   0,  LEFT_ALT_PRESSED    }, // rxvt
    {   ESC CSI"D",     VK_LEFT,    0,  LEFT_ALT_PRESSED    }, // rxvt

    // insert,delete,home,end,pgup,pgdn
    {   CSI"2~",        VK_INSERT,  0,  0,                  }, // xt gt kon rxvt
    {   CSI"3~",        VK_DELETE,  0,  0,                  }, // xt gt kon rxvt
    {   CSI"5~",        VK_PRIOR,   0,  0,                  }, // xt gt kon rxvt
    {   CSI"6~",        VK_NEXT,    0,  0,                  }, // xt gt kon rxvt
    {   CSI"H",         VK_HOME,    0,  0,                  }, // xt kon
    {   CSI"F",         VK_END,     0,  0,                  }, // xt kon
    {   ESC"OH",        VK_HOME,    0,  0,                  }, // gt
    {   ESC"OF",        VK_END,     0,  0,                  }, // gt
    {   CSI"7^",        VK_HOME,    0,  0,                  }, // rxvt
    {   CSI"8^",        VK_END,     0,  0,                  }, // rxvt
    // ctrl-<key>
    {   CSI"2;5~",      VK_INSERT,  0,  LEFT_CTRL_PRESSED   }, // xt
    {   CSI"3;5~",      VK_DELETE,  0,  LEFT_CTRL_PRESSED   }, // xt gt kon
    {   CSI"1;5H",      VK_HOME,    0,  LEFT_CTRL_PRESSED   }, // xt kon
    {   CSI"1;5F",      VK_END,     0,  LEFT_CTRL_PRESSED   }, // xt kon
    {   CSI"5;5~",      VK_PRIOR,   0,  LEFT_CTRL_PRESSED   }, // xt gt
    {   CSI"6;5~",      VK_NEXT,    0,  LEFT_CTRL_PRESSED   }, // xt gt
    {   CSI"2^",        VK_INSERT,  0,  LEFT_CTRL_PRESSED   }, // rxvt
    {   CSI"3^",        VK_DELETE,  0,  LEFT_CTRL_PRESSED   }, // rxvt
    {   CSI"7^",        VK_HOME,    0,  LEFT_CTRL_PRESSED   }, // rxvt
    {   CSI"8^",        VK_END,     0,  LEFT_CTRL_PRESSED   }, // rxvt
    {   CSI"5^",        VK_PRIOR,   0,  LEFT_CTRL_PRESSED   }, // rxvt
    {   CSI"6^",        VK_NEXT,    0,  LEFT_CTRL_PRESSED   }, // rxvt
    // alt-<key>
    {   CSI"2;3~",      VK_INSERT,  0,  LEFT_ALT_PRESSED    }, // xt gt
    {   CSI"3;3~",      VK_DELETE,  0,  LEFT_ALT_PRESSED    }, // xt gt
    {   CSI"1;3H",      VK_HOME,    0,  LEFT_ALT_PRESSED    }, // xt
    {   CSI"1;3F",      VK_END,     0,  LEFT_ALT_PRESSED    }, // xt
    {   CSI"5;3~",      VK_PRIOR,   0,  LEFT_ALT_PRESSED    }, // xt gt
    {   CSI"6;3~",      VK_NEXT,    0,  LEFT_ALT_PRESSED    }, // xt gt
    {   ESC CSI"2~",    VK_INSERT,  0,  LEFT_ALT_PRESSED    }, // rxvt
    {   ESC CSI"3~",    VK_DELETE,  0,  LEFT_ALT_PRESSED    }, // rxvt
    {   ESC CSI"7~",    VK_HOME,    0,  LEFT_ALT_PRESSED    }, // rxvt
    {   ESC CSI"8~",    VK_END,     0,  LEFT_ALT_PRESSED    }, // rxvt
    {   ESC CSI"5~",    VK_PRIOR,   0,  LEFT_ALT_PRESSED    }, // rxvt
    {   ESC CSI"6~",    VK_NEXT,    0,  LEFT_ALT_PRESSED    }, // rxvt
};

ConsoleInput::ConsoleInput(Win32Console *console, DsrSender *dsrSender) :
    m_console(console),
    m_dsrSender(dsrSender),
    m_dsrSent(false),
    lastWriteTick(0)
{
    for (size_t i = 0; i < sizeof(keyDescriptorTable) / sizeof(keyDescriptorTable[0]); ++i) {
        KeyDescriptor *k = &keyDescriptorTable[i];
        k->encodingLen = strlen(k->encoding);
        m_lookup.set(k->encoding, k);
    }
}

void ConsoleInput::writeInput(const std::string &input)
{
    Trace("writeInput: %d bytes", input.size());
    if (input.size() == 0)
        return;
    m_byteQueue.append(input);
    doWrite(false);
    if (!m_byteQueue.empty() && !m_dsrSent) {
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

ConsoleInput::KeyLookup::KeyLookup() : match(NULL), children(NULL)
{
}

ConsoleInput::KeyLookup::~KeyLookup()
{
    if (children != NULL) {
        for (int i = 0; i < 256; ++i)
            delete (*children)[i];
    }
    delete [] children;
}

void ConsoleInput::KeyLookup::set(const char *encoding,
                                  const KeyDescriptor *descriptor)
{
    unsigned char ch = encoding[0];
    if (ch == '\0') {
        match = descriptor;
        return;
    }
    if (children == NULL) {
        children = (KeyLookup*(*)[256])new KeyLookup*[256];
        memset(children, 0, sizeof(KeyLookup*) * 256);
    }
    if ((*children)[ch] == NULL) {
        (*children)[ch] = new KeyLookup;
    }
    (*children)[ch]->set(encoding + 1, descriptor);
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
    // Ctrl-C.
    if (input[0] == '\x03' && m_console->processedInputMode()) {
        Trace("Ctrl-C");
        BOOL ret = GenerateConsoleCtrlEvent(CTRL_C_EVENT, 0);
        Trace("GenerateConsoleCtrlEvent: %d", ret);
        return 1;
    }

    // Attempt to match the Device Status Report (DSR) reply.
    int dsrLen = matchDsr(input);
    if (dsrLen > 0) {
        Trace("Received a DSR reply");
        m_dsrSent = false;
        return dsrLen;
    } else if (!isEof && dsrLen == -1) {
        // Incomplete DSR match.
        Trace("Incomplete DSR match");
        return -1;
    }

    // Recognize Alt-<character>.
    if (input[0] == '\x1B' && input[1] != '\0' &&
            m_lookup.getChild('\x1B')->getChild(input[1]) == NULL) {
        int len = utf8CharLength(input[1]);
        if (1 + len > inputSize) {
            // Incomplete character.
            Trace("Incomplete Alt-char match");
            return -1;
        }
        appendUtf8Char(records, &input[1], len, LEFT_ALT_PRESSED);
        return 1 + len;
    }

    // Recognize an ESC-encoded keypress.
    bool incomplete;
    const KeyDescriptor *match = lookupKey(input, isEof, &incomplete);
    if (incomplete) {
        // Incomplete match -- need more characters (or wait for a
        // timeout to signify flushed input).
        Trace("Incomplete ESC-keypress match");
        return -1;
    } else if (match != NULL) {
        appendKeyPress(records,
                       match->virtualKey,
                       match->unicodeChar,
                       match->keyState);
        return match->encodingLen;
    }

    // A UTF-8 character.
    int len = utf8CharLength(input[0]);
    if (len > inputSize) {
        // Incomplete character.
        Trace("Incomplete UTF-8 character");
        return -1;
    }
    appendUtf8Char(records, &input[0], len, 0);
    return len;
}

void ConsoleInput::appendUtf8Char(std::vector<INPUT_RECORD> &records,
                                  const char *charBuffer,
                                  int charLen,
                                  int keyState)
{
    WCHAR wideInput[2];
    int wideLen = MultiByteToWideChar(CP_UTF8,
                                      0,
                                      charBuffer,
                                      charLen,
                                      wideInput,
                                      sizeof(wideInput) / sizeof(wideInput[0]));
    // TODO: Characters outside the BMP.
    if (wideLen != 1)
        return;

    short charScan = VkKeyScan(wideInput[0]);
    int virtualKey = 0;
    if (charScan != -1) {
        virtualKey = charScan & 0xFF;
        if (charScan & 0x100)
            keyState |= SHIFT_PRESSED;
        else if (charScan & 0x200)
            keyState |= LEFT_CTRL_PRESSED;
        else if (charScan & 0x400)
            keyState |= LEFT_ALT_PRESSED;
    }
    appendKeyPress(records, virtualKey, wideInput[0], keyState);
}

void ConsoleInput::appendKeyPress(std::vector<INPUT_RECORD> &records,
                                  int virtualKey,
                                  int unicodeChar,
                                  int keyState)
{
    bool ctrl = keyState & LEFT_CTRL_PRESSED;
    bool alt = keyState & LEFT_ALT_PRESSED;
    bool shift = keyState & SHIFT_PRESSED;
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
const ConsoleInput::KeyDescriptor *
ConsoleInput::lookupKey(const char *encoding, bool isEof, bool *incomplete)
{
    Trace("lookupKey");
    for (int i = 0; encoding[i] != '\0'; ++i)
        Trace("%d", encoding[i]);

    *incomplete = false;
    KeyLookup *node = &m_lookup;
    const KeyDescriptor *longestMatch = NULL;
    for (int i = 0; encoding[i] != '\0'; ++i) {
        unsigned char ch = encoding[i];
        node = node->getChild(ch);
        Trace("ch: %d --> node:%p", ch, node);
        if (node == NULL) {
            return longestMatch;
        } else if (node->getMatch() != NULL) {
            longestMatch = node->getMatch();
        }
    }
    if (isEof) {
        return longestMatch;
    } else if (node->hasChildren()) {
        *incomplete = true;
        return NULL;
    } else {
        return longestMatch;
    }
}

// Match the Device Status Report console input:  ESC [ nn ; mm R
// Returns:
// 0   no match
// >0  match, returns length of match
// -1  incomplete match
int ConsoleInput::matchDsr(const char *encoding)
{
    const char *pch = encoding;
#define CHECK(cond) \
        do { \
            if (cond) { pch++; } \
            else if (*pch == '\0') { return -1; } \
            else { return 0; } \
        } while(0)
    CHECK(*pch == '\x1B');
    CHECK(*pch == '[');
    CHECK(isdigit(*pch));
    while (isdigit(*pch))
        pch++;
    CHECK(*pch == ';');
    CHECK(isdigit(*pch));
    while (isdigit(*pch))
        pch++;
    CHECK(*pch == 'R');
    return pch - encoding;
#undef CHECK
}
