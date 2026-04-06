# ProcessGuard · 进程守护工具

纯 C++ / Win32 API 实现的轻量进程守护工具，单文件 exe，无任何运行时依赖，体积约 350KB。

---

## 功能特性

| 功能 | 说明 |
|------|------|
| 拖拽/浏览添加 | 将 `.exe` 或 `.lnk` 拖入窗口，或点击「添加进程」浏览选择 |
| 自动重启 | 进程异常退出后自动重启，记录重启次数 |
| 实时监控 | 每 2 秒刷新 CPU / 内存 / 重启次数 |
| 未响应检测 | 检测 UI 进程是否挂起（SendMessageTimeout 500ms） |
| 自我守护 | 启动时自动 spawn 隐藏 watchdog 子进程保护自身 |
| 配置持久化 | 重启后自动恢复上次守护列表（JSON 格式） |
| 单独启停 | 点击复选框切换单个进程的守护开关（默认添加后不自动守护） |
| 最小化托盘 | 关闭窗口最小化到系统托盘，右键菜单操作 |
| 单实例 | 重复启动时自动激活已有窗口 |

---

## 快速使用

1. 双击运行 `ProcessGuard.exe`
2. 将要守护的 `.exe` 或快捷方式 `.lnk` 拖入窗口，或点击「＋ 添加进程」
3. 勾选右下角复选框开启守护（默认添加后处于禁用状态）
4. 关闭窗口 → 最小化到托盘继续后台运行
5. 托盘图标右键 → 「退出守护」才会真正退出

---

## 自我守护原理

```
ProcessGuard.exe（主进程）
    │
    └─ 启动 ──► Watchdog 子进程（隐藏，无窗口）
                    │
                    └─ WaitForSingleObject(主进程句柄)
                                │
                    主进程崩溃 ──► CreateProcess 重启主进程
```

Watchdog 极度轻量，仅持有一个内核等待，不轮询，CPU 占用为零。主进程正常退出时会先终止 Watchdog，不会误重启。

---

## 配置文件

保存于 `%APPDATA%\ProcessGuard\config.json`，可手动编辑：

```json
{
  "processes": [
    {
      "exePath": "C:\\Tools\\myapp.exe",
      "name": "myapp.exe",
      "enabled": true,
      "restartDelayMs": 3000,
      "maxRestarts": 0
    }
  ]
}
```

| 字段 | 说明 |
|------|------|
| `enabled` | `false` 表示暂停守护但保留在列表中 |
| `restartDelayMs` | 检测到进程退出后等待毫秒数再重启（默认 3000） |
| `maxRestarts` | 最大重启次数，`0` 表示无限 |

---

## 编译

### 环境要求

**macOS（推荐，本项目开发环境）**

```bash
brew install mingw-w64
```

**Linux / WSL**

```bash
sudo apt install mingw-w64
```

**Windows（MSYS2）**

```bash
pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-binutils
```

### 编译步骤

```bash
# 克隆项目
git clone <repo-url>
cd ProcessGuardCpp

# 编译（产物在 dist/ProcessGuard.exe）
make

# 清理
make clean
```

> 编译器：`x86_64-w64-mingw32-g++`，目标平台 Windows x64，静态链接，无外部依赖。

### 更换图标

将新图标 PNG 放入 `res/`，运行转换脚本生成 ICO，再重新编译：

```bash
python3 tools/png_to_ico.py res/你的图标.png res/ProcessGuard.ico
make clean && make
```

---

## 文件结构

```
ProcessGuardCpp/
├── src/
│   ├── main.cpp       # 程序入口、单实例、watchdog 模式分发
│   ├── common.h       # 数据结构、颜色常量、枚举定义
│   ├── config.h       # JSON 配置读写（无外部库）
│   ├── engine.h       # 守护引擎（后台线程、进程启停、CPU/内存采集）
│   ├── selfguard.h    # Watchdog 子进程逻辑
│   └── window.h       # 完整 Win32 GUI（GDI 绘制、托盘、拖拽）
├── res/
│   ├── ProcessGuard.rc   # 资源文件（图标、版本信息、DPI 清单）
│   ├── ProcessGuard.ico  # 应用图标（由 png_to_ico.py 生成）
│   └── shield.png        # 图标源文件
├── tools/
│   └── png_to_ico.py     # PNG 转 Windows ICO 工具
├── dist/
│   └── ProcessGuard.exe  # 编译产物（make 后生成）
└── Makefile
```

---

## 技术细节

- **语言**：C++17，仅用 Win32 / Shell API，无第三方库
- **GUI**：纯 GDI 绘制，双缓冲防闪烁，DPI-Aware（PerMonitorV2）
- **图标**：通过系统图标列表（`SHGFI_SYSICONINDEX` + `SHIL_EXTRALARGE`）获取高清 48px 图标
- **体积**：strip 后约 350KB，静态链接 stdc++ / pthread
- **兼容性**：Windows 7 SP1 及以上（`WINVER=0x0601`）


纯 C++ / Win32 API 实现的轻量进程守护工具，单文件 exe，无任何运行时依赖。

---

## 功能特性

| 功能 | 说明 |
|------|------|
| 拖拽添加 | 将 `.exe` 直接拖入窗口即可添加守护 |
| 自动重启 | 进程异常退出后按设定延迟自动重启 |
| 实时监控 | 每 2 秒刷新 CPU / 内存 / PID / 重启次数 |
| 未响应检测 | 检测 UI 进程是否挂起（SendMessageTimeout） |
| 自我守护 | 启动时自动 spawn 隐藏 watchdog 子进程保护自身 |
| 配置持久化 | 重启后自动恢复上次守护列表（JSON 格式） |
| 单独启停 | 双击列表行可切换单个进程的守护开关 |
| 最小化托盘 | 关闭窗口最小化到系统托盘，右键菜单操作 |
| 单实例 | 重复启动时自动激活已有窗口 |

---

## 快速使用

1. 双击运行 `ProcessGuard.exe`
2. 将要守护的 `.exe` 拖入窗口，或点击「＋ 添加进程」浏览选择
3. 工具自动启动并持续守护所有列表中的进程
4. 关闭窗口 → 最小化到托盘继续后台运行
5. 托盘图标右键 → 「退出」才会真正退出

---

## 自我守护原理

```
ProcessGuard.exe（主进程）
    │
    └─ 启动 ──► Watchdog 子进程（隐藏，无窗口）
                    │
                    └─ WaitForSingleObject(主进程句柄, INFINITE)
                                │
                    主进程崩溃 ──► CreateProcess 重启主进程
```

- Watchdog 极度轻量：仅持有一个内核等待，不轮询，CPU 占用为零
- 主进程正常退出时会先 `TerminateProcess` 掉 Watchdog，不会误重启

---

## 配置文件

保存于 `%APPDATA%\ProcessGuard\config.json`，可手动编辑：

```json
{
  "processes": [
    {
      "exePath": "C:\\Tools\\myapp.exe",
      "name": "myapp.exe",
      "enabled": true,
      "restartDelayMs": 3000,
      "maxRestarts": 0,
      "args": "--config prod.ini"
    }
  ]
}
```

| 字段 | 说明 |
|------|------|
| `restartDelayMs` | 检测到进程退出后等待毫秒数再重启（默认 3000） |
| `maxRestarts` | 最大重启次数，`0` 表示无限 |
| `args` | 启动参数，多个参数空格分隔 |
| `enabled` | `false` 表示暂停守护但保留在列表中 |

---

## 自行编译

### 环境要求

- Linux / WSL：`sudo apt install mingw-w64`
- 或 Windows：安装 [MSYS2](https://www.msys2.org/) + `pacman -S mingw-w64-x86_64-gcc`

### 编译

```bash
make
# 产物: dist/ProcessGuard.exe （约 260KB，静态链接，无依赖）
```

### 文件结构

```
ProcessGuard/
├── src/
│   ├── main.cpp       # 程序入口、单实例、watchdog 模式分发
│   ├── common.h       # 数据结构、颜色常量、枚举定义
│   ├── config.h       # JSON 配置读写（无外部库）
│   ├── engine.h       # 守护引擎（后台线程、进程启停、CPU/内存采集）
│   ├── selfguard.h    # 自我守护 watchdog 实现
│   └── window.h       # Win32 GUI、ListView、自定义绘制、拖拽、托盘
├── res/
│   └── ProcessGuard.rc  # 版本信息 + Manifest（启用视觉样式）
├── Makefile
└── README.md
```

---

## 技术要点

- **纯 Win32 API**：无 MFC / Qt / wxWidgets，无 Python，无运行时
- **后台守护线程**：`std::thread` + `std::mutex`，每 2 秒轮询 `GetExitCodeProcess`
- **CPU 采集**：`GetProcessTimes` 差分计算，无需 PDH / WMI
- **内存采集**：`GetProcessMemoryInfo` 读取 WorkingSetSize
- **未响应检测**：`SendMessageTimeoutW` + `SMTO_ABORTIFHUNG`
- **自定义绘制**：`NM_CUSTOMDRAW` 对状态列着色
- **深色主题**：`DwmSetWindowAttribute(DWMWA_USE_IMMERSIVE_DARK_MODE)`
- **静态链接**：`-static -lstdc++ -lpthread`，单文件部署

---

## 注意事项

- 守护需要管理员权限的程序时，请以管理员身份运行 ProcessGuard
- Watchdog 子进程在任务管理器中可见（进程名同主程序，隐藏窗口）
- 配置文件为 UTF-8 编码，路径中支持中文和 Unicode 字符
