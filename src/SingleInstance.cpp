// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "SingleInstance.h"

#include <QLocalSocket>

#include <memory>

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
    m_isPrimary = true;
    if (!m_server->listen(m_key)) {
        // Still run as primary, but later instances cannot reach us.
        qWarning("SingleInstance: cannot listen on %s: %s",
                 qPrintable(m_key), qPrintable(m_server->errorString()));
        return;
    }
    connect(m_server, &QLocalServer::newConnection, this, &SingleInstance::onNewConnection);
}

SingleInstance::~SingleInstance() {
    if (m_server)
        m_server->close();
}

void SingleInstance::onNewConnection() {
    while (auto* sock = m_server->nextPendingConnection()) {
        // A message is terminated by '\n' or by the client disconnecting; it
        // may arrive in several chunks, so accumulate until one of those.
        auto buffer = std::make_shared<QByteArray>();
        auto handled = std::make_shared<bool>(false);
        auto finish = [this, sock, buffer, handled]() {
            if (*handled)
                return;
            *handled = true;
            const QString message = QString::fromUtf8(*buffer).trimmed();
            sock->disconnectFromServer();
            sock->deleteLater();
            if (!message.isEmpty())
                emit messageReceived(message);
        };
        auto consume = [sock, buffer, finish]() {
            buffer->append(sock->readAll());
            if (buffer->contains('\n'))
                finish();
        };

        connect(sock, &QLocalSocket::readyRead, this, consume);
        connect(sock, &QLocalSocket::disconnected, this, [sock, buffer, finish]() {
            buffer->append(sock->readAll());
            finish();
        });

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
    // '\n' terminates the message, so it must not appear inside it.
    QString line = message;
    line.replace(QLatin1Char('\n'), QLatin1Char(' '));
    const QByteArray payload = line.toUtf8() + '\n';
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
