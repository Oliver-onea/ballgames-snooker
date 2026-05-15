# Snooker Cross-Platform Port

这是从 `项目2(1).exe` 逆向出的跨平台版本。原 exe 是 MinGW/GCC 构建的 Win64 GDI 程序，没有外部素材；球桌、球、球袋、文字和砖墙背景都由代码绘制。

源码以 MIT License 开源。原始 Windows 二进制样本不纳入仓库，只保留跨平台重实现源码。

## 目录结构

```text
native/sdl/       C++/SDL2 主版本
native/pygame/    Python/Pygame 备用版
web/              浏览器/移动端备用版
scripts/          辅助脚本
docs/             平台说明
build/            本地构建产物
```

## C++/SDL2 本地运行

推荐使用 C++/SDL2 版，运行后打开独立桌面窗口，不依赖浏览器。

当前项目内已经安装了隔离编译环境：

- micromamba: `.tools/bin/micromamba`
- C++/SDL2 环境: `.cpp-env`
- 构建产物: `build/sdl/snooker_sdl`

如果环境被删掉，`make` 会自动调用 `scripts/bootstrap-cpp-env.sh` 重建 Linux/macOS 的本地工具链。

默认构建：

```bash
make
```

运行：

```bash
make run
```

无窗口验证：

```bash
make smoke
```

## Pygame/SDL 备用版

如果暂时不走 C++ 构建，也可以运行 Python/Pygame 版：

```bash
make pygame
```

Windows:

```bat
scripts\run-pygame.bat
```

脚本会自动创建 `.venv` 并安装 `pygame`。之后会直接启动 `native/pygame/snooker_pygame.py`。

## 浏览器备用版

直接用浏览器打开 `web/index.html` 也可以运行。这个版本不依赖 Node、npm 或本地服务器，适合临时预览和移动端。

## 打包

桌面端主线用 `native/sdl`。每个平台在本机打包：Windows 在 Windows 打，macOS 在 macOS 打，Linux 在 Linux 打。当前 Linux 本机可以直接使用：

```bash
make
```

如果要打 Python 备用版：

```bash
. .venv/bin/activate
python -m pip install pyinstaller
pyinstaller --onefile --windowed --name Snooker native/pygame/snooker_pygame.py
```

PyInstaller 不能可靠跨系统交叉打包。

更完整的平台命令见 `docs/PLATFORMS.md`。

## 操作

- 开局移动鼠标预览白球位置，左键确认摆球。
- 桌面端右键开始瞄准，移动鼠标调整方向，左键释放击球。
- 触屏端拖动瞄准，松手击球。
- `F11` 切换全屏，`Esc` 退出本地版。

## 逆向要点

- 游戏窗口标题：`Snooker`，窗口类名：`BrownBricksSnooker`。
- 原版使用 Win32 GDI 绘制：`Ellipse`、`Rectangle`、`LineTo`、`Arc`、`TextOutA`。
- 球桌逻辑尺寸为 `840x440`，内场为 `800x400`，6 个球袋半径为 `15`。
- 白球初始坐标为 `(220, 240)`，红球三角阵列从 `(660, 220)` 开始。
- 摩擦系数为 `0.992`，停止阈值为 `0.034`，玩家击球速度为 `10`，机器人击球速度为 `9`。
- 计分和回合逻辑按原 exe 复刻，包括白球入袋后重置全局比分和重新摆球。
