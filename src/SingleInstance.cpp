// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "SingleInstance.h"

#include <QLocalSocket>
#include <QDataStream>

SingleInstance::SingleInstance(const QString& key, QObject* parent)
    : QObject(parent)
    , m_key(key)
{
    // Try to connect → if succeeds we are secondary
    QLocalSocket socket;
    socket.connectToServer(m_key);
    if (socket.waitForConnected(200)) {
        m_isPrimary = false;
        return;
    }

    // We are primary – remove stale socket and listen
    QLocalServer::removeServer(m_key);
    m_server = new QLocalServer(this);
    if (!m_server->listen(m_key)) {
        // fallback: still claim primary
        m_isPrimary = true;
        return;
    }
    m_isPrimary = true;
    connect(m_server, &QLocalServer::newConnection, this, &SingleInstance::onNewConnection);
}

SingleInstance::~SingleInstance() {
    if (m_server)
        m_server->close();
}

void SingleInstance::onNewConnection() {
    while (auto* sock = m_server->nextPendingConnection()) {
        connect(sock, &QLocalSocket::readyRead, this, [this, sock]() {
            const QByteArray data = sock->readAll();
            const QString msg = QString::fromUtf8(data);
            emit messageReceived(msg);
            sock->disconnectFromServer();
            sock->deleteLater();
        });
        connect(sock, &QLocalSocket::disconnected, sock, &QLocalSocket::deleteLater);
    }
}

bool SingleInstance::sendMessage(const QString& key, const QString& message) {
    QLocalSocket socket;
    socket.connectToServer(key);
    if (!socket.waitForConnected(500))
        return false;
    socket.write(message.toUtf8());
    socket.flush();
    socket.waitForBytesWritten(500);
    socket.disconnectFromServer();
    return true;
}
