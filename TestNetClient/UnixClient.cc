#include "UnixClient.h"
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#include <QSocketNotifier>
#include <QCoreApplication>
#include <QTcpSocket>
#include <QByteArray>


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
    connect(socket, SIGNAL(readyRead()), SLOT(socketReadyRead()));
    connect(socket, SIGNAL(bytesWritten(qint64)), SLOT(socketBytesWritten()));
    connect(socket, SIGNAL(disconnected()), SLOT(socketDisconnected()));
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

    char buf[512];
    int count = read(0, buf, sizeof(buf));
    Q_ASSERT(count > 0);
    qint64 actualWritten = socket->write(buf, count);
    Q_ASSERT(actualWritten == count);

    terminalReadNotifier->setEnabled(socket->bytesToWrite() < 4096);
}

void UnixClient::socketBytesWritten()
{
    terminalReadNotifier->setEnabled(socket->bytesToWrite() < 4096);
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
    do {
        if (terminalWriteBuffer.size() < 4096) {
            QByteArray data = socket->readAll();
            if (data.size() > 0) {
                terminalWriteBuffer.append(data);
                continue;
            }
        }
        if (!terminalWriteBuffer.isEmpty()) {
            int actual = write(STDOUT_FILENO,
                               terminalWriteBuffer.constData(),
                               terminalWriteBuffer.size());
            Q_ASSERT(actual >= 0);
            if (actual > 0) {
                terminalWriteBuffer = terminalWriteBuffer.mid(actual);
                continue;
            }
        }
    } while(0);

    terminalWriteNotifier->setEnabled(!terminalWriteBuffer.isEmpty());
}
