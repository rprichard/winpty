#ifndef CLIENT_H
#define CLIENT_H

#include <QObject>

#ifdef _WIN32
#define IS_WINDOWS
#else
#define IS_UNIX
#endif

class Client : public QObject
{
    Q_OBJECT
public:
    explicit Client(QObject *parent = 0);

signals:

public slots:

};

#endif // CLIENT_H
