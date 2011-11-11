#ifndef CLIENT_H
#define CLIENT_H

#include <QObject>
#include <QByteArray>
#include <termios.h>

class QSocketNotifier;
class QTcpSocket;

class UnixClient : public QObject
{
    Q_OBJECT
public:
    explicit UnixClient(QTcpSocket *socket, QObject *parent = 0);
    virtual ~UnixClient();

private:
    static termios setRawTerminalMode();
    static void restoreTerminalMode(termios original);

signals:

private slots:
    void terminalReadActivated();
    void socketBytesWritten();
    void socketReadyRead();
    void terminalWriteActivated();
    void doServerToClient();

public slots:

private:
    termios savedTermios;
    QSocketNotifier *terminalReadNotifier;
    QSocketNotifier *terminalWriteNotifier;
    QTcpSocket *socket;
    QByteArray terminalWriteBuffer;
};

#endif // CLIENT_H
