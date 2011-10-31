#include "TextWidget.h"
#include "AgentClient.h"
#include <QKeyEvent>
#include <QtDebug>

TextWidget::TextWidget(QWidget *parent) :
    QWidget(parent),
    m_agentClient(NULL)
{
}

void TextWidget::initWithAgent(AgentClient *agentClient)
{
    m_agentClient = agentClient;
}

void TextWidget::keyPressEvent(QKeyEvent *event)
{
    m_agentClient->sendKeyPress(event);
}

void TextWidget::keyReleaseEvent(QKeyEvent *event)
{
    m_agentClient->sendKeyRelease(event);
}
