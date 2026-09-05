# 剪切板 (ShearPlate)

一个基于 **C++ / Qt 6** 的 Windows 剪贴板历史管理工具。常驻系统右下角托盘，实时记录你复制/剪切的**文本、图片、文件**，以磁盘文件形式持久保存，支持单击上屏、拖拽导出、全局快捷键呼出、深浅色主题、毛玻璃效果等，是日常办公与开发中提升效率的小助手。

> 纯本地运行，数据只保存在你自己的电脑上，不上传任何内容。

---

## ✨ 功能特性

### 实时监控 & 持久存储
- 常驻后台、驻留右下角系统托盘，关闭面板后继续运行
- 实时监听系统剪贴板，自动捕获**文本 / 图片 / 文件（文件夹）**
- 每条记录以独立文件落盘（文本 `*.json`、图片 `*.json + *.png`）
- 默认保留 **12 小时**，过期自动清理（保留时长可在设置中调整）

### 悬浮面板
- 左键点击托盘图标或按全局快捷键，面板从屏幕右侧**滑入**（关闭时滑出）
- 白色圆角、半透明毛玻璃（可在设置中开关）、无边框置顶
- 面板宽度 330px，条目卡片化展示，正好完整显示 6 条

### 交互方式
| 操作 | 效果 |
| --- | --- |
| **单击**条目 | 自动上屏（粘贴到当前光标处，面板保持显示） |
| **拖拽**条目 | 文本/图片拖到输入框；文件拖到资源管理器复制 |
| **右键**条目 | 弹出菜单，可删除该条 |
| **右键**托盘图标 | 打开剪切板 / 设置 / 退出 |

### 智能类型识别
- **文本**：显示文本图标（可在设置中开关图标显示）；自动识别**网页链接 / 文件夹路径**，显示为可点击链接（下划线 + link 图标），点击打开浏览器/资源管理器
- **图片**：显示缩略图，并智能识别**截图**（尺寸接近屏幕分辨率），文字显示「截图」
- **文件**：显示系统默认图标；多个文件智能汇总
  - 两个视频 → `共2个视频`；两个 exe → `共2个可执行文件`；两个文件夹 → `共2个文件夹`
  - 类型混在一起 → `共2个文件`；下方展示文件名列表（减淡、超长省略）
  - 支持分类：视频、音频、图片、可执行文件、文档、压缩文件、文件夹
- **文件类型粘贴**：把文件条目粘贴到资源管理器时，以**真实文件**形式复制（而非路径文本）
- **打开所在目录**：点击「前往」图标（单文件）或「共 x 个文件」标题（多文件），打开目录并**预选中**这些文件（多文件全选）

### 便捷操作
- **全局快捷键**呼出/隐藏面板，默认 `Ctrl+Shift+V`，可在设置中自定义（即使程序不在前台也能触发）
- **单实例保护**：重复双击 exe 只会运行一个实例，并自动唤起已有面板
- **单击置顶 + 刷新时效**：单击某条后自动置顶、重新计时 12 小时（可在设置中关闭）
- **空状态提示**：无记录时居中显示提示图与「剪切板无内容」

### 主题与外观
- **深色 / 浅色 / 跟随系统**三种主题模式，所有页面（面板、设置、右键菜单）统一跟随
- **毛玻璃（半透明）效果**可在设置中开关
- 图标分 `light` / `dark` 两套，主题切换时自动切换

### 可配置设置
- 保留时长（小时）
- 开机自动启动
- 点击行为（是否置顶并刷新保留时长）
- 文本类型是否显示图标
- 外观主题（跟随系统 / 浅色 / 深色）
- 毛玻璃（半透明）效果
- 全局快捷呼出键

---

## 🛠️ 技术栈

- **语言**：C++17
- **框架**：Qt 6.x（Core / Gui / Widgets / Network）
- **构建**：CMake 3.16+，Ninja 或 MinGW Makefiles
- **平台**：Windows（使用 `SendInput`、`RegisterHotKey`、`QLocalServer`、Shell COM 等系统能力）

---

## 📦 构建

### 依赖
- Qt 6.x（建议勾选 MinGW 64-bit 组件）
- MinGW 工具链（Qt 自带）
- CMake 3.16+、Ninja（Qt 自带）

### 方式一：一键编译（推荐）

1. 安装 Qt 6.x：从 [qt.io](https://www.qt.io/download-open-source) 下载在线安装器，安装时勾选：
   - **Qt 6.x.x → MinGW 64-bit**
   - **Developer and Designer Tools → MinGW、CMake、Ninja**
2. 双击项目根目录的 `build.bat`
3. 完成后直接双击 `build\ShearPlate.exe`（已自动部署 Qt 运行库，绿色免安装）

> 脚本会自动定位 Qt；若提示找不到，可手动指定后重跑：
> `set QT_DIR=C:\Qt\6.x.x\mingw_64` 然后运行 `build.bat`

### 方式二：Qt Creator

1. 打开 Qt Creator → `File → Open File or Project…`，选择 `CMakeLists.txt`
2. 选择已配置好的 Qt 6 Kit（MinGW）
3. 点击运行

### 方式三：命令行

```powershell
# MinGW 示例
cmake -S . -B build -G "Ninja" -DCMAKE_PREFIX_PATH=C:\Qt\6.x.x\mingw_64 -DCMAKE_BUILD_TYPE=Release
cmake --build build

# 部署 Qt 运行库
C:\Qt\6.x.x\mingw_64\bin\windeployqt.exe --release --no-translations build\ShearPlate.exe
```

### 打包为安装程序

1. 先完成编译（生成 `build\` 目录）
2. 安装 [Inno Setup 6](https://jrsoftware.org/isinfo.php)
3. 右键 `installer.iss` → Compile（或命令行 `ISCC.exe installer.iss`）
4. 在 `Output\ShearPlate-Setup.exe` 得到安装包（含开始菜单/桌面快捷方式与卸载入口）

---

## 🚀 使用说明

1. 运行 `ShearPlate.exe`，程序驻留右下角托盘
2. **左键**托盘图标（或按全局快捷键）呼出面板
3. 复制任意文本、图片或文件，面板会实时出现对应记录
4. 单击自动上屏、拖拽到目标应用、右键删除
5. **右键**托盘图标 → `设置`，可调整保留时长、主题、毛玻璃、快捷键等

---

## 📁 数据存储位置

剪贴板记录保存在：

```
%APPDATA%\ShearPlate\ShearPlate\clips\
```

- 文本记录：`<id>.json`
- 图片记录：`<id>.json` + `<id>.png`
- 应用设置：注册表 `HKCU\Software\ShearPlate`

---

## 🖼️ 图标资源

图标分 **light（浅色）** 与 **dark（深色）** 两套，位于 `images\light\` 和 `images\dark\`：

| 文件 | 用途 |
| --- | --- |
| `taskbar.png` | 系统托盘图标 |
| `logo.png` | 面板标题栏 Logo |
| `close_default.png` / `close_active.png` | 面板关闭按钮（默认/悬停） |
| `shortcut_default.png` / `shortcut_active.png` | 单文件条目「前往」图标（默认/悬停） |
| `text.png` | 文本类型图标 |
| `link.png` | 链接图标（文本链接、多文件标题末尾） |

`images\` 顶层（不区分主题）：

| 文件 | 用途 |
| --- | --- |
| `favicon.ico` | 程序 exe / 安装包图标 |
| `filelist.png` | 多文件/文件夹图标 |
| `nothing.png` | 空状态提示图 |

> 图标可替换：替换 `images\` 下同名文件后重新编译即可。

---

## 📂 项目结构

```
ShearPlate/
├── CMakeLists.txt          # 构建配置
├── build.bat               # 一键编译脚本
├── installer.iss           # Inno Setup 安装包脚本
├── resources.qrc           # 图标资源
├── images/                 # 图标（light / dark 两套）
└── src/
    ├── main.cpp            # 入口
    ├── ClipboardApp.*      # 应用总控（单实例、热键、模块串联）
    ├── ClipboardManager.*  # 剪贴板监听 + 上屏/回填
    ├── ClipboardStore.*    # 磁盘存储 + 过期清理
    ├── ClipboardItem.h     # 记录数据结构（类型分类/链接/截图判定）
    ├── ClipboardListModel.*
    ├── ClipboardItemDelegate.*  # 列表条目绘制
    ├── ClipboardListView.* # 列表视图（拖拽、悬停/点击）
    ├── ClipboardPanel.*    # 圆角悬浮面板（滑入滑出动画）
    ├── TrayController.*    # 系统托盘
    ├── GlobalHotkey.*      # 全局快捷键（RegisterHotKey）
    ├── SettingsDialog.*    # 设置面板
    ├── AppSettings.h       # 设置读写
    ├── Theme.h             # 主题配色（浅色/深色）
    ├── HoverIconButton.*   # 悬停切换图标按钮
    ├── Util.h              # 图标加载（含主题图标）
    └── WinUtil.*           # Windows 系统能力（SendInput、前台窗口等）
```

---

## 📄 许可证

本项目采用 [Apache License 2.0](./LICENSE) 开源，欢迎 Star、Fork 与 PR。
