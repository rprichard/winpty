#ifndef TEXTWIDGET_H
#define TEXTWIDGET_H

#include <QWidget>
#include <QKeyEvent>

class AgentClient;

class TextWidget : public QWidget
{
    Q_OBJECT
public:
    explicit TextWidget(QWidget *parent = 0);
    void initWithAgent(AgentClient *agentClient);

signals:

public slots:

protected:
    virtual void keyPressEvent(QKeyEvent *event);
    virtual void keyReleaseEvent(QKeyEvent *event);

private:
    AgentClient *m_agentClient;
};

#endif // TEXTWIDGET_H
