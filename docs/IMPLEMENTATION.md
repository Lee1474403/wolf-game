# 改造说明

## 服务端新增结构

### `Room` — `server/include/room.h`

每个四位房间码对应一个 `Room`。它拥有独立的：

- `GameState`：玩家、准备、阶段、牌组、底牌、投票和角色交换结果；
- `mutex`：保护本房间状态；
- `send_mutex`：防止流程线程与客户端线程向同一连接交叉写包；
- `host_socket`：唯一房主；
- `generation` 与 `state_changed`：房间解散/重置时中断旧流程线程；
- 阶段和讨论超时配置。

`resetForNextRoundUnlocked()` 只重置局内状态并取消所有准备，不关闭仍在线玩家的连接。

### `RoomManager` — `server/include/room_manager.h`

`RoomManager` 以 `unordered_map<roomCode, shared_ptr<Room>>` 管理任意数量房间，提供：

- `join()`：校验四位码、自动创建、满员/进行中/房间上限拒绝；
- `leave()`：开局前移除玩家并转移房主；游戏中掉线则解散；
- `dissolve()`：通知并唤醒房间中所有连接；
- `expireInactiveRooms()`：处理超时未准备房间。

不存在旧版的全局 `players/current_phase/is_game_started` 单例状态。

## 关键流程变化

### 连接与加入

TCP accept 后不再立刻占用某个房间。连接必须首先发送 `JOIN|code|nickname`，随后客户端处理线程持有相应 `Room` 的共享引用。这样正在游戏的房间不会阻止新连接创建其他房间。

### 准备与房主开始

`READY|1` 与 `READY|0` 只维护准备状态，不再自动发牌。`START` 会依次校验：

1. 请求者是房主；
2. 游戏尚未开始；
3. 人数至少 7；
4. 当前所有玩家均已准备。

通过后仍调用原有 `prepare_deck()` 牌组规则、相同 shuffle/发牌方式与相同 `game_flow_controller()` 夜晚顺序。

### 游戏逻辑隔离

以下函数仅增加 `Room&`/`shared_ptr<Room>` 上下文参数，规则判断本身保持不变：

- `prepare_deck(Room&, int)`
- `process_night_action(Room&, SOCKET, string)`
- `notify_werewolves_of_teammates(Room&)`
- `notify_minion_of_werewolves(Room&)`
- `notify_insomniacs_of_role(Room&)`
- `announce_vote_result(Room&)`
- `game_flow_controller(shared_ptr<Room>)`

平票优先级和胜负判定仍是原实现：皮匠优先，其次好人，最后狼人；被投出皮匠则皮匠胜，被投出狼人则好人胜，否则狼人胜。

### 掉线与重置

- 等待房间掉线：移除该玩家；房主掉线时转给最早加入者；空房从管理器删除。
- 游戏中掉线：本房间发送友好解散原因并关闭会话；其他房间无影响。
- 正常结束：发送结构化结算，3 秒后清理身份/行动/投票并回到等待准备状态，房间码和连接继续复用。

## 客户端分层

- `NetworkManager`：连接、协议编码/解析和结构化 Qt 信号，不持有界面控件。
- `MainWindow`：视图状态、房间卡、阶段卡、触控操作与对话框。
- `PlayerAvatarWidget`：单个席位视图，负责房主/准备/本人/可选/已选视觉状态。

原 `mainwindow.ui` 已移除，界面由 `MainWindow` 以明确的卡片组件构建；网络原始行只写入后台 `qDebug()`，不再连接到任何用户界面控件。

## 视觉资源

- `client/resources/styles/mobile_theme.qss`：雾蓝夜间主题、圆角卡片、48px 触控按钮和状态属性样式；
- `client/resources/images/avatar_villager.svg`：席位头像；
- 顶部月相字符和阶段卡共同形成产品的视觉识别，不依赖外部图片下载。

## 兼容性

夜间技能指令（`COPY`、`VIEW_PLAYER`、`VIEW_TABLE`、`ROB`、`SWAP`、`DRINK`、`REVEAL`、`PASS`）保持原格式。由于新版必须在连接后选择房间码，并新增房主显式开始，旧版没有房间输入和开始按钮的客户端无法完整操作新版服务器；规则层的人类可读私聊仍由新客户端兼容解析。
