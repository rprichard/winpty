#ifndef BUFFER_H
#define BUFFER_H

#include <sstream>
#include <iostream>

class WriteBuffer
{
private:
    std::stringstream ss;
public:
    void putInt(int i);
    void putWString(const std::wstring &str);
    void putWString(const wchar_t *str);
    std::string str() const;
};

inline void WriteBuffer::putInt(int i)
{
    ss.write((const char*)&i, sizeof(i));
}

inline void WriteBuffer::putWString(const std::wstring &str)
{
    putInt(str.size());
    ss.write((const char*)str.c_str(), sizeof(wchar_t) * str.size());
}

inline void WriteBuffer::putWString(const wchar_t *str)
{
    int len = wcslen(str);
    putInt(len);
    ss.write((const char*)str, sizeof(wchar_t) * len);
}

inline std::string WriteBuffer::str() const
{
    return ss.str();
}

class ReadBuffer
{
private:
    std::stringstream ss;
public:
    ReadBuffer(const std::string &packet);
    int getInt();
    std::wstring getWString();
    bool eof();
};

inline ReadBuffer::ReadBuffer(const std::string &packet) : ss(packet)
{
}

inline int ReadBuffer::getInt()
{
    int i;
    ss.read((char*)&i, sizeof(i));
    return i;
}

inline std::wstring ReadBuffer::getWString()
{
    int len = getInt();
    wchar_t *tmp = new wchar_t[len];
    ss.read((char*)tmp, sizeof(wchar_t) * len);
    std::wstring ret(tmp, len);
    delete [] tmp;
    return ret;
}

inline bool ReadBuffer::eof()
{
    ss.peek();
    return ss.eof();
}

#endif /* BUFFER_H */
