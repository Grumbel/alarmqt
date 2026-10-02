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

    // Send a message to the primary instance (call from secondary)
    static bool sendMessage(const QString& key, const QString& message);

signals:
    void messageReceived(const QString& message);

private slots:
    void onNewConnection();

private:
    QString m_key;
    QLocalServer* m_server = nullptr;
    bool m_isPrimary = false;
};
