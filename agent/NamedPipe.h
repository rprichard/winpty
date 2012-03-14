#ifndef NAMEDPIPE_H
#define NAMEDPIPE_H

#include <windows.h>
#include <string>

class EventLoop;

class NamedPipe
{
private:
    // The EventLoop uses these private members.
    friend class EventLoop;
    NamedPipe();
    ~NamedPipe();
    HANDLE getWaitEvent1();
    HANDLE getWaitEvent2();
    void poll();

private:
    class IoWorker
    {
    public:
        IoWorker(NamedPipe *namedPipe);
        virtual ~IoWorker();
        void service();
        HANDLE getWaitEvent();
    protected:
        NamedPipe *m_namedPipe;
        bool m_pending;
        HANDLE m_event;
        OVERLAPPED m_over;
        enum { kIoSize = 64 * 1024 };
        char m_buffer[kIoSize];
        virtual void completeIo(int size) = 0;
        virtual bool shouldIssueIo(int *size, bool *isRead) = 0;
    };

    class InputWorker : public IoWorker
    {
    public:
        InputWorker(NamedPipe *namedPipe) : IoWorker(namedPipe) {}
    protected:
        virtual void completeIo(int size);
        virtual bool shouldIssueIo(int *size, bool *isRead);
    };

    class OutputWorker : public IoWorker
    {
    public:
        OutputWorker(NamedPipe *namedPipe) : IoWorker(namedPipe) {}
    protected:
        virtual void completeIo(int size);
        virtual bool shouldIssueIo(int *size, bool *isRead);
    };

public:
    bool connectToServer(LPCWSTR pipeName);
    void write(const void *data, int size);
    void write(const char *text);
    int readBufferSize();
    void setReadBufferSize(int size);
    int bytesAvailable();
    int peek(void *data, int size);
    std::string read(int size);
    std::string readAll();
    void closePipe();
    bool isClosed();

private:
    // Input/output buffers
    int m_readBufferSize;
    std::string m_inQueue;
    std::string m_outQueue;
    HANDLE m_handle;
    InputWorker m_inputWorker;
    OutputWorker m_outputWorker;
};

#endif // NAMEDPIPE_H
