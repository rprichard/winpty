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

ConsoleInput::KeyDescriptor ConsoleInput::keyDescriptorTable[] = {
    // Ctrl-<letter/digit> seems to be handled OK by the default code path.

    // Alt-<letter/digit>
    {   "\x1B""a",      'A',        LEFT_ALT_PRESSED    },
    {   "\x1B""b",      'B',        LEFT_ALT_PRESSED    },
    {   "\x1B""c",      'C',        LEFT_ALT_PRESSED    },
    {   "\x1B""d",      'D',        LEFT_ALT_PRESSED    },
    {   "\x1B""e",      'E',        LEFT_ALT_PRESSED    },
    {   "\x1B""f",      'F',        LEFT_ALT_PRESSED    },
    {   "\x1Bg",        'G',        LEFT_ALT_PRESSED    },
    {   "\x1Bh",        'H',        LEFT_ALT_PRESSED    },
    {   "\x1Bi",        'I',        LEFT_ALT_PRESSED    },
    {   "\x1Bj",        'J',        LEFT_ALT_PRESSED    },
    {   "\x1Bk",        'K',        LEFT_ALT_PRESSED    },
    {   "\x1Bl",        'L',        LEFT_ALT_PRESSED    },
    {   "\x1Bm",        'M',        LEFT_ALT_PRESSED    },
    {   "\x1Bn",        'N',        LEFT_ALT_PRESSED    },
    {   "\x1Bo",        'O',        LEFT_ALT_PRESSED    },
    {   "\x1Bp",        'P',        LEFT_ALT_PRESSED    },
    {   "\x1Bq",        'Q',        LEFT_ALT_PRESSED    },
    {   "\x1Br",        'R',        LEFT_ALT_PRESSED    },
    {   "\x1Bs",        'S',        LEFT_ALT_PRESSED    },
    {   "\x1Bt",        'T',        LEFT_ALT_PRESSED    },
    {   "\x1Bu",        'U',        LEFT_ALT_PRESSED    },
    {   "\x1Bv",        'V',        LEFT_ALT_PRESSED    },
    {   "\x1Bw",        'W',        LEFT_ALT_PRESSED    },
    {   "\x1Bx",        'X',        LEFT_ALT_PRESSED    },
    {   "\x1By",        'Y',        LEFT_ALT_PRESSED    },
    {   "\x1Bz",        'Z',        LEFT_ALT_PRESSED    },
    {   "\x1B""A",      'A',        LEFT_ALT_PRESSED    },
    {   "\x1B""B",      'B',        LEFT_ALT_PRESSED    },
    {   "\x1B""C",      'C',        LEFT_ALT_PRESSED    },
    {   "\x1B""D",      'D',        LEFT_ALT_PRESSED    },
    {   "\x1B""E",      'E',        LEFT_ALT_PRESSED    },
    {   "\x1B""F",      'F',        LEFT_ALT_PRESSED    },
    {   "\x1BG",        'G',        LEFT_ALT_PRESSED    },
    {   "\x1BH",        'H',        LEFT_ALT_PRESSED    },
    {   "\x1BI",        'I',        LEFT_ALT_PRESSED    },
    {   "\x1BJ",        'J',        LEFT_ALT_PRESSED    },
    {   "\x1BK",        'K',        LEFT_ALT_PRESSED    },
    {   "\x1BL",        'L',        LEFT_ALT_PRESSED    },
    {   "\x1BM",        'M',        LEFT_ALT_PRESSED    },
    {   "\x1BN",        'N',        LEFT_ALT_PRESSED    },
    {   "\x1BO",        'O',        LEFT_ALT_PRESSED    },
    {   "\x1BP",        'P',        LEFT_ALT_PRESSED    },
    {   "\x1BQ",        'Q',        LEFT_ALT_PRESSED    },
    {   "\x1BR",        'R',        LEFT_ALT_PRESSED    },
    {   "\x1BS",        'S',        LEFT_ALT_PRESSED    },
    {   "\x1BT",        'T',        LEFT_ALT_PRESSED    },
    {   "\x1BU",        'U',        LEFT_ALT_PRESSED    },
    {   "\x1BV",        'V',        LEFT_ALT_PRESSED    },
    {   "\x1BW",        'W',        LEFT_ALT_PRESSED    },
    {   "\x1BX",        'X',        LEFT_ALT_PRESSED    },
    {   "\x1BY",        'Y',        LEFT_ALT_PRESSED    },
    {   "\x1BZ",        'Z',        LEFT_ALT_PRESSED    },

    {   "\x1B",         VK_ESCAPE,  0,                  },
    {   "\x1B[",        '[',        LEFT_ALT_PRESSED    },

    // Function keys
    {   "\x1BOP",       VK_F1,      0,                  }, // xt gt kon
    {   "\x1BOQ",       VK_F2,      0,                  }, // xt gt kon
    {   "\x1BOR",       VK_F3,      0,                  }, // xt gt kon
    {   "\x1BOS",       VK_F4,      0,                  }, // xt gt kon
    {   "\x1B[11~",     VK_F1,      0,                  }, // rxvt
    {   "\x1B[12~",     VK_F2,      0,                  }, // rxvt
    {   "\x1B[13~",     VK_F3,      0,                  }, // rxvt
    {   "\x1B[14~",     VK_F4,      0,                  }, // rxvt
    {   "\x1B[15~",     VK_F5,      0,                  }, // xt gt kon rxvt
    {   "\x1B[17~",     VK_F6,      0,                  }, // xt gt kon rxvt
    {   "\x1B[18~",     VK_F7,      0,                  }, // xt gt kon rxvt
    {   "\x1B[19~",     VK_F8,      0,                  }, // xt gt kon rxvt
    {   "\x1B[20~",     VK_F9,      0,                  }, // xt gt kon rxvt
    {   "\x1B[21~",     VK_F10,     0,                  }, // xt gt kon rxvt
    {   "\x1B[23~",     VK_F11,     0,                  }, // xt gt kon rxvt
    {   "\x1B[24~",     VK_F12,     0,                  }, // xt gt kon rxvt

    {   "\x7F",         VK_BACK,    0,                  },
    {   "\x1B\x7F",     VK_BACK,    LEFT_ALT_PRESSED,   },

    // arrow keys
    {   "\x1B[A",       VK_UP,      0,                  }, // xt gt kon rxvt
    {   "\x1B[B",       VK_DOWN,    0,                  }, // xt gt kon rxvt
    {   "\x1B[C",       VK_RIGHT,   0,                  }, // xt gt kon rxvt
    {   "\x1B[D",       VK_LEFT,    0,                  }, // xt gt kon rxvt
    // ctrl-<arrow>
    {   "\x1B[1;5A",    VK_UP,      LEFT_CTRL_PRESSED   }, // xt gt kon
    {   "\x1B[1;5B",    VK_DOWN,    LEFT_CTRL_PRESSED   }, // xt gt kon
    {   "\x1B[1;5C",    VK_RIGHT,   LEFT_CTRL_PRESSED   }, // xt gt kon
    {   "\x1B[1;5D",    VK_LEFT,    LEFT_CTRL_PRESSED   }, // xt gt kon
    {   "\x1BOa",       VK_UP,      LEFT_CTRL_PRESSED   }, // rxvt
    {   "\x1BOb",       VK_DOWN,    LEFT_CTRL_PRESSED   }, // rxvt
    {   "\x1BOc",       VK_RIGHT,   LEFT_CTRL_PRESSED   }, // rxvt
    {   "\x1BOd",       VK_LEFT,    LEFT_CTRL_PRESSED   }, // rxvt
    // alt-<arrow>
    {   "\x1B[1;3A",    VK_UP,      LEFT_ALT_PRESSED    }, // xt gt kon
    {   "\x1B[1;3B",    VK_DOWN,    LEFT_ALT_PRESSED    }, // xt gt kon
    {   "\x1B[1;3C",    VK_RIGHT,   LEFT_ALT_PRESSED    }, // xt gt kon
    {   "\x1B[1;3D",    VK_LEFT,    LEFT_ALT_PRESSED    }, // xt gt kon
    {   "\x1B\x1B[A",   VK_UP,      LEFT_ALT_PRESSED    }, // rxvt
    {   "\x1B\x1B[B",   VK_DOWN,    LEFT_ALT_PRESSED    }, // rxvt
    {   "\x1B\x1B[C",   VK_RIGHT,   LEFT_ALT_PRESSED    }, // rxvt
    {   "\x1B\x1B[D",   VK_LEFT,    LEFT_ALT_PRESSED    }, // rxvt

    // insert,delete,home,end,pgup,pgdn
    {   "\x1B[2~",      VK_INSERT,  0,                  }, // xt gt kon rxvt
    {   "\x1B[3~",      VK_DELETE,  0,                  }, // xt gt kon rxvt
    {   "\x1B[5~",      VK_PRIOR,   0,                  }, // xt gt kon rxvt
    {   "\x1B[6~",      VK_NEXT,    0,                  }, // xt gt kon rxvt
    {   "\x1B[H",       VK_HOME,    0,                  }, // xt kon
    {   "\x1B[F",       VK_END,     0,                  }, // xt kon
    {   "\x1BOH",       VK_HOME,    0,                  }, // gt
    {   "\x1BOF",       VK_END,     0,                  }, // gt
    {   "\x1B[7^",      VK_HOME,    0,                  }, // rxvt
    {   "\x1B[8^",      VK_END,     0,                  }, // rxvt
    // ctrl-<key>
    {   "\x1B[2;5~",    VK_INSERT,  LEFT_CTRL_PRESSED   }, // xt
    {   "\x1B[3;5~",    VK_DELETE,  LEFT_CTRL_PRESSED   }, // xt gt kon
    {   "\x1B[1;5H",    VK_HOME,    LEFT_CTRL_PRESSED   }, // xt kon
    {   "\x1B[1;5F",    VK_END,     LEFT_CTRL_PRESSED   }, // xt kon
    {   "\x1B[5;5~",    VK_PRIOR,   LEFT_CTRL_PRESSED   }, // xt gt
    {   "\x1B[6;5~",    VK_NEXT,    LEFT_CTRL_PRESSED   }, // xt gt
    {   "\x1B[2^",      VK_INSERT,  LEFT_CTRL_PRESSED   }, // rxvt
    {   "\x1B[3^",      VK_DELETE,  LEFT_CTRL_PRESSED   }, // rxvt
    {   "\x1B[7^",      VK_HOME,    LEFT_CTRL_PRESSED   }, // rxvt
    {   "\x1B[8^",      VK_END,     LEFT_CTRL_PRESSED   }, // rxvt
    {   "\x1B[5^",      VK_PRIOR,   LEFT_CTRL_PRESSED   }, // rxvt
    {   "\x1B[6^",      VK_NEXT,    LEFT_CTRL_PRESSED   }, // rxvt
    // alt-<key>
    {   "\x1B[2;3~",    VK_INSERT,  LEFT_ALT_PRESSED    }, // xt gt
    {   "\x1B[3;3~",    VK_DELETE,  LEFT_ALT_PRESSED    }, // xt gt
    {   "\x1B[1;3H",    VK_HOME,    LEFT_ALT_PRESSED    }, // xt
    {   "\x1B[1;3F",    VK_END,     LEFT_ALT_PRESSED    }, // xt
    {   "\x1B[5;3~",    VK_PRIOR,   LEFT_ALT_PRESSED    }, // xt gt
    {   "\x1B[6;3~",    VK_NEXT,    LEFT_ALT_PRESSED    }, // xt gt
    {   "\x1B\x1B[2~",  VK_INSERT,  LEFT_ALT_PRESSED    }, // rxvt
    {   "\x1B\x1B[3~",  VK_DELETE,  LEFT_ALT_PRESSED    }, // rxvt
    {   "\x1B\x1B[7~",  VK_HOME,    LEFT_ALT_PRESSED    }, // rxvt
    {   "\x1B\x1B[8~",  VK_END,     LEFT_ALT_PRESSED    }, // rxvt
    {   "\x1B\x1B[5~",  VK_PRIOR,   LEFT_ALT_PRESSED    }, // rxvt
    {   "\x1B\x1B[6~",  VK_NEXT,    LEFT_ALT_PRESSED    }, // rxvt
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
