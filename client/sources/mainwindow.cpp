#include "mainwindow.h"

#include <QAbstractButton>
#include <QDateTime>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QIntValidator>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QScrollArea>
#include <QScroller>
#include <QStackedWidget>
#include <QStyle>
#include <QTextBrowser>
#include <QTextDocument>
#include <QVBoxLayout>
#include <utility>

namespace {

QFrame *makeCard(const QString &objectName, QWidget *parent = nullptr) {
    auto *frame = new QFrame(parent);
    frame->setObjectName(objectName);
    frame->setFrameShape(QFrame::NoFrame);
    return frame;
}

QLabel *makeLabel(const QString &text, const QString &objectName,
                  QWidget *parent = nullptr) {
    auto *label = new QLabel(text, parent);
    label->setObjectName(objectName);
    label->setWordWrap(true);
    return label;
}

} // namespace

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), m_network(new NetworkManager(this)) {
    setWindowTitle("月夜议会 · 一夜终极狼人杀");
    setMinimumSize(360, 640);
    buildUi();
    setupNetworkConnections();
    resetToJoinPage();
}

void MainWindow::buildUi() {
    m_pages = new QStackedWidget(this);
    m_pages->setObjectName("appPages");
    m_joinPage = buildJoinPage();
    m_gamePage = buildGamePage();
    m_pages->addWidget(m_joinPage);
    m_pages->addWidget(m_gamePage);
    setCentralWidget(m_pages);
}

QWidget *MainWindow::buildJoinPage() {
    auto *page = new QWidget(this);
    page->setObjectName("joinPage");
    auto *root = new QVBoxLayout(page);
    root->setContentsMargins(20, 24, 20, 24);
    root->setSpacing(16);
    root->addStretch();

    auto *hero = makeCard("joinHero", page);
    auto *heroLayout = new QVBoxLayout(hero);
    heroLayout->setContentsMargins(22, 24, 22, 24);
    heroLayout->setSpacing(7);
    auto *moon = makeLabel("◐", "moonSignature", hero);
    moon->setAlignment(Qt::AlignCenter);
    auto *title = makeLabel("月夜议会", "joinTitle", hero);
    title->setAlignment(Qt::AlignCenter);
    auto *subtitle = makeLabel("四位房间码，把这一夜交给九张身份牌", "joinSubtitle", hero);
    subtitle->setAlignment(Qt::AlignCenter);
    heroLayout->addWidget(moon);
    heroLayout->addWidget(title);
    heroLayout->addWidget(subtitle);
    root->addWidget(hero);

    auto *formCard = makeCard("formCard", page);
    auto *form = new QVBoxLayout(formCard);
    form->setContentsMargins(18, 18, 18, 18);
    form->setSpacing(9);

    form->addWidget(makeLabel("服务器", "fieldCaption", formCard));
    auto *serverRow = new QHBoxLayout();
    serverRow->setSpacing(8);
    m_hostEdit = new QLineEdit("127.0.0.1", formCard);
    m_hostEdit->setObjectName("hostEdit");
    m_hostEdit->setPlaceholderText("服务器地址");
    m_portEdit = new QLineEdit("8888", formCard);
    m_portEdit->setObjectName("portEdit");
    m_portEdit->setPlaceholderText("端口");
    m_portEdit->setMaximumWidth(96);
    m_portEdit->setValidator(new QIntValidator(1, 65535, m_portEdit));
    m_portEdit->setInputMethodHints(Qt::ImhDigitsOnly);
    serverRow->addWidget(m_hostEdit, 1);
    serverRow->addWidget(m_portEdit);
    form->addLayout(serverRow);

    form->addWidget(makeLabel("你的昵称", "fieldCaption", formCard));
    m_nameEdit = new QLineEdit(formCard);
    m_nameEdit->setObjectName("nameEdit");
    m_nameEdit->setPlaceholderText("2–20 个字符");
    m_nameEdit->setMaxLength(20);
    form->addWidget(m_nameEdit);

    form->addWidget(makeLabel("房间码", "fieldCaption", formCard));
    m_roomCodeEdit = new QLineEdit(formCard);
    m_roomCodeEdit->setObjectName("roomCodeEdit");
    m_roomCodeEdit->setPlaceholderText("0000");
    m_roomCodeEdit->setMaxLength(4);
    m_roomCodeEdit->setAlignment(Qt::AlignCenter);
    m_roomCodeEdit->setInputMethodHints(Qt::ImhDigitsOnly);
    m_roomCodeEdit->setValidator(
        new QRegularExpressionValidator(QRegularExpression("\\d{0,4}"), m_roomCodeEdit));
    form->addWidget(m_roomCodeEdit);

    m_joinError = makeLabel(QString(), "joinError", formCard);
    m_joinError->hide();
    form->addWidget(m_joinError);

    m_joinButton = new QPushButton("进入房间", formCard);
    m_joinButton->setObjectName("primaryButton");
    m_joinButton->setMinimumHeight(54);
    form->addWidget(m_joinButton);
    root->addWidget(formCard);

    auto *footnote = makeLabel("房间不存在时会自动创建，你将成为房主", "joinFootnote", page);
    footnote->setAlignment(Qt::AlignCenter);
    root->addWidget(footnote);
    root->addStretch();

    connect(m_joinButton, &QPushButton::clicked, this, &MainWindow::joinRoom);
    connect(m_roomCodeEdit, &QLineEdit::returnPressed, this, &MainWindow::joinRoom);
    return page;
}

QWidget *MainWindow::buildGamePage() {
    auto *page = new QWidget(this);
    page->setObjectName("gamePage");
    auto *pageLayout = new QVBoxLayout(page);
    pageLayout->setContentsMargins(12, 12, 12, 12);
    pageLayout->setSpacing(10);

    auto *topRow = new QHBoxLayout();
    auto *brand = makeLabel("月夜议会", "compactBrand", page);
    m_connectionLabel = makeLabel("● 已连接", "connectionBadge", page);
    m_connectionLabel->setAlignment(Qt::AlignCenter);
    m_leaveButton = new QPushButton("离开", page);
    m_leaveButton->setObjectName("ghostButton");
    m_leaveButton->setMinimumSize(64, 48);
    topRow->addWidget(brand);
    topRow->addStretch();
    topRow->addWidget(m_connectionLabel);
    topRow->addWidget(m_leaveButton);
    pageLayout->addLayout(topRow);

    auto *scroll = new QScrollArea(page);
    scroll->setObjectName("gameScroll");
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setWidgetResizable(true);
    scroll->viewport()->setStyleSheet("background: transparent;");
    QScroller::grabGesture(scroll->viewport(), QScroller::TouchGesture);

    auto *content = new QWidget(scroll);
    content->setObjectName("gameContent");
    auto *contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(1, 1, 1, 1);
    contentLayout->setSpacing(10);

    auto *roomCard = makeCard("roomCard", content);
    auto *roomLayout = new QHBoxLayout(roomCard);
    roomLayout->setContentsMargins(16, 14, 16, 14);
    auto *roomText = new QVBoxLayout();
    roomText->setSpacing(1);
    roomText->addWidget(makeLabel("房间", "cardKicker", roomCard));
    m_roomCodeLabel = makeLabel("0000", "roomCodeValue", roomCard);
    roomText->addWidget(m_roomCodeLabel);
    roomLayout->addLayout(roomText);
    roomLayout->addStretch();
    auto *metrics = new QVBoxLayout();
    metrics->setSpacing(3);
    m_onlineLabel = makeLabel("0 / 9 在线", "roomMetric", roomCard);
    m_onlineLabel->setAlignment(Qt::AlignRight);
    m_readySummaryLabel = makeLabel("0 人已准备", "roomMetricMuted", roomCard);
    m_readySummaryLabel->setAlignment(Qt::AlignRight);
    metrics->addWidget(m_onlineLabel);
    metrics->addWidget(m_readySummaryLabel);
    roomLayout->addLayout(metrics);
    contentLayout->addWidget(roomCard);

    auto *phaseCard = makeCard("phaseCard", content);
    auto *phaseLayout = new QHBoxLayout(phaseCard);
    phaseLayout->setContentsMargins(16, 17, 16, 17);
    phaseLayout->setSpacing(13);
    auto *phaseMoon = makeLabel("◐", "phaseMoon", phaseCard);
    phaseMoon->setAlignment(Qt::AlignCenter);
    phaseMoon->setFixedSize(54, 54);
    phaseLayout->addWidget(phaseMoon);
    auto *phaseText = new QVBoxLayout();
    phaseText->setSpacing(2);
    m_phaseKicker = makeLabel("房间等待中", "cardKicker", phaseCard);
    m_phaseTitle = makeLabel("等待玩家准备", "phaseTitle", phaseCard);
    m_phasePrompt = makeLabel("至少 7 人且全员准备后，由房主开始游戏", "phasePrompt", phaseCard);
    phaseText->addWidget(m_phaseKicker);
    phaseText->addWidget(m_phaseTitle);
    phaseText->addWidget(m_phasePrompt);
    phaseLayout->addLayout(phaseText, 1);
    contentLayout->addWidget(phaseCard);

    auto *roleCard = makeCard("roleCard", content);
    auto *roleLayout = new QHBoxLayout(roleCard);
    roleLayout->setContentsMargins(16, 13, 16, 13);
    roleLayout->addWidget(makeLabel("当前身份", "cardKicker", roleCard));
    roleLayout->addStretch();
    auto *roleText = new QVBoxLayout();
    roleText->setSpacing(1);
    m_roleLabel = makeLabel("等待发牌", "roleValue", roleCard);
    m_roleLabel->setAlignment(Qt::AlignRight);
    m_roleDetailLabel = makeLabel("身份仅你可见", "roleDetail", roleCard);
    m_roleDetailLabel->setAlignment(Qt::AlignRight);
    roleText->addWidget(m_roleLabel);
    roleText->addWidget(m_roleDetailLabel);
    roleLayout->addLayout(roleText);
    contentLayout->addWidget(roleCard);

    auto *seatCard = makeCard("seatCard", content);
    auto *seatLayout = new QVBoxLayout(seatCard);
    seatLayout->setContentsMargins(10, 12, 10, 12);
    seatLayout->setSpacing(8);
    auto *seatHeader = new QHBoxLayout();
    seatHeader->addWidget(makeLabel("玩家席位", "sectionTitle", seatCard));
    seatHeader->addStretch();
    seatHeader->addWidget(makeLabel("行动时可选择高亮席位", "sectionHint", seatCard));
    seatLayout->addLayout(seatHeader);
    auto *gridContainer = new QWidget(seatCard);
    gridContainer->setObjectName("playerGrid");
    setupPlayerGrid(gridContainer);
    seatLayout->addWidget(gridContainer);
    contentLayout->addWidget(seatCard);

    auto *eventCard = makeCard("eventCard", content);
    auto *eventLayout = new QVBoxLayout(eventCard);
    eventLayout->setContentsMargins(14, 12, 14, 12);
    eventLayout->setSpacing(6);
    eventLayout->addWidget(makeLabel("游戏提示", "sectionTitle", eventCard));
    m_eventFeed = new QTextBrowser(eventCard);
    m_eventFeed->setObjectName("eventFeed");
    m_eventFeed->setMinimumHeight(96);
    m_eventFeed->setMaximumHeight(150);
    m_eventFeed->document()->setMaximumBlockCount(40);
    eventLayout->addWidget(m_eventFeed);
    contentLayout->addWidget(eventCard);

    m_resultCard = makeCard("resultCard", content);
    auto *resultLayout = new QVBoxLayout(m_resultCard);
    resultLayout->setContentsMargins(16, 15, 16, 15);
    resultLayout->setSpacing(6);
    resultLayout->addWidget(makeLabel("本局结算", "cardKicker", m_resultCard));
    m_resultTitle = makeLabel("", "resultTitle", m_resultCard);
    resultLayout->addWidget(m_resultTitle);
    m_resultDetails = new QTextBrowser(m_resultCard);
    m_resultDetails->setObjectName("resultDetails");
    m_resultDetails->setMinimumHeight(190);
    resultLayout->addWidget(m_resultDetails);
    m_resultCard->hide();
    contentLayout->addWidget(m_resultCard);
    contentLayout->addStretch();

    scroll->setWidget(content);
    pageLayout->addWidget(scroll, 1);

    auto *actionDock = makeCard("actionDock", page);
    auto *actionLayout = new QVBoxLayout(actionDock);
    actionLayout->setContentsMargins(12, 10, 12, 12);
    actionLayout->setSpacing(8);
    m_actionHint = makeLabel("等待其他玩家行动…", "actionHint", actionDock);
    m_actionHint->setAlignment(Qt::AlignCenter);
    actionLayout->addWidget(m_actionHint);
    m_actionButton = new QPushButton("执行行动", actionDock);
    m_actionButton->setObjectName("actionButton");
    m_actionButton->setMinimumHeight(52);
    actionLayout->addWidget(m_actionButton);
    auto *lobbyButtons = new QHBoxLayout();
    lobbyButtons->setSpacing(8);
    m_readyButton = new QPushButton("准备", actionDock);
    m_readyButton->setObjectName("readyButton");
    m_readyButton->setMinimumHeight(50);
    m_startButton = new QPushButton("开始游戏", actionDock);
    m_startButton->setObjectName("startButton");
    m_startButton->setMinimumHeight(50);
    lobbyButtons->addWidget(m_readyButton, 1);
    lobbyButtons->addWidget(m_startButton, 1);
    actionLayout->addLayout(lobbyButtons);
    pageLayout->addWidget(actionDock);

    connect(m_leaveButton, &QPushButton::clicked, this, [this] {
        m_network->disconnectFromServer();
        resetToJoinPage();
    });
    connect(m_readyButton, &QPushButton::clicked, this, &MainWindow::toggleReady);
    connect(m_startButton, &QPushButton::clicked, m_network, &NetworkManager::startGame);
    connect(m_actionButton, &QPushButton::clicked, this, &MainWindow::performPendingAction);
    return page;
}

void MainWindow::setupPlayerGrid(QWidget *container) {
    auto *grid = new QGridLayout(container);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setHorizontalSpacing(6);
    grid->setVerticalSpacing(6);
    for (int column = 0; column < 3; ++column) grid->setColumnStretch(column, 1);
    for (int i = 0; i < 9; ++i) {
        auto *avatar = new PlayerAvatarWidget(i, container);
        m_avatars.append(avatar);
        grid->addWidget(avatar, i / 3, i % 3);
        connect(avatar, &PlayerAvatarWidget::playerClicked, this,
                &MainWindow::handleAvatarClicked);
    }
}

void MainWindow::setupNetworkConnections() {
    connect(m_network, &NetworkManager::connectionChanged, this, [this](bool connected) {
        if (m_connectionLabel) {
            m_connectionLabel->setText(connected ? "● 已连接" : "● 已断开");
            m_connectionLabel->setProperty("connected", connected);
            m_connectionLabel->style()->unpolish(m_connectionLabel);
            m_connectionLabel->style()->polish(m_connectionLabel);
        }
    });
    connect(m_network, &NetworkManager::joinedRoom, this,
            [this](const QString &code, int playerId, bool isHost, bool created) {
        m_roomCode = code;
        m_myId = playerId;
        m_hostId = isHost ? playerId : -1;
        m_joinButton->setEnabled(true);
        m_joinButton->setText("进入房间");
        m_pages->setCurrentWidget(m_gamePage);
        m_roomCodeLabel->setText(code);
        appendEvent(created ? "房间已创建" : "已加入房间",
                    isHost ? "你是房主，集齐玩家并全员准备后即可开始" :
                             "等待房主在全员准备后开始游戏");
    });
    connect(m_network, &NetworkManager::joinRejected, this, [this](const QString &reason) {
        m_joinButton->setEnabled(true);
        m_joinButton->setText("进入房间");
        m_joinError->setText(reason);
        m_joinError->show();
        m_network->disconnectFromServer();
    });
    connect(m_network, &NetworkManager::playerIdChanged, this, [this](int playerId) {
        m_myId = playerId;
    });
    connect(m_network, &NetworkManager::roomStatusReceived, this,
            [this](const QString &code, int count, int capacity, bool started,
                   int hostId, const QJsonArray &players) {
        m_roomCode = code;
        m_playerCount = count;
        m_hostId = hostId;
        m_gameStarted = started;
        m_players = players;
        m_readyCount = 0;
        m_occupiedSeats.clear();
        for (PlayerAvatarWidget *avatar : std::as_const(m_avatars)) avatar->setEmpty();

        for (const QJsonValue &value : players) {
            const QJsonObject player = value.toObject();
            const int id = player["id"].toInt();
            const int index = id - 1;
            if (index < 0 || index >= m_avatars.size()) continue;
            const bool ready = player["ready"].toBool();
            m_occupiedSeats.insert(index);
            if (ready) ++m_readyCount;
            if (id == m_myId) m_ready = ready;
            m_avatars[index]->setPlayerInfo(index, player["name"].toString(), ready);
            m_avatars[index]->setCurrentPlayer(id == m_myId);
            m_avatars[index]->setHost(id == hostId);
        }
        m_roomCodeLabel->setText(code);
        m_onlineLabel->setText(QString("%1 / %2 在线").arg(count).arg(capacity));
        m_readySummaryLabel->setText(QString("%1 人已准备").arg(m_readyCount));
        refreshRoomControls();
        refreshAvatarActionState();
    });
    connect(m_network, &NetworkManager::readyAcknowledged, this,
            [this](bool ready, const QString &message) {
        m_ready = ready;
        appendEvent("准备状态", message);
        refreshRoomControls();
    });
    connect(m_network, &NetworkManager::roleAssigned, this, [this](const QString &role) {
        m_initialRole = role;
        m_copiedRole.clear();
        m_roleLabel->setText(role);
        m_roleDetailLabel->setText("初始分配身份 · 仅你可见");
    });
    connect(m_network, &NetworkManager::copiedRole, this, [this](const QString &role) {
        m_copiedRole = role;
        m_roleDetailLabel->setText("幽灵已复制：" + role);
        appendEvent("幽灵复制完成", "你复制了“" + role + "”的能力");
    });
    connect(m_network, &NetworkManager::phaseChanged, this,
            [this](const QString &phase, const QString &step, const QString &prompt) {
        clearPendingAction();
        if (phase == "NIGHT") {
            m_phaseKicker->setText("月相 · " + step);
            m_phaseTitle->setText("夜晚行动阶段");
            m_actionHint->setText("等待其他玩家行动…");
        } else if (phase == "DAY" && step == "DISCUSSION") {
            m_phaseKicker->setText("天光 · 自由讨论");
            m_phaseTitle->setText("白天讨论阶段");
            m_actionHint->setText("请交流线索，投票尚未开始");
        } else if (phase == "DAY" && step == "VOTE") {
            m_phaseKicker->setText("天光 · 最终投票");
            m_phaseTitle->setText("白天投票阶段");
        } else {
            m_phaseKicker->setText("本局结算");
            m_phaseTitle->setText("正在公布结果");
        }
        m_phasePrompt->setText(prompt);
    });
    connect(m_network, &NetworkManager::actionRequired, this,
            [this](const QString &action, const QString &prompt) {
        m_pendingAction = action;
        m_selectedTargets.clear();
        m_actionHint->setText(prompt);
        m_actionButton->setText(action == "VOTE" ? "选择并提交投票" : "执行我的行动");
        m_actionButton->setEnabled(action != "TROUBLEMAKER");
        refreshAvatarActionState();
    });
    connect(m_network, &NetworkManager::waitingRequired, this,
            [this](const QString &prompt) { clearPendingAction(prompt); });
    connect(m_network, &NetworkManager::noticeReceived, this,
            &MainWindow::appendEvent);
    connect(m_network, &NetworkManager::gameStarted, this, [this] {
        m_gameStarted = true;
        m_resultCard->hide();
        m_phaseKicker->setText("月相 · 入夜");
        m_phaseTitle->setText("游戏开始");
        m_phasePrompt->setText("身份已经分配，请等待夜晚指引");
        refreshRoomControls();
    });
    connect(m_network, &NetworkManager::gameEnded, this, &MainWindow::showResult);
    connect(m_network, &NetworkManager::gameReset, this, [this](const QString &message) {
        m_gameStarted = false;
        m_ready = false;
        m_initialRole.clear();
        m_copiedRole.clear();
        m_roleLabel->setText("等待发牌");
        m_roleDetailLabel->setText("身份仅你可见");
        m_phaseKicker->setText("下一局");
        m_phaseTitle->setText("房间已重新开放");
        m_phasePrompt->setText(message);
        clearPendingAction("准备好后可以开始下一局");
        refreshRoomControls();
    });
    connect(m_network, &NetworkManager::roomDisbanded, this, [this](const QString &message) {
        QMessageBox::information(this, "房间已解散", message);
        m_network->disconnectFromServer();
        resetToJoinPage();
    });
    connect(m_network, &NetworkManager::errorOccurred, this, [this](const QString &message) {
        if (m_pages->currentWidget() == m_joinPage) {
            m_joinError->setText(message);
            m_joinError->show();
            m_joinButton->setEnabled(true);
            m_joinButton->setText("进入房间");
        } else {
            appendEvent("操作未完成", message);
            if (!m_pendingAction.isEmpty()) {
                m_actionButton->setEnabled(m_pendingAction != "TROUBLEMAKER" ||
                                           m_selectedTargets.size() == 2);
                m_actionHint->setText("请调整选择后重试");
            }
        }
    });
}

void MainWindow::resetToJoinPage() {
    m_myId = -1;
    m_hostId = -1;
    m_playerCount = 0;
    m_readyCount = 0;
    m_ready = false;
    m_gameStarted = false;
    m_roomCode.clear();
    m_initialRole.clear();
    m_copiedRole.clear();
    m_players = QJsonArray();
    m_eventFeed->clear();
    m_resultCard->hide();
    clearPendingAction();
    for (PlayerAvatarWidget *avatar : std::as_const(m_avatars)) avatar->setEmpty();
    m_joinError->hide();
    m_joinButton->setEnabled(true);
    m_joinButton->setText("进入房间");
    m_pages->setCurrentWidget(m_joinPage);
}

void MainWindow::joinRoom() {
    const QString host = m_hostEdit->text().trimmed();
    const QString name = m_nameEdit->text().trimmed();
    const QString code = m_roomCodeEdit->text();
    bool portOk = false;
    const int port = m_portEdit->text().toInt(&portOk);

    QString error;
    if (host.isEmpty()) error = "请输入服务器地址";
    else if (!portOk || port < 1 || port > 65535) error = "端口必须在 1–65535 之间";
    else if (name.isEmpty()) error = "请输入你的昵称";
    else if (!QRegularExpression("^\\d{4}$").match(code).hasMatch())
        error = "房间码必须是四位数字";

    if (!error.isEmpty()) {
        m_joinError->setText(error);
        m_joinError->show();
        return;
    }
    m_joinError->hide();
    m_joinButton->setEnabled(false);
    m_joinButton->setText("正在进入…");
    m_network->connectAndJoin(host, static_cast<quint16>(port), code, name);
}

void MainWindow::toggleReady() {
    if (!m_gameStarted) m_network->setReady(!m_ready);
}

void MainWindow::refreshRoomControls() {
    const bool isHost = m_myId > 0 && m_myId == m_hostId;
    m_readyButton->setVisible(!m_gameStarted);
    m_readyButton->setEnabled(!m_gameStarted);
    m_readyButton->setText(m_ready ? "取消准备" : "准备");
    m_readyButton->setProperty("ready", m_ready);
    m_readyButton->style()->unpolish(m_readyButton);
    m_readyButton->style()->polish(m_readyButton);

    m_startButton->setVisible(isHost && !m_gameStarted);
    m_startButton->setEnabled(isHost && !m_gameStarted && m_playerCount >= 7 &&
                              m_readyCount == m_playerCount);
    if (!m_gameStarted) {
        m_phaseKicker->setText(isHost ? "房主控制" : "房间等待中");
        m_phaseTitle->setText(m_playerCount < 7 ? "等待更多玩家" :
                              (m_readyCount == m_playerCount ? "全员已准备" : "等待玩家准备"));
        m_phasePrompt->setText(isHost ? "至少 7 人且全员准备后，你可以开始游戏" :
                                      "准备完成后，等待房主开始游戏");
    }
}

void MainWindow::refreshAvatarActionState() {
    const bool selecting = m_pendingAction == "TROUBLEMAKER" && m_gameStarted;
    for (int i = 0; i < m_avatars.size(); ++i) {
        const bool enabled = selecting && m_occupiedSeats.contains(i) && i != m_myId - 1;
        m_avatars[i]->setActionEnabled(enabled);
    }
}

void MainWindow::clearPendingAction(const QString &waitingText) {
    m_pendingAction.clear();
    for (int id : std::as_const(m_selectedTargets)) {
        if (id >= 0 && id < m_avatars.size()) m_avatars[id]->setSelected(false);
    }
    m_selectedTargets.clear();
    m_actionButton->setText("执行行动");
    m_actionButton->setEnabled(false);
    m_actionHint->setText(waitingText);
    refreshAvatarActionState();
}

void MainWindow::appendEvent(const QString &title, const QString &message) {
    const QString time = QDateTime::currentDateTime().toString("HH:mm");
    m_eventFeed->append(QString("<p><span class='eventTime'>%1</span> "
                                "<b>%2</b><br><span class='eventText'>%3</span></p>")
                            .arg(time.toHtmlEscaped(), title.toHtmlEscaped(),
                                 message.toHtmlEscaped()));
}

void MainWindow::showResult(const QString &winner, const QJsonArray &identities) {
    clearPendingAction("本局已结束");
    m_phaseKicker->setText("终局");
    m_phaseTitle->setText("身份已经公开");
    m_phasePrompt->setText(winner);
    m_resultTitle->setText(winner);

    QString html = "<table width='100%' cellspacing='0' cellpadding='5'>";
    html += "<tr><th align='left'>玩家</th><th align='left'>初始</th>"
            "<th align='left'>最终</th><th align='right'>票</th></tr>";
    for (const QJsonValue &value : identities) {
        const QJsonObject item = value.toObject();
        html += QString("<tr><td>%1号 %2</td><td>%3</td><td>%4</td><td align='right'>%5</td></tr>")
                    .arg(item["id"].toInt())
                    .arg(item["name"].toString().toHtmlEscaped(),
                         item["initialRole"].toString().toHtmlEscaped(),
                         item["currentRole"].toString().toHtmlEscaped())
                    .arg(item["votes"].toInt());
    }
    html += "</table>";
    m_resultDetails->setHtml(html);
    m_resultCard->show();
    appendEvent("本局结果", winner);
}

int MainWindow::choosePlayer(const QString &title, const QString &prompt, bool includeSelf) {
    QStringList options;
    for (const QJsonValue &value : m_players) {
        const QJsonObject player = value.toObject();
        const int id = player["id"].toInt();
        if (!includeSelf && id == m_myId) continue;
        QString label = QString("%1号 · %2").arg(id).arg(player["name"].toString());
        if (id == m_myId) label += "（我）";
        options << label;
    }
    if (options.isEmpty()) return -1;
    bool ok = false;
    const QString choice = QInputDialog::getItem(this, title, prompt, options, 0, false, &ok);
    if (!ok || choice.isEmpty()) return -1;
    return choice.section("号", 0, 0).toInt();
}

int MainWindow::chooseTableCard(const QString &title, const QString &prompt) {
    bool ok = false;
    const QString choice = QInputDialog::getItem(this, title, prompt,
                                                 {"底牌 1", "底牌 2", "底牌 3"},
                                                 0, false, &ok);
    return ok ? choice.right(1).toInt() : -1;
}

void MainWindow::performPendingAction() {
    QString command;
    if (m_pendingAction == "COPY") {
        const int target = choosePlayer("幽灵行动", "选择要复制的玩家", false);
        if (target > 0) command = QString("COPY %1").arg(target);
    } else if (m_pendingAction == "SEER") {
        performSeerAction();
        return;
    } else if (m_pendingAction == "LONE_WOLF") {
        const int card = chooseTableCard("独狼行动", "选择查看一张底牌");
        if (card > 0) command = QString("VIEW_TABLE %1").arg(card);
    } else if (m_pendingAction == "ROBBER") {
        QMessageBox choice(this);
        choice.setWindowTitle("强盗行动");
        choice.setText("交换一名玩家的牌，或保留当前身份");
        QAbstractButton *swapButton = choice.addButton("选择玩家", QMessageBox::AcceptRole);
        QAbstractButton *passButton = choice.addButton("跳过行动", QMessageBox::DestructiveRole);
        choice.addButton("返回", QMessageBox::RejectRole);
        choice.exec();
        if (choice.clickedButton() == passButton) command = "PASS";
        else if (choice.clickedButton() == swapButton) {
            const int target = choosePlayer("强盗行动", "选择交换目标", false);
            if (target > 0) command = QString("ROB %1").arg(target);
        }
    } else if (m_pendingAction == "DRUNK") {
        const int card = chooseTableCard("酒鬼行动", "选择一张底牌交换（你不会看到新身份）");
        if (card > 0) command = QString("DRINK %1").arg(card);
    } else if (m_pendingAction == "REVEALER") {
        const int target = choosePlayer("揭示者行动", "选择查看目标", false);
        if (target > 0) command = QString("REVEAL %1").arg(target);
    } else if (m_pendingAction == "VOTE") {
        const int target = choosePlayer("最终投票", "选择你认为应该被投出的玩家", true);
        if (target > 0) {
            m_network->sendVote(target);
            m_actionButton->setEnabled(false);
            m_actionHint->setText("选票已提交，等待其他玩家…");
        }
        return;
    } else if (m_pendingAction == "TROUBLEMAKER") {
        performTroublemakerAction();
        return;
    }

    if (!command.isEmpty()) {
        m_network->sendGameCommand(command);
        m_actionButton->setEnabled(false);
        m_actionHint->setText("行动已提交，等待阶段结束…");
        refreshAvatarActionState();
    }
}

void MainWindow::performSeerAction() {
    QMessageBox choice(this);
    choice.setWindowTitle("预言家行动");
    choice.setText("选择一种查验方式");
    QAbstractButton *playerButton = choice.addButton("查看一名玩家", QMessageBox::AcceptRole);
    QAbstractButton *tableButton = choice.addButton("查看两张底牌", QMessageBox::ActionRole);
    choice.addButton("返回", QMessageBox::RejectRole);
    choice.exec();

    QString command;
    if (choice.clickedButton() == playerButton) {
        const int target = choosePlayer("预言家行动", "选择要查验的玩家", false);
        if (target > 0) command = QString("VIEW_PLAYER %1").arg(target);
    } else if (choice.clickedButton() == tableButton) {
        bool ok = false;
        const QString cards = QInputDialog::getItem(this, "预言家行动", "选择两张底牌",
                                                    {"底牌 1 和 2", "底牌 1 和 3", "底牌 2 和 3"},
                                                    0, false, &ok);
        if (ok) {
            if (cards.contains("1 和 2")) command = "VIEW_TABLE 1 2";
            else if (cards.contains("1 和 3")) command = "VIEW_TABLE 1 3";
            else command = "VIEW_TABLE 2 3";
        }
    }
    if (!command.isEmpty()) {
        m_network->sendGameCommand(command);
        m_actionButton->setEnabled(false);
        m_actionHint->setText("查验请求已提交…");
    }
}

void MainWindow::handleAvatarClicked(int id) {
    if (m_pendingAction != "TROUBLEMAKER" || id == m_myId - 1 ||
        !m_occupiedSeats.contains(id)) return;
    if (m_selectedTargets.contains(id)) {
        m_selectedTargets.removeAll(id);
        m_avatars[id]->setSelected(false);
    } else if (m_selectedTargets.size() < 2) {
        m_selectedTargets.append(id);
        m_avatars[id]->setSelected(true);
    }
    m_actionButton->setEnabled(m_selectedTargets.size() == 2);
    m_actionButton->setText(QString("确认交换（%1/2）").arg(m_selectedTargets.size()));
    m_actionHint->setText(m_selectedTargets.size() == 2 ?
                              "已选两名玩家，确认后提交交换" : "请点击两名其他玩家的席位");
}

void MainWindow::performTroublemakerAction() {
    if (m_selectedTargets.size() != 2) return;
    const int first = m_selectedTargets[0] + 1;
    const int second = m_selectedTargets[1] + 1;
    if (QMessageBox::question(this, "确认交换",
                              QString("确认交换 %1 号与 %2 号玩家的身份牌？")
                                  .arg(first).arg(second)) != QMessageBox::Yes) return;
    m_network->sendGameCommand(QString("SWAP %1 %2").arg(first).arg(second));
    m_actionButton->setEnabled(false);
    m_actionHint->setText("交换请求已提交…");
    for (int id : std::as_const(m_selectedTargets)) m_avatars[id]->setSelected(false);
    refreshAvatarActionState();
}
