#include "NamedPipe.h"
#include "EventLoop.h"
#include "../Shared/DebugClient.h"
#include <string.h>
#include <assert.h>

NamedPipe::NamedPipe() :
    m_readBufferSize(64 * 1024),
    m_handle(NULL),
    m_inputWorker(this),
    m_outputWorker(this)
{
}

NamedPipe::~NamedPipe()
{
    closePipe();
}

HANDLE NamedPipe::getWaitEvent1()
{
    return m_inputWorker.getWaitEvent();
}

HANDLE NamedPipe::getWaitEvent2()
{
    return m_outputWorker.getWaitEvent();
}

void NamedPipe::poll()
{
    m_inputWorker.service();
    m_outputWorker.service();
}

NamedPipe::IoWorker::IoWorker(NamedPipe *namedPipe) :
    m_namedPipe(namedPipe),
    m_pending(false)
{
    m_event = CreateEvent(NULL, TRUE, FALSE, NULL);
    assert(m_event != NULL);
}

NamedPipe::IoWorker::~IoWorker()
{
    // TODO: Does it matter if an I/O is currently pending?
    CloseHandle(m_event);
}

void NamedPipe::IoWorker::service()
{
    if (m_namedPipe->isClosed()) {
        m_pending = false;
        ResetEvent(m_event);
        return;
    }

    if (m_pending) {
        DWORD actual;
        BOOL ret = GetOverlappedResult(m_namedPipe->m_handle, &m_over, &actual, FALSE);
        if (!ret) {
            if (GetLastError() == ERROR_IO_INCOMPLETE) {
                // There is a pending I/O.
                return;
            } else {
                // Pipe error.  Close the pipe.
                ResetEvent(m_event);
                m_pending = false;
                m_namedPipe->closePipe();
                return;
            }
        }
        ResetEvent(m_event);
        m_pending = false;
        completeIo(actual);
    }
    int nextSize;
    bool isRead;
    while (shouldIssueIo(&nextSize, &isRead)) {
        DWORD actual = 0;
        memset(&m_over, 0, sizeof(m_over));
        m_over.hEvent = m_event;
        Trace("[startio] isread:%d size:%d", isRead, nextSize);
        BOOL ret = isRead
                ? ReadFile(m_namedPipe->m_handle, m_buffer, nextSize, &actual, &m_over)
                : WriteFile(m_namedPipe->m_handle, m_buffer, nextSize, &actual, &m_over);
        if (!ret) {
            if (GetLastError() == ERROR_IO_PENDING) {
                // There is a pending I/O.
                m_pending = true;
                return;
            } else {
                // Pipe error.  Close the pipe.
                m_namedPipe->closePipe();
                return;
            }
        }
        completeIo(actual);
    }
}

HANDLE NamedPipe::IoWorker::getWaitEvent()
{
    return m_pending ? m_event : NULL;
}

void NamedPipe::InputWorker::completeIo(int size)
{
    m_namedPipe->m_inQueue.append(m_buffer, size);
}

bool NamedPipe::InputWorker::shouldIssueIo(int *size, bool *isRead)
{
    *isRead = true;
    if (m_namedPipe->isClosed()) {
        return false;
    } else if ((int)m_namedPipe->m_inQueue.size() < m_namedPipe->readBufferSize()) {
        *size = kIoSize;
        return true;
    } else {
        return false;
    }
}

void NamedPipe::OutputWorker::completeIo(int size)
{
}

bool NamedPipe::OutputWorker::shouldIssueIo(int *size, bool *isRead)
{
    *isRead = false;
    if (!m_namedPipe->m_outQueue.empty()) {
        int writeSize = std::min((int)m_namedPipe->m_outQueue.size(), (int)kIoSize);
        memcpy(m_buffer, m_namedPipe->m_outQueue.data(), writeSize);
        m_namedPipe->m_outQueue.erase(0, writeSize);
        *size = writeSize;
        return true;
    } else {
        return false;
    }
}

bool NamedPipe::connectToServer(LPCWSTR pipeName)
{
    assert(m_handle == NULL);
    m_handle = CreateFile(pipeName,
                          GENERIC_READ | GENERIC_WRITE,
                          0,
                          NULL,
                          OPEN_EXISTING,
                          FILE_FLAG_OVERLAPPED,
                          NULL);
    Trace("connection to [%ls], handle == 0x%x", pipeName, m_handle);
    if (m_handle == INVALID_HANDLE_VALUE)
        return false;
    m_inputWorker.service();

    // TODO: I suppose the user could call write before calling
    // connectToServer.  I think that would work, but I'm not sure.
    m_outputWorker.service();

    return true;
}

void NamedPipe::write(const void *data, int size)
{
    m_outQueue.append((const char*)data, size);
    m_outputWorker.service();
}

void NamedPipe::write(const char *text)
{
    write(text, strlen(text));
}

int NamedPipe::readBufferSize()
{
    return m_readBufferSize;
}

void NamedPipe::setReadBufferSize(int size)
{
    m_readBufferSize = size;
    m_inputWorker.service();
}

int NamedPipe::bytesAvailable()
{
    return m_inQueue.size();
}

int NamedPipe::peek(void *data, int size)
{
    int ret = std::min(size, (int)m_inQueue.size());
    memcpy(data, m_inQueue.data(), ret);
    return ret;
}

std::string NamedPipe::read(int size)
{
    int retSize = std::min(size, (int)m_inQueue.size());
    std::string ret = m_inQueue.substr(0, retSize);
    m_inQueue.erase(0, retSize);
    m_inputWorker.service();
    return ret;
}

std::string NamedPipe::readAll()
{
    std::string ret = m_inQueue;
    m_inQueue.clear();
    m_inputWorker.service();
    return ret;
}

void NamedPipe::closePipe()
{
    // TODO: Use CancelIo, ResetEvent, etc, to ensure that the socket is in
    // a completely shut down state when this function returns.
    if (m_handle == NULL)
        return;
    CloseHandle(m_handle);
    m_handle = NULL;
}

bool NamedPipe::isClosed()
{
    return m_handle == NULL;
}
