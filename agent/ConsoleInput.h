#ifndef CONSOLEINPUT_H
#define CONSOLEINPUT_H

#include <string>
#include <vector>
#include <windows.h>

class Win32Console;

class ConsoleInput
{
public:
    ConsoleInput(Win32Console *console);
    void writeInput(const std::string &input);
    void flushIncompleteEscapeCode();

private:
    struct KeyDescriptor {
        const char *encoding;
        int virtualKey;
        int keyState;
        int encodingLen;
    };

    class KeyLookup {
    public:
        KeyLookup();
        ~KeyLookup();
        void set(const char *encoding, const KeyDescriptor *descriptor);
        const KeyDescriptor *getMatch() const { return match; }
        bool hasChildren() const { return children != NULL; }
        KeyLookup *getChild(int i) { return children != NULL ? (*children)[i] : NULL; }
    private:
        const KeyDescriptor *match;
        KeyLookup *(*children)[256];
    };

    void doWrite(bool isEof);
    int appendChar(std::vector<INPUT_RECORD> &records,
                   const char *input,
                   int inputSize,
                   bool isEof);
    static int utf8CharLength(char firstByte);
    const KeyDescriptor *lookupKey(const char *encoding, bool isEof, bool *incomplete);

private:
    static KeyDescriptor keyDescriptorTable[];
    Win32Console *m_console;
    std::string m_byteQueue;
    KeyLookup m_lookup;
    DWORD lastWriteTick;
};

#endif // CONSOLEINPUT_H
