#include "ConsoleInput.h"
#include "Win32Console.h"
#include "../Shared/DebugClient.h"
#include <string.h>

#ifndef MAPVK_VK_TO_VSC
#define MAPVK_VK_TO_VSC 0
#endif
#ifndef MAPVK_VK_TO_CHAR
#define MAPVK_VK_TO_CHAR 2
#endif

const int kIncompleteEscapeTimeoutMs = 1000;

#define ESC "\x1B"
#define CSI ESC"["

ConsoleInput::KeyDescriptor ConsoleInput::keyDescriptorTable[] = {
    // Ctrl-<letter/digit> seems to be handled OK by the default code path.

    // Alt-<letter/digit>
    {   ESC"a",         'A',        LEFT_ALT_PRESSED    },
    {   ESC"b",         'B',        LEFT_ALT_PRESSED    },
    {   ESC"c",         'C',        LEFT_ALT_PRESSED    },
    {   ESC"d",         'D',        LEFT_ALT_PRESSED    },
    {   ESC"e",         'E',        LEFT_ALT_PRESSED    },
    {   ESC"f",         'F',        LEFT_ALT_PRESSED    },
    {   ESC"g",         'G',        LEFT_ALT_PRESSED    },
    {   ESC"h",         'H',        LEFT_ALT_PRESSED    },
    {   ESC"i",         'I',        LEFT_ALT_PRESSED    },
    {   ESC"j",         'J',        LEFT_ALT_PRESSED    },
    {   ESC"k",         'K',        LEFT_ALT_PRESSED    },
    {   ESC"l",         'L',        LEFT_ALT_PRESSED    },
    {   ESC"m",         'M',        LEFT_ALT_PRESSED    },
    {   ESC"n",         'N',        LEFT_ALT_PRESSED    },
    {   ESC"o",         'O',        LEFT_ALT_PRESSED    },
    {   ESC"p",         'P',        LEFT_ALT_PRESSED    },
    {   ESC"q",         'Q',        LEFT_ALT_PRESSED    },
    {   ESC"r",         'R',        LEFT_ALT_PRESSED    },
    {   ESC"s",         'S',        LEFT_ALT_PRESSED    },
    {   ESC"t",         'T',        LEFT_ALT_PRESSED    },
    {   ESC"u",         'U',        LEFT_ALT_PRESSED    },
    {   ESC"v",         'V',        LEFT_ALT_PRESSED    },
    {   ESC"w",         'W',        LEFT_ALT_PRESSED    },
    {   ESC"x",         'X',        LEFT_ALT_PRESSED    },
    {   ESC"y",         'Y',        LEFT_ALT_PRESSED    },
    {   ESC"z",         'Z',        LEFT_ALT_PRESSED    },
    {   ESC"A",         'A',        LEFT_ALT_PRESSED    },
    {   ESC"B",         'B',        LEFT_ALT_PRESSED    },
    {   ESC"C",         'C',        LEFT_ALT_PRESSED    },
    {   ESC"D",         'D',        LEFT_ALT_PRESSED    },
    {   ESC"E",         'E',        LEFT_ALT_PRESSED    },
    {   ESC"F",         'F',        LEFT_ALT_PRESSED    },
    {   ESC"G",         'G',        LEFT_ALT_PRESSED    },
    {   ESC"H",         'H',        LEFT_ALT_PRESSED    },
    {   ESC"I",         'I',        LEFT_ALT_PRESSED    },
    {   ESC"J",         'J',        LEFT_ALT_PRESSED    },
    {   ESC"K",         'K',        LEFT_ALT_PRESSED    },
    {   ESC"L",         'L',        LEFT_ALT_PRESSED    },
    {   ESC"M",         'M',        LEFT_ALT_PRESSED    },
    {   ESC"N",         'N',        LEFT_ALT_PRESSED    },
    {   ESC"O",         'O',        LEFT_ALT_PRESSED    },
    {   ESC"P",         'P',        LEFT_ALT_PRESSED    },
    {   ESC"Q",         'Q',        LEFT_ALT_PRESSED    },
    {   ESC"R",         'R',        LEFT_ALT_PRESSED    },
    {   ESC"S",         'S',        LEFT_ALT_PRESSED    },
    {   ESC"T",         'T',        LEFT_ALT_PRESSED    },
    {   ESC"U",         'U',        LEFT_ALT_PRESSED    },
    {   ESC"V",         'V',        LEFT_ALT_PRESSED    },
    {   ESC"W",         'W',        LEFT_ALT_PRESSED    },
    {   ESC"X",         'X',        LEFT_ALT_PRESSED    },
    {   ESC"Y",         'Y',        LEFT_ALT_PRESSED    },
    {   ESC"Z",         'Z',        LEFT_ALT_PRESSED    },

    {   ESC,            VK_ESCAPE,  0,                  },
    {   ESC"[",         '[',        LEFT_ALT_PRESSED    },

    // Function keys
    {   ESC"OP",        VK_F1,      0,                  }, // xt gt kon
    {   ESC"OQ",        VK_F2,      0,                  }, // xt gt kon
    {   ESC"OR",        VK_F3,      0,                  }, // xt gt kon
    {   ESC"OS",        VK_F4,      0,                  }, // xt gt kon
    {   CSI"11~",       VK_F1,      0,                  }, // rxvt
    {   CSI"12~",       VK_F2,      0,                  }, // rxvt
    {   CSI"13~",       VK_F3,      0,                  }, // rxvt
    {   CSI"14~",       VK_F4,      0,                  }, // rxvt
    {   CSI"15~",       VK_F5,      0,                  }, // xt gt kon rxvt
    {   CSI"17~",       VK_F6,      0,                  }, // xt gt kon rxvt
    {   CSI"18~",       VK_F7,      0,                  }, // xt gt kon rxvt
    {   CSI"19~",       VK_F8,      0,                  }, // xt gt kon rxvt
    {   CSI"20~",       VK_F9,      0,                  }, // xt gt kon rxvt
    {   CSI"21~",       VK_F10,     0,                  }, // xt gt kon rxvt
    {   CSI"23~",       VK_F11,     0,                  }, // xt gt kon rxvt
    {   CSI"24~",       VK_F12,     0,                  }, // xt gt kon rxvt

    {   "\x7F",         VK_BACK,    0,                  },
    {   ESC"\x7F",      VK_BACK,    LEFT_ALT_PRESSED,   },

    // arrow keys
    {   CSI"A",         VK_UP,      0,                  }, // xt gt kon rxvt
    {   CSI"B",         VK_DOWN,    0,                  }, // xt gt kon rxvt
    {   CSI"C",         VK_RIGHT,   0,                  }, // xt gt kon rxvt
    {   CSI"D",         VK_LEFT,    0,                  }, // xt gt kon rxvt
    // ctrl-<arrow>
    {   CSI"1;5A",      VK_UP,      LEFT_CTRL_PRESSED   }, // xt gt kon
    {   CSI"1;5B",      VK_DOWN,    LEFT_CTRL_PRESSED   }, // xt gt kon
    {   CSI"1;5C",      VK_RIGHT,   LEFT_CTRL_PRESSED   }, // xt gt kon
    {   CSI"1;5D",      VK_LEFT,    LEFT_CTRL_PRESSED   }, // xt gt kon
    {   ESC"Oa",        VK_UP,      LEFT_CTRL_PRESSED   }, // rxvt
    {   ESC"Ob",        VK_DOWN,    LEFT_CTRL_PRESSED   }, // rxvt
    {   ESC"Oc",        VK_RIGHT,   LEFT_CTRL_PRESSED   }, // rxvt
    {   ESC"Od",        VK_LEFT,    LEFT_CTRL_PRESSED   }, // rxvt
    // alt-<arrow>
    {   CSI"1;3A",      VK_UP,      LEFT_ALT_PRESSED    }, // xt gt kon
    {   CSI"1;3B",      VK_DOWN,    LEFT_ALT_PRESSED    }, // xt gt kon
    {   CSI"1;3C",      VK_RIGHT,   LEFT_ALT_PRESSED    }, // xt gt kon
    {   CSI"1;3D",      VK_LEFT,    LEFT_ALT_PRESSED    }, // xt gt kon
    {   ESC CSI"A",     VK_UP,      LEFT_ALT_PRESSED    }, // rxvt
    {   ESC CSI"B",     VK_DOWN,    LEFT_ALT_PRESSED    }, // rxvt
    {   ESC CSI"C",     VK_RIGHT,   LEFT_ALT_PRESSED    }, // rxvt
    {   ESC CSI"D",     VK_LEFT,    LEFT_ALT_PRESSED    }, // rxvt

    // insert,delete,home,end,pgup,pgdn
    {   CSI"2~",        VK_INSERT,  0,                  }, // xt gt kon rxvt
    {   CSI"3~",        VK_DELETE,  0,                  }, // xt gt kon rxvt
    {   CSI"5~",        VK_PRIOR,   0,                  }, // xt gt kon rxvt
    {   CSI"6~",        VK_NEXT,    0,                  }, // xt gt kon rxvt
    {   CSI"H",         VK_HOME,    0,                  }, // xt kon
    {   CSI"F",         VK_END,     0,                  }, // xt kon
    {   ESC"OH",        VK_HOME,    0,                  }, // gt
    {   ESC"OF",        VK_END,     0,                  }, // gt
    {   CSI"7^",        VK_HOME,    0,                  }, // rxvt
    {   CSI"8^",        VK_END,     0,                  }, // rxvt
    // ctrl-<key>
    {   CSI"2;5~",      VK_INSERT,  LEFT_CTRL_PRESSED   }, // xt
    {   CSI"3;5~",      VK_DELETE,  LEFT_CTRL_PRESSED   }, // xt gt kon
    {   CSI"1;5H",      VK_HOME,    LEFT_CTRL_PRESSED   }, // xt kon
    {   CSI"1;5F",      VK_END,     LEFT_CTRL_PRESSED   }, // xt kon
    {   CSI"5;5~",      VK_PRIOR,   LEFT_CTRL_PRESSED   }, // xt gt
    {   CSI"6;5~",      VK_NEXT,    LEFT_CTRL_PRESSED   }, // xt gt
    {   CSI"2^",        VK_INSERT,  LEFT_CTRL_PRESSED   }, // rxvt
    {   CSI"3^",        VK_DELETE,  LEFT_CTRL_PRESSED   }, // rxvt
    {   CSI"7^",        VK_HOME,    LEFT_CTRL_PRESSED   }, // rxvt
    {   CSI"8^",        VK_END,     LEFT_CTRL_PRESSED   }, // rxvt
    {   CSI"5^",        VK_PRIOR,   LEFT_CTRL_PRESSED   }, // rxvt
    {   CSI"6^",        VK_NEXT,    LEFT_CTRL_PRESSED   }, // rxvt
    // alt-<key>
    {   CSI"2;3~",      VK_INSERT,  LEFT_ALT_PRESSED    }, // xt gt
    {   CSI"3;3~",      VK_DELETE,  LEFT_ALT_PRESSED    }, // xt gt
    {   CSI"1;3H",      VK_HOME,    LEFT_ALT_PRESSED    }, // xt
    {   CSI"1;3F",      VK_END,     LEFT_ALT_PRESSED    }, // xt
    {   CSI"5;3~",      VK_PRIOR,   LEFT_ALT_PRESSED    }, // xt gt
    {   CSI"6;3~",      VK_NEXT,    LEFT_ALT_PRESSED    }, // xt gt
    {   ESC CSI"2~",    VK_INSERT,  LEFT_ALT_PRESSED    }, // rxvt
    {   ESC CSI"3~",    VK_DELETE,  LEFT_ALT_PRESSED    }, // rxvt
    {   ESC CSI"7~",    VK_HOME,    LEFT_ALT_PRESSED    }, // rxvt
    {   ESC CSI"8~",    VK_END,     LEFT_ALT_PRESSED    }, // rxvt
    {   ESC CSI"5~",    VK_PRIOR,   LEFT_ALT_PRESSED    }, // rxvt
    {   ESC CSI"6~",    VK_NEXT,    LEFT_ALT_PRESSED    }, // rxvt
};

ConsoleInput::ConsoleInput(Win32Console *console) : m_console(console), lastWriteTick(0)
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
        int charSize = appendChar(records, &data[idx], m_byteQueue.size() - idx, isEof);
        if (charSize == -1)
            break;
        idx += charSize;
    }
    m_byteQueue.erase(0, idx);
    m_console->writeInput(records.data(), records.size());
}

int ConsoleInput::appendChar(std::vector<INPUT_RECORD> &records,
                             const char *input,
                             int inputSize,
                             bool isEof)
{
    if (*input == '\x03' && m_console->processedInputMode()) {
        Trace("Ctrl-C");
        BOOL ret = GenerateConsoleCtrlEvent(CTRL_C_EVENT, 0);
        Trace("GenerateConsoleCtrlEvent: %d", ret);
        return 1;
    }

    bool incomplete;
    const KeyDescriptor *match = lookupKey(input, isEof, &incomplete);
    if (incomplete) {
        // Incomplete match -- need more characters (or wait for a
        // timeout to signify flushed input).
        return -1;
    } else if (match != NULL) {
        Trace("keypress: VK=%d (%d bytes)", match->virtualKey, match->encodingLen);
        INPUT_RECORD ir;
        memset(&ir, 0, sizeof(ir));
        ir.EventType = KEY_EVENT;
        ir.Event.KeyEvent.bKeyDown = TRUE;
        ir.Event.KeyEvent.wRepeatCount = 1;
        ir.Event.KeyEvent.wVirtualKeyCode = match->virtualKey;
        ir.Event.KeyEvent.wVirtualScanCode =
                MapVirtualKey(match->virtualKey, MAPVK_VK_TO_VSC);
        ir.Event.KeyEvent.uChar.UnicodeChar =
                MapVirtualKey(match->virtualKey, MAPVK_VK_TO_CHAR);
        records.push_back(ir);
        ir.Event.KeyEvent.bKeyDown = FALSE;
        records.push_back(ir);
        return match->encodingLen;
    } else {
        // A UTF-8 character.
        int len = utf8CharLength(input[0]);
        if (len > inputSize) {
            // Incomplete character.
            return -1;
        }
        Trace("UTF-8 char (%d bytes)", len);
        WCHAR wideInput[2];
        int wideLen = MultiByteToWideChar(CP_UTF8,
                                          0,
                                          input,
                                          len,
                                          wideInput,
                                          sizeof(wideInput) / sizeof(wideInput[0]));
        // TODO: Characters outside the BMP.
        if (wideLen == 1) {
            short vk = VkKeyScan(wideInput[0]);
            INPUT_RECORD ir;
            memset(&ir, 0, sizeof(ir));
            ir.EventType = KEY_EVENT;
            ir.Event.KeyEvent.bKeyDown = TRUE;
            ir.Event.KeyEvent.wRepeatCount = 1;
            if (vk != -1) {
                ir.Event.KeyEvent.wVirtualKeyCode = vk & 0xFF;
                int keyState = 0;
                if (vk & 0x100)
                    keyState |= SHIFT_PRESSED;
                else if (vk & 0x200)
                    keyState |= LEFT_CTRL_PRESSED;
                else if (vk & 0x400)
                    keyState |= LEFT_ALT_PRESSED;
                ir.Event.KeyEvent.dwControlKeyState = keyState;
            }
            ir.Event.KeyEvent.wVirtualScanCode =
                    MapVirtualKey(ir.Event.KeyEvent.wVirtualKeyCode,
                                  MAPVK_VK_TO_VSC);
            ir.Event.KeyEvent.uChar.UnicodeChar = wideInput[0];
            Trace("vk:%d sc:%d wc:%d cks:%d",
                  ir.Event.KeyEvent.wVirtualKeyCode,
                  ir.Event.KeyEvent.wVirtualScanCode,
                  ir.Event.KeyEvent.uChar.UnicodeChar,
                  ir.Event.KeyEvent.dwControlKeyState);
            records.push_back(ir);
            ir.Event.KeyEvent.bKeyDown = FALSE;
            records.push_back(ir);
        }
        return len;
    }
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
