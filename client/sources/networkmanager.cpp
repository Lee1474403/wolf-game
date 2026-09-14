#include "networkmanager.h"

#include <QDebug>
#include <QJsonObject>
#include <QNetworkProxy>
#include <QUrl>

namespace {

const QString kServerHost = QStringLiteral("101.201.81.53");
constexpr quint16 kServerPort = 8888;

} // namespace

NetworkManager::NetworkManager(QObject *parent) : QObject(parent), m_socket(new QTcpSocket(this)) {
    m_socket->setProxy(QNetworkProxy::NoProxy);
    connect(m_socket, &QTcpSocket::readyRead, this, &NetworkManager::onReadyRead);
    connect(m_socket, &QTcpSocket::connected, this, &NetworkManager::onConnected);
    connect(m_socket, &QTcpSocket::disconnected, this, &NetworkManager::onDisconnected);
    connect(m_socket, &QTcpSocket::errorOccurred, this, &NetworkManager::onError);
}

NetworkManager::~NetworkManager() {
    disconnectFromServer();
}

void NetworkManager::connectAndJoin(const QString &roomCode, const QString &nickname) {
    m_roomCode = roomCode;
    m_nickname = nickname.trimmed();
    m_joined = false;
    if (m_socket->state() != QAbstractSocket::UnconnectedState) {
        m_socket->abort();
    }
    m_socket->connectToHost(kServerHost, kServerPort);
}

void NetworkManager::disconnectFromServer() {
    m_joined = false;
    if (m_socket && m_socket->state() != QAbstractSocket::UnconnectedState) {
        m_socket->disconnectFromHost();
        if (m_socket->state() != QAbstractSocket::UnconnectedState) {
            m_socket->waitForDisconnected(300);
        }
    }
}

void NetworkManager::setReady(bool ready) {
    sendLine(ready ? "READY|1" : "READY|0");
}

void NetworkManager::startGame() {
    sendLine("START");
}

void NetworkManager::sendVote(int targetId) {
    sendLine(QString("VOTE|%1").arg(targetId));
}

void NetworkManager::sendGameCommand(const QString &command) {
    sendLine(command.trimmed());
}

bool NetworkManager::isConnected() const {
    return m_socket->state() == QAbstractSocket::ConnectedState;
}

void NetworkManager::onReadyRead() {
    while (m_socket->canReadLine()) {
        const QString line = QString::fromUtf8(m_socket->readLine()).trimmed();
        if (!line.isEmpty()) parseMessage(line);
    }
}

void NetworkManager::onConnected() {
    emit connectionChanged(true);
    sendLine(QString("JOIN|%1|%2").arg(m_roomCode, encodeField(m_nickname)));
}

void NetworkManager::onDisconnected() {
    const bool wasJoined = m_joined;
    m_joined = false;
    emit connectionChanged(false);
    if (wasJoined) emit errorOccurred("与服务器的连接已断开");
}

void NetworkManager::onError(QAbstractSocket::SocketError socketError) {
    Q_UNUSED(socketError)
    emit errorOccurred(m_socket->errorString());
}

void NetworkManager::sendLine(const QString &message) {
    if (m_socket->state() != QAbstractSocket::ConnectedState) {
        emit errorOccurred("尚未连接服务器");
        return;
    }
    m_socket->write(message.toUtf8() + '\n');
    m_socket->flush();
    qDebug().noquote() << "[Client -> Server]" << message;
}

QString NetworkManager::encodeField(const QString &value) {
    return QString::fromLatin1(QUrl::toPercentEncoding(value, QByteArray("-_.~")));
}

QString NetworkManager::decodeField(const QString &value) {
    return QString::fromUtf8(QByteArray::fromPercentEncoding(value.toUtf8()));
}

void NetworkManager::parseMessage(const QString &message) {
    qDebug().noquote() << "[Server -> Client]" << message;
    const QStringList fields = message.split('|', Qt::KeepEmptyParts);
    const QString type = fields.value(0);

    if (type == "JOINED" && fields.size() >= 5) {
        m_joined = true;
        emit joinedRoom(fields[1], fields[2].toInt(), fields[3] == "1", fields[4] == "1");
    } else if (type == "PLAYER_ID") {
        emit playerIdChanged(fields.value(1).toInt());
    } else if (type == "JOIN_REJECTED") {
        emit joinRejected(decodeField(fields.value(2, "无法加入房间")));
    } else if (type == "ROOM_STATUS" && fields.size() >= 6) {
        QJsonArray players;
        for (int i = 6; i < fields.size(); ++i) {
            const QStringList playerFields = fields[i].split(',', Qt::KeepEmptyParts);
            if (playerFields.size() < 3) continue;
            QJsonObject player;
            player["id"] = playerFields[0].toInt();
            player["name"] = decodeField(playerFields[1]);
            player["ready"] = playerFields[2] == "1";
            players.append(player);
        }
        emit roomStatusReceived(fields[1], fields[2].toInt(), fields[3].toInt(),
                                fields[4] == "1", fields[5].toInt(), players);
    } else if (type == "READY_ACK") {
        emit readyAcknowledged(fields.value(1) == "1", decodeField(fields.value(2)));
    } else if (type == "ROLE") {
        emit roleAssigned(decodeField(fields.value(1)));
    } else if (type == "COPIED_ROLE") {
        emit copiedRole(decodeField(fields.value(1)));
    } else if (type == "PHASE") {
        emit phaseChanged(decodeField(fields.value(1)), decodeField(fields.value(2)),
                          decodeField(fields.value(3)));
    } else if (type == "ACTION") {
        emit actionRequired(decodeField(fields.value(1)), decodeField(fields.value(2)));
    } else if (type == "WAITING") {
        emit waitingRequired(decodeField(fields.value(1)));
    } else if (type == "NOTICE") {
        emit noticeReceived(decodeField(fields.value(1)), decodeField(fields.value(2)));
    } else if (type == "ERROR") {
        emit errorOccurred(decodeField(fields.value(2, fields.value(1))));
    } else if (type == "GAME_START") {
        emit gameStarted();
    } else if (type == "RESULT" && fields.size() >= 3) {
        QJsonArray identities;
        const QStringList records = fields[2].split(';', Qt::SkipEmptyParts);
        for (const QString &record : records) {
            const QStringList values = record.split(',', Qt::KeepEmptyParts);
            if (values.size() < 5) continue;
            QJsonObject identity;
            identity["id"] = values[0].toInt();
            identity["name"] = decodeField(values[1]);
            identity["initialRole"] = decodeField(values[2]);
            identity["currentRole"] = decodeField(values[3]);
            identity["votes"] = values[4].toInt();
            identities.append(identity);
        }
        emit gameEnded(decodeField(fields[1]), identities);
    } else if (type == "GAME_RESET") {
        emit gameReset(decodeField(fields.value(1)));
    } else if (type == "ROOM_DISBANDED") {
        m_joined = false;
        emit roomDisbanded(decodeField(fields.value(1)));
    } else if (!message.contains("[DEBUG]", Qt::CaseInsensitive)) {
        // 兼容旧版规则层的人类可读私聊消息；不把原始数据包或调试行显示到界面。
        QString friendly = message;
        friendly.remove('\n');
        emit noticeReceived("游戏提示", friendly);
    }
}
