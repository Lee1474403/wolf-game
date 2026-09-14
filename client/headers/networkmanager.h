#ifndef NETWORKMANAGER_H
#define NETWORKMANAGER_H

#include <QJsonArray>
#include <QObject>
#include <QTcpSocket>

class NetworkManager : public QObject {
    Q_OBJECT

public:
    explicit NetworkManager(QObject *parent = nullptr);
    ~NetworkManager() override;

    void connectAndJoin(const QString &roomCode, const QString &nickname);
    void disconnectFromServer();
    void setReady(bool ready);
    void startGame();
    void sendVote(int targetId);
    void sendGameCommand(const QString &command);
    bool isConnected() const;

signals:
    void connectionChanged(bool connected);
    void joinedRoom(const QString &roomCode, int playerId, bool isHost, bool created);
    void playerIdChanged(int playerId);
    void joinRejected(const QString &reason);
    void roomStatusReceived(const QString &roomCode, int count, int capacity,
                            bool gameStarted, int hostId, const QJsonArray &players);
    void readyAcknowledged(bool ready, const QString &message);
    void roleAssigned(const QString &role);
    void copiedRole(const QString &role);
    void phaseChanged(const QString &phase, const QString &step, const QString &prompt);
    void actionRequired(const QString &action, const QString &prompt);
    void waitingRequired(const QString &prompt);
    void noticeReceived(const QString &title, const QString &message);
    void gameStarted();
    void gameEnded(const QString &winner, const QJsonArray &identities);
    void gameReset(const QString &message);
    void roomDisbanded(const QString &message);
    void errorOccurred(const QString &message);

private slots:
    void onReadyRead();
    void onConnected();
    void onDisconnected();
    void onError(QAbstractSocket::SocketError socketError);

private:
    QTcpSocket *m_socket = nullptr;
    QString m_roomCode;
    QString m_nickname;
    bool m_joined = false;

    void parseMessage(const QString &message);
    void sendLine(const QString &message);
    static QString encodeField(const QString &value);
    static QString decodeField(const QString &value);
};

#endif
