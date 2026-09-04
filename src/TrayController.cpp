#include "TrayController.h"

#include <QAction>
#include <QMenu>

#include "ClipboardPanel.h"
#include "Util.h"

TrayController::TrayController(ClipboardPanel *panel, QObject *parent)
    : QObject(parent)
    , m_panel(panel)
{
    m_tray = new QSystemTrayIcon(this);
    m_tray->setIcon(loadThemedIcon(QStringLiteral("taskbar.png")));
    m_tray->setToolTip(QStringLiteral("剪切板"));

    m_menu = new QMenu();
    QAction *openAction = m_menu->addAction(QStringLiteral("打开剪切板"));
    QAction *settingsAction = m_menu->addAction(QStringLiteral("设置"));
    m_menu->addSeparator();
    QAction *quitAction = m_menu->addAction(QStringLiteral("退出"));
    m_tray->setContextMenu(m_menu);

    connect(m_tray, &QSystemTrayIcon::activated, this, &TrayController::onActivated);
    connect(openAction, &QAction::triggered, this, &TrayController::togglePanel);
    connect(settingsAction, &QAction::triggered, this, &TrayController::openSettingsRequested);
    connect(quitAction, &QAction::triggered, this, &TrayController::quitRequested);
}

void TrayController::show()
{
    m_tray->show();
}

void TrayController::reloadIcon()
{
    m_tray->setIcon(loadThemedIcon(QStringLiteral("taskbar.png")));
}

void TrayController::togglePanel()
{
    if (m_panel->isVisible()) {
        m_panel->slideOut();
        return;
    }
    showPanel();
}

void TrayController::showPanel()
{
    if (m_panel->isVisible())
        return;
    emit beforePanelShow();
    m_panel->positionNear(m_tray->geometry());
    m_panel->showPanel();
}

QRect TrayController::trayGeometry() const
{
    return m_tray->geometry();
}

void TrayController::onActivated(QSystemTrayIcon::ActivationReason reason)
{
    // 左键单击切换面板
    if (reason == QSystemTrayIcon::Trigger)
        togglePanel();
}
