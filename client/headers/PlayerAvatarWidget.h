#ifndef PLAYERAVATARWIDGET_H
#define PLAYERAVATARWIDGET_H

#include <QWidget>
#include <QLabel>
#include <QVBoxLayout>
#include <QMouseEvent>

class PlayerAvatarWidget : public QWidget {
    Q_OBJECT
public:
    explicit PlayerAvatarWidget(int positionId, QWidget *parent = nullptr);

    // 设置玩家信息：如果name为空则显示为“虚位以待”
    void setPlayerInfo(int id, const QString &name, bool ready);
    void setEmpty();
    bool isEmpty() const { return m_isEmpty; }
    // 修改为支持 bool 参数
    void setEmpty(bool empty);
    // 新增清理函数
    void clearInfo();
    void setSelected(bool selected);
    void setCurrentPlayer(bool currentPlayer);
    void setHost(bool host);
    void setActionEnabled(bool enabled);

signals:
    void playerClicked(int id); // 被点击时发送信号给MainWindow

protected:
    void mousePressEvent(QMouseEvent *event) override;

private:
    int m_id;
    bool m_isEmpty;
    bool m_isReady = false;
    bool m_isHost = false;
    bool m_actionEnabled = false;
    QLabel *lblAvatar;
    QLabel *lblId;
    QLabel *lblName;
    QLabel *lblStatus;

    void refreshStyle();
    void refreshStatus();

};

#endif
