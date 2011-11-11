// TestNetClient: a command-line client to the TestNetServer

#include "UnixClient.h"
#include <QCoreApplication>
#include <QTcpSocket>

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    if (argc != 3) {
        printf("Usage: %s host port\n", argv[0]);
        return 1;
    }

    const char *hostname = argv[1];
    int port = atoi(argv[2]);

    QTcpSocket socket;
    socket.connectToHost(hostname, port);
    if (!socket.waitForConnected())
        qFatal("Error connecting to %s:%d", hostname, port);

    // TODO: Should Nagle's algorithm be on or off for a remote-echo shell
    // protocol?  I turned it off because the TestNetServer doesn't send any
    // data to the client yet, so Nagle's algorithm made input appear laggy
    // in the Windows Console window on the server.
    socket.setSocketOption(QAbstractSocket::LowDelayOption, 1);

    UnixClient unixClient(&socket);
    return a.exec();
}
