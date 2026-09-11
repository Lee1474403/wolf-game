# Qt Android 客户端

客户端使用 Qt 6 Widgets、Qt Network 与 Qt Svg，支持 Windows 桌面调试和 Android arm64 竖屏运行。

## 功能

- 四位数字房间码输入和校验；
- 自动创建/加入房间、满员与进行中错误提示；
- 房主、在线人数、准备状态实时同步；
- 初始身份、阶段、当前行动和等待状态清晰显示；
- 角色行动和投票使用触控友好的选择控件；
- 游戏结束展示胜利阵营、初始/最终身份和票数；
- 不在界面显示 `qDebug()`、原始包内容或开发按钮。

## 桌面构建

```bash
mkdir build && cd build
qmake ../WerewolfClient.pro CONFIG+=release
make -j
```

## Android 构建

在 Qt Creator 中选择 Qt 6 Android arm64-v8a Kit。也可从仓库根目录运行可选构建容器：

```bash
docker compose --profile android run --rm android-builder
```

APK 输出到 `artifacts/`。详细部署与协议说明见仓库根目录 `README.md` 和 `docs/PROTOCOL.md`。
