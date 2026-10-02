// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "SingleInstance.h"

#include <QLocalSocket>

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
        // Helper: secondary may write and close before readyRead is connected.
        auto consume = [this, sock]() {
            const QByteArray data = sock->readAll();
            if (data.isEmpty())
                return;
            emit messageReceived(QString::fromUtf8(data).trimmed());
            sock->disconnectFromServer();
            sock->deleteLater();
        };

        connect(sock, &QLocalSocket::readyRead, this, consume);
        connect(sock, &QLocalSocket::disconnected, sock, &QLocalSocket::deleteLater);

        // Data may already be buffered
        if (sock->bytesAvailable() > 0)
            consume();
    }
}

bool SingleInstance::sendMessage(const QString& key, const QString& message) {
    QLocalSocket socket;
    socket.connectToServer(key);
    if (!socket.waitForConnected(1000))
        return false;
    const QByteArray payload = message.toUtf8();
    if (socket.write(payload) != payload.size())
        return false;
    socket.flush();
    socket.waitForBytesWritten(1000);
    // Give the server a moment to read before we tear down the socket
    socket.waitForDisconnected(200);
    socket.disconnectFromServer();
    if (socket.state() != QLocalSocket::UnconnectedState)
        socket.waitForDisconnected(200);
    return true;
}
