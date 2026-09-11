# 月夜议会 · 一夜终极狼人杀

本仓库已经从单房间演示版改造成可并发运行多房间、可用 Docker 部署、适配 Qt Android 竖屏操作的试玩版本。角色技能、夜晚顺序、牌组规则、投票平票优先级和胜负判定保持原有行为；改动集中在房间状态承载、网络协议、客户端呈现和部署层。

## 工程结构

```text
wolf_game/
├── server/                      # 标准 C++ TCP 服务端
│   ├── include/                 # Room、RoomManager、规则接口
│   ├── src/                     # 网络、流程与原有游戏逻辑
│   └── tests/                   # 房间单元测试与多房间黑盒测试
├── client/                      # Qt 6 Widgets / Android 客户端
│   ├── headers/
│   ├── sources/
│   ├── resources/
│   └── android/
├── docker/
│   ├── server/Dockerfile        # Alpine 多阶段运行时镜像
│   └── android_builder/         # 可选 Qt Android 构建镜像与脚本
├── docs/
├── docker-compose.yml
└── .env.example
```

## 一条命令启动服务端

全新 Linux 主机只需安装 Docker 与 Docker Compose：

```bash
docker compose up -d --build
docker compose logs -f server
```

默认监听 `TCP 8888`。动态改为 `9999`：

```bash
GAME_PORT=9999 docker compose up -d --build
```

PowerShell：

```powershell
$env:GAME_PORT = "9999"
docker compose up -d --build
```

服务端只使用标准 C++ Socket，最终镜像基于 Alpine，仅包含剥离符号后的可执行文件、`libstdc++` 与时区数据；构建目录、源码、`.o`、`.moc` 不会进入运行时镜像。日志写入命名卷 `wolf-game-logs` 的 `/var/log/werewolf/server.log`。

### 环境变量

| 变量 | 默认值 | 说明 |
|---|---:|---|
| `GAME_PORT` | `8888` | TCP 监听与 Compose 映射端口 |
| `GAME_BACKLOG` | `128` | Socket listen backlog |
| `GAME_MAX_CONNECTIONS` | `1024` | 全服并发 TCP 连接上限 |
| `GAME_MAX_ROOMS` | `0` | 房间数上限，`0` 表示不限制 |
| `GAME_PHASE_SECONDS` | `60` | 每个夜间角色阶段时长 |
| `GAME_DISCUSSION_SECONDS` | `60` | 白天讨论时长 |
| `GAME_READY_TIMEOUT_SECONDS` | `300` | 未开局房间无状态变化后的解散时间，`0` 禁用 |
| `GAME_LOG_FILE` | `/var/log/werewolf/server.log` | 日志文件路径 |

同样支持命令行参数，例如：

```bash
./server --port=9999 --max-connections=2048 --max-rooms=0 \
  --phase-seconds=60 --discussion-seconds=60 --ready-timeout-seconds=300
```

## 房间流程

1. 客户端连接后发送四位数字房间码和昵称。
2. 房间不存在时自动创建，首位玩家成为房主；存在且未开始时加入。
3. 房间固定最多 9 人，少于 7 人不能开局。
4. 玩家可准备/取消准备；只有房主能在至少 7 人且全员准备时开始。
5. 每个房间拥有独立玩家、牌组、角色、阶段、技能与投票状态，其他房间不受影响。
6. 开局前房主掉线会自动移交；游戏中任意玩家掉线会安全解散当前房间并给其他玩家提示。
7. 正常结算后保留房间连接，清空局内身份和行动状态，所有玩家重新准备即可开始下一局。

## 本地编译

### 服务端

```bash
cmake -S server -B server/build -DCMAKE_BUILD_TYPE=Release
cmake --build server/build --parallel
ctest --test-dir server/build --output-on-failure
./server/build/server --port=8888
```

### Qt 客户端

需要 Qt 6（Core、Gui、Widgets、Network、Svg）：

```bash
mkdir -p client/build
cd client/build
qmake ../WerewolfClient.pro CONFIG+=release
make -j
```

Android 使用 Qt Creator 选择 `Android arm64-v8a` Kit 即可。应用已固定竖屏，并在加入页校验昵称、端口和四位数字房间码。

## 可选：容器内构建 Android APK

Android 镜像包含 JDK 17、Android SDK 35、NDK r27c、Qt Desktop host tools 和 Qt Android arm64 库。首次构建需要下载较大依赖：

```bash
docker compose --profile android build android-builder
docker compose --profile android run --rm android-builder
```

产物输出到宿主机 `artifacts/WerewolfClient-arm64-v8a.apk`。未提供发布证书时脚本会生成 Android 调试证书并签名；发布构建可在 `.env` 中配置 `ANDROID_KEYSTORE`、别名和密码。该可选镜像依赖 Qt/Android 官方下载服务，未被普通 `docker compose up` 构建或启动。

## 客户端体验

- 加入页不再硬编码 IP，也不再用弹窗索取昵称。
- 房间卡显示房间码、在线人数、准备人数、房主和本人席位。
- 身份卡始终显示本局初始身份；幽灵复制结果作为辅助说明显示。
- 月相阶段卡明确区分夜晚行动、白天讨论、投票和结算。
- 只有当前需要行动的玩家获得可用操作入口；其他玩家显示“等待其他玩家行动…”。
- 游戏提示区只展示整理后的用户消息，不展示原始包、进程号或 `DEBUG` 文本。
- 结算卡显示胜利阵营、全员初始/最终身份和得票数。
- 所有主要按钮最小高度为 48px，适配 Android 触控。

## 验证

服务端单元测试：

```bash
ctest --test-dir server/build --output-on-failure
```

启动一个测试端口后执行多房间黑盒测试：

```bash
GAME_PORT=18888 GAME_PHASE_SECONDS=1 GAME_DISCUSSION_SECONDS=0 \
  ./server/build/server
python3 server/tests/integration_multi_room.py 127.0.0.1 18888
```

该测试会创建两个独立的 7 人房间，验证非房主不能开始、房间 A 开局不影响房间 B、进行中拒绝加入、仍可创建房间 C，以及房间 B 可独立开局。

更多实现位置见 [改造说明](docs/IMPLEMENTATION.md) 与 [通信协议](docs/PROTOCOL.md)。
