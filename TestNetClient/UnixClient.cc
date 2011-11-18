#include "UnixClient.h"
#include "UnixSignalHandler.h"
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <QSocketNotifier>
#include <QCoreApplication>
#include <QTcpSocket>
#include <QByteArray>


const int INPUT_BUFFER_SIZE = 1024;
const int SOCKET_BUFFER_SIZE = 32 * 1024;
const int SCREEN_BUFFER_SIZE = 32 * 1024;

UnixClient::UnixClient(QTcpSocket *socket, QObject *parent) :
    QObject(parent), socket(socket)
{
    // Configure input.
    savedTermios = setRawTerminalMode();

    // Terminal I/O notifiers.
    terminalReadNotifier = new QSocketNotifier(STDIN_FILENO, QSocketNotifier::Read, this);
    terminalReadNotifier->setEnabled(true);
    connect(terminalReadNotifier, SIGNAL(activated(int)), SLOT(terminalReadActivated()));
    terminalWriteNotifier = new QSocketNotifier(STDOUT_FILENO, QSocketNotifier::Write, this);
    terminalWriteNotifier->setEnabled(true);
    connect(terminalWriteNotifier, SIGNAL(activated(int)), SLOT(terminalWriteActivated()));

    // Read from TCP.
    socket->setReadBufferSize(SOCKET_BUFFER_SIZE);
    connect(socket, SIGNAL(readyRead()), SLOT(socketReadyRead()));
    connect(socket, SIGNAL(bytesWritten(qint64)), SLOT(socketBytesWritten()));
    connect(socket, SIGNAL(disconnected()), SLOT(socketDisconnected()));

    // Detect terminal resizing.
    UnixSignalHandler *ush = new UnixSignalHandler(SIGWINCH, this);
    connect(ush, SIGNAL(signaled(int)), SLOT(terminalResized()));
    terminalResized();
}

UnixClient::~UnixClient()
{
    restoreTerminalMode(savedTermios);
}

// Put the input terminal into non-blocking non-canonical mode.
termios UnixClient::setRawTerminalMode()
{
    if (!isatty(STDIN_FILENO))
        qFatal("input is not a tty");
    if (!isatty(STDOUT_FILENO))
        qFatal("output is not a tty");

    // This code makes the terminal output non-blocking.
    int flags = fcntl(STDOUT_FILENO, F_GETFL, 0);
    if (flags == -1)
        qFatal("fcntl F_GETFL on stdout failed");
    if (fcntl(STDOUT_FILENO, F_SETFL, flags | O_NONBLOCK) == -1)
        qFatal("fcntl F_SETFL on stdout failed");

    termios buf;
    if (tcgetattr(STDIN_FILENO, &buf) < 0)
        qFatal("tcgetattr failed");
    termios saved = buf;
    buf.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    buf.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    buf.c_cflag &= ~(CSIZE | PARENB);
    buf.c_cflag |= CS8;
    buf.c_oflag &= ~OPOST;
    buf.c_cc[VMIN] = 0;
    buf.c_cc[VTIME] = 0;
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &buf) < 0)
        qFatal("tcsetattr failed");
    return saved;
}

void UnixClient::restoreTerminalMode(termios original)
{
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &original) < 0)
        qFatal("error restoring terminal mode");
}

void UnixClient::terminalResized()
{
    winsize sz;
    ioctl(STDIN_FILENO, TIOCGWINSZ, &sz);
    //printf("%d %d\r\n", sz.ws_col, sz.ws_row);

    // I invented this escape sequence out of thin air for the "TestNet"
    // protocol.
    char cmd[16];
    sprintf(cmd, "\x1B[:r%08x", (sz.ws_row << 16) | sz.ws_col);
    socket->write(cmd);
}

void UnixClient::socketDisconnected()
{
    printf("Connection terminated\r\n");
    QCoreApplication::quit();
}

/////////////////////////////////////////////////////////////////////////////
// I/O: client -> server

void UnixClient::terminalReadActivated()
{
    terminalReadNotifier->setEnabled(false);

    char buf[INPUT_BUFFER_SIZE];
    int count = read(0, buf, sizeof(buf));
    Q_ASSERT(count > 0);
    qint64 actualWritten = socket->write(buf, count);
    Q_ASSERT(actualWritten == count);

    terminalReadNotifier->setEnabled(socket->bytesToWrite() < INPUT_BUFFER_SIZE);
}

void UnixClient::socketBytesWritten()
{
    terminalReadNotifier->setEnabled(socket->bytesToWrite() < INPUT_BUFFER_SIZE);
}

/////////////////////////////////////////////////////////////////////////////
// I/O: server -> client

void UnixClient::socketReadyRead()
{
    doServerToClient();
}

void UnixClient::terminalWriteActivated()
{
    terminalWriteNotifier->setEnabled(false);
    doServerToClient();
}

void UnixClient::doServerToClient()
{
    bool did_something;
    do {
        did_something = false;
        if (terminalWriteBuffer.size() < SCREEN_BUFFER_SIZE) {
            QByteArray data = socket->read(SCREEN_BUFFER_SIZE - terminalWriteBuffer.size());
            if (data.size() > 0) {
                terminalWriteBuffer.append(data);
                did_something = true;
            }
        }
        if (!terminalWriteBuffer.isEmpty()) {
            int actual = write(STDOUT_FILENO,
                               terminalWriteBuffer.constData(),
                               terminalWriteBuffer.size());
            if (actual > 0) {
                did_something = true;
                terminalWriteBuffer = terminalWriteBuffer.mid(actual);
            } else if (actual == 0) {
                fprintf(stderr, "error: stdout closed\n");
                QCoreApplication::exit(1);
            } else if (errno != EAGAIN) {
                perror("error writing to stdout");
                QCoreApplication::exit(1);
            }
        }
    } while(did_something);

    terminalWriteNotifier->setEnabled(!terminalWriteBuffer.isEmpty());
}
