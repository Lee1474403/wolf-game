# TCP 通信协议

协议为 UTF-8、逐行文本，每条消息以 `\n` 结束。结构化消息使用 `|` 分隔字段，字段中的 `%`、`|`、`,`、`;` 和换行使用百分号编码。

## 客户端 → 服务端

| 消息 | 说明 |
|---|---|
| `JOIN|1234|昵称` | 连接后的第一条消息；房间不存在时创建 |
| `READY|1` | 准备 |
| `READY|0` | 取消准备 |
| `START` | 房主请求开始 |
| `VOTE|3` | 投给 3 号玩家 |
| `COPY 3` | 幽灵复制 3 号玩家 |
| `VIEW_PLAYER 3` | 预言家查看玩家 |
| `VIEW_TABLE 1 2` | 预言家查看两张底牌 |
| `VIEW_TABLE 2` | 独狼查看一张底牌 |
| `ROB 3` | 强盗交换 |
| `SWAP 2 5` | 捣蛋鬼交换两人 |
| `DRINK 2` | 酒鬼与底牌交换 |
| `REVEAL 3` | 揭示者查看玩家 |
| `PASS` | 强盗跳过 |

## 服务端 → 客户端

### 房间

```text
JOINED|<roomCode>|<myPlayerId>|<isHost>|<created>
PLAYER_ID|<newPlayerId>
JOIN_REJECTED|<errorCode>|<message>
ROOM_STATUS|<code>|<count>|9|<started>|<hostId>|<id>,<name>,<ready>|...
READY_ACK|<ready>|<message>
GAME_START|<roomCode>
GAME_RESET|<message>
ROOM_DISBANDED|<message>
```

`JOIN_REJECTED` 常见错误码：`INVALID_CODE`、`INVALID_NAME`、`ROOM_FULL`、`GAME_IN_PROGRESS`、`ROOM_LIMIT`、`SERVER_BUSY`。

### 对局

```text
ROLE|<initialRole>
COPIED_ROLE|<role>
PHASE|<NIGHT/DAY/ENDING>|<step>|<publicPrompt>
ACTION|<actionType>|<privatePrompt>
WAITING|<prompt>
NOTICE|<title>|<message>
ERROR|<code>|<message>
```

`actionType` 包括：`COPY`、`SEER`、`LONE_WOLF`、`ROBBER`、`TROUBLEMAKER`、`DRUNK`、`REVEALER`、`VOTE`。

### 结算

```text
RESULT|<winner>|<id>,<name>,<initialRole>,<currentRole>,<votes>;<nextPlayer>...
```

客户端用该消息显示胜利阵营、所有玩家的初始/最终身份和得票数。
