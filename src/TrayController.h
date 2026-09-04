#pragma once

#include <QObject>
#include <QSystemTrayIcon>

class ClipboardPanel;
class QMenu;

// 右下角系统托盘图标：左键切换面板，右键弹出菜单。
class TrayController : public QObject
{
    Q_OBJECT
public:
    explicit TrayController(ClipboardPanel *panel, QObject *parent = nullptr);

    void show();
    void reloadIcon();      // 主题切换后重新加载托盘图标
    void togglePanel();     // 切换显示/隐藏（托盘左键、全局热键）
    void showPanel();       // 显示（已显示则不动，用于单实例激活）
    QRect trayGeometry() const;

signals:
    void openSettingsRequested();
    void quitRequested();
    // 面板即将显示（用于在面板抢占焦点前记录前台窗口）
    void beforePanelShow();

private slots:
    void onActivated(QSystemTrayIcon::ActivationReason reason);

private:
    QSystemTrayIcon *m_tray = nullptr;
    QMenu *m_menu = nullptr;
    ClipboardPanel *m_panel = nullptr;
};
