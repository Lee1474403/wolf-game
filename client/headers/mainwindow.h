#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "PlayerAvatarWidget.h"
#include "networkmanager.h"

#include <QJsonArray>
#include <QMainWindow>
#include <QSet>
#include <QVector>

class QFrame;
class QLabel;
class QLineEdit;
class QPushButton;
class QStackedWidget;
class QTextBrowser;
class QWidget;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override = default;

private:
    NetworkManager *m_network = nullptr;
    QStackedWidget *m_pages = nullptr;
    QWidget *m_joinPage = nullptr;
    QWidget *m_gamePage = nullptr;

    QLineEdit *m_nameEdit = nullptr;
    QLineEdit *m_roomCodeEdit = nullptr;
    QLabel *m_joinError = nullptr;
    QPushButton *m_joinButton = nullptr;

    QLabel *m_connectionLabel = nullptr;
    QLabel *m_roomCodeLabel = nullptr;
    QLabel *m_onlineLabel = nullptr;
    QLabel *m_readySummaryLabel = nullptr;
    QLabel *m_roleLabel = nullptr;
    QLabel *m_roleDetailLabel = nullptr;
    QLabel *m_phaseKicker = nullptr;
    QLabel *m_phaseTitle = nullptr;
    QLabel *m_phasePrompt = nullptr;
    QLabel *m_actionPhaseLabel = nullptr;
    QLabel *m_actionHint = nullptr;
    QTextBrowser *m_eventFeed = nullptr;
    QFrame *m_resultCard = nullptr;
    QLabel *m_resultTitle = nullptr;
    QTextBrowser *m_resultDetails = nullptr;
    QPushButton *m_readyButton = nullptr;
    QPushButton *m_startButton = nullptr;
    QPushButton *m_actionButton = nullptr;
    QPushButton *m_leaveButton = nullptr;

    QVector<PlayerAvatarWidget*> m_avatars;
    QSet<int> m_occupiedSeats;
    QList<int> m_selectedTargets;
    QJsonArray m_players;
    int m_myId = -1;
    int m_hostId = -1;
    int m_playerCount = 0;
    int m_readyCount = 0;
    bool m_ready = false;
    bool m_gameStarted = false;
    QString m_roomCode;
    QString m_initialRole;
    QString m_copiedRole;
    QString m_pendingAction;

    void buildUi();
    QWidget *buildJoinPage();
    QWidget *buildGamePage();
    void setupNetworkConnections();
    void setupPlayerGrid(QWidget *container);
    void resetToJoinPage();
    void refreshRoomControls();
    void refreshAvatarActionState();
    void clearPendingAction(const QString &waitingText = "等待其他玩家行动…");
    void appendEvent(const QString &title, const QString &message);
    void showResult(const QString &winner, const QJsonArray &identities);

    void joinRoom();
    void toggleReady();
    void performPendingAction();
    void handleAvatarClicked(int id);
    int choosePlayer(const QString &title, const QString &prompt, bool includeSelf);
    int chooseTableCard(const QString &title, const QString &prompt);
    void performSeerAction();
    void performTroublemakerAction();
};

#endif
