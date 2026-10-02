// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QObject>
#include <QLocalServer>
#include <QLocalSocket>
#include <QString>

class SingleInstance : public QObject {
    Q_OBJECT
public:
    explicit SingleInstance(const QString& key, QObject* parent = nullptr);
    ~SingleInstance() override;

    bool isPrimary() const { return m_isPrimary; }

    /** Send a one-line command to the primary.
     *  If @p reply is non-null, wait for a response body (until the server
     *  closes the socket) and store it there. */
    static bool sendMessage(const QString& key, const QString& message,
                            QString* reply = nullptr);

signals:
    /** Emitted when a secondary sent a complete newline-terminated command.
     *  @p socket is still open: write a reply then return; the server closes
     *  it afterward. Fire-and-forget commands may ignore the socket. */
    void messageReceived(const QString& message, QLocalSocket* socket);

private slots:
    void onNewConnection();

private:
    QString m_key;
    QLocalServer* m_server = nullptr;
    bool m_isPrimary = false;
};
