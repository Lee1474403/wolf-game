#include "PlayerAvatarWidget.h"
#include <QPixmap>
#include <QSizePolicy>
#include <QStyle>
#include <QStringList>


PlayerAvatarWidget::PlayerAvatarWidget(int positionId, QWidget *parent)
    : QWidget(parent), m_id(positionId), m_isEmpty(true)
{
    setObjectName("playerCard");
    setProperty("selected", false);
    setProperty("currentPlayer", false);
    setProperty("actionEnabled", false);
    setProperty("occupied", false);
    setProperty("actionEnabled", false);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    setMinimumSize(92, 122);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(5, 7, 5, 7);
    layout->setSpacing(3);

    lblAvatar = new QLabel(this);
    lblAvatar->setObjectName("avatarImage");
    lblAvatar->setFixedSize(58, 58);
    lblAvatar->setAlignment(Qt::AlignCenter);
    QPixmap avatarPixmap(":/images/avatar_villager.svg");
    if (!avatarPixmap.isNull()) {
        lblAvatar->setPixmap(avatarPixmap.scaled(54, 54, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    } else {
        lblAvatar->setText("👤");
    }

    lblId = new QLabel(QString("席位 %1").arg(m_id + 1, 2, 10, QLatin1Char('0')), this);
    lblId->setObjectName("seatNumber");
    lblId->setAlignment(Qt::AlignCenter);

    lblName = new QLabel("虚位以待", this);
    lblName->setObjectName("playerName");
    lblName->setAlignment(Qt::AlignCenter);
    lblName->setTextInteractionFlags(Qt::NoTextInteraction);

    lblStatus = new QLabel("", this);
    lblStatus->setObjectName("playerStatus");
    lblStatus->setAlignment(Qt::AlignCenter);

    layout->addWidget(lblAvatar, 0, Qt::AlignCenter);
    layout->addWidget(lblId);
    layout->addWidget(lblName);
    layout->addWidget(lblStatus);

    setEmpty();
}

void PlayerAvatarWidget::setPlayerInfo(int id, const QString &name, bool ready) {
    m_id = id;
    m_isEmpty = false;
    setProperty("occupied", true);
    lblAvatar->setEnabled(true);
    lblId->setText(QString("席位 %1").arg(m_id + 1, 2, 10, QLatin1Char('0')));
    const QString displayName = name.size() > 8 ? name.left(7) + "…" : name;
    lblName->setText(displayName);
    lblName->setToolTip(name);
    m_isReady = ready;
    refreshStatus();
    refreshStyle();
}

void PlayerAvatarWidget::setEmpty() {
    m_isEmpty = true;
    m_isReady = false;
    m_isHost = false;
    m_actionEnabled = false;
    setProperty("occupied", false);
    setProperty("selected", false);
    setProperty("currentPlayer", false);
    lblName->setText("虚位以待");
    lblName->setToolTip(QString());
    lblStatus->setText("");
    lblStatus->setStyleSheet(QString());
    lblAvatar->setEnabled(false);
    refreshStyle();
}

void PlayerAvatarWidget::setEmpty(bool empty) {
    m_isEmpty = empty;
    if (empty) {
        this->setEmpty(); // 调用你之前写的那个设置虚位样式的函数
    }
    // 保持可见性，或者根据你的需求隐藏
    this->setVisible(true);
}

void PlayerAvatarWidget::clearInfo() {
    setEmpty();
}

void PlayerAvatarWidget::setSelected(bool selected) {
    setProperty("selected", selected);
    refreshStyle();
}

void PlayerAvatarWidget::setCurrentPlayer(bool currentPlayer) {
    setProperty("currentPlayer", currentPlayer);
    refreshStyle();
}

void PlayerAvatarWidget::setHost(bool host) {
    m_isHost = host;
    refreshStatus();
    refreshStyle();
}

void PlayerAvatarWidget::setActionEnabled(bool enabled) {
    m_actionEnabled = enabled;
    setProperty("actionEnabled", enabled);
    refreshStyle();
}

void PlayerAvatarWidget::refreshStatus() {
    if (m_isEmpty) {
        lblStatus->clear();
        return;
    }
    QStringList badges;
    if (m_isHost) badges << "房主";
    badges << (m_isReady ? "已准备" : "未准备");
    lblStatus->setText(badges.join(" · "));
    lblStatus->setProperty("ready", m_isReady);
    lblStatus->style()->unpolish(lblStatus);
    lblStatus->style()->polish(lblStatus);
}

void PlayerAvatarWidget::refreshStyle() {
    style()->unpolish(this);
    style()->polish(this);
    update();
}

void PlayerAvatarWidget::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton && !m_isEmpty && m_actionEnabled) {
        emit playerClicked(m_id);
    }
    QWidget::mousePressEvent(event);
}
