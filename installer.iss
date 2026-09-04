; ============================================================
;  ShearPlate 安装脚本（Inno Setup 6）
;  用法：安装 Inno Setup 6 后，右键本文件 → Compile，或命令行：
;        ISCC.exe installer.iss
;  生成：Output\ShearPlate-Setup.exe
; ============================================================

#define AppName "剪切板 (ShearPlate)"
#define AppVersion "0.1.11"
#define AppExeName "ShearPlate.exe"
#define AppPublisher "黑色草莓"

[Setup]
; 应用唯一标识（GUID），请保持稳定；如需重新生成可用在线 GUID 生成器
AppId={{7F3A5C2E-9B1D-4E6A-8C4F-2D5B7E9A1C3D}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher={#AppPublisher}
DefaultDirName={autopf}\ShearPlate
DefaultGroupName={#AppName}
; 安装包输出目录与文件名
OutputDir=Output
OutputBaseFilename=ShearPlate-Setup
; 安装包图标（使用项目内的 exe 图标）
SetupIconFile=images\favicon.ico
; 卸载图标
UninstallDisplayIcon={app}\{#AppExeName}
; 压缩与权限
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
; 需要管理员权限（可选，写 HKCU 其实不需要，此处按需）
PrivilegesRequired=lowest
; 中文语言
ShowLanguageDialog=no

[Languages]
Name: "chinesesimp"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "创建桌面快捷方式"; GroupDescription: "附加任务："

[Files]
; 打包整个 build 目录（含 exe、Qt DLL、平台插件等）
Source: "build\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs; Excludes: "CMakeFiles,ShearPlate_autogen,.qt,CMakeCache.txt,cmake_install.cmake,build.ninja,.ninja_deps,.ninja_log,app.rc,*.obj,*.cpp,*.json,*.cmake,*.lock,*.txt,*.h,*.d"

[Icons]
; 开始菜单快捷方式
Name: "{group}\{#AppName}"; Filename: "{app}\{#AppExeName}"
; 卸载快捷方式
Name: "{group}\卸载 {#AppName}"; Filename: "{uninstallexe}"
; 桌面快捷方式（可选任务）
Name: "{autodesktop}\{#AppName}"; Filename: "{app}\{#AppExeName}"; Tasks: desktopicon

[Run]
; 安装完成后询问是否运行
Filename: "{app}\{#AppExeName}"; Description: "立即运行 {#AppName}"; Flags: nowait postinstall skipifsilent
