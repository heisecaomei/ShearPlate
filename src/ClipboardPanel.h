#pragma once

#include <QIcon>
#include <QWidget>

#include "ClipboardItem.h"

class ClipboardListModel;
class ClipboardItemDelegate;
class ClipboardListView;
class HoverIconButton;
class QStackedLayout;
class QLabel;
class QToolButton;
class QButtonGroup;

// 白色圆角悬浮面板：展示剪贴板历史，支持单击上屏、拖拽、右键删除。
class ClipboardPanel : public QWidget
{
    Q_OBJECT
public:
    explicit ClipboardPanel(QWidget *parent = nullptr);

    void setModel(ClipboardListModel *model);
    void setTextShowIcon(bool enabled);
    void positionNear(const QRect &trayGeometry);
    void showPanel();
    void slideOut();
    void refreshTheme();

signals:
    void pasteRequested(const ClipboardItem &item, bool cutAfterPaste); // 单击上屏；true=剪切语义
    void deleteRequested(const QString &id);
    void togglePinRequested(const QString &id);
    void panelHidden(); // 面板隐藏（用于取消等待粘贴）
    void settingsRequested(); // 点击分类栏底部设置按钮

protected:
    void paintEvent(QPaintEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void hideEvent(QHideEvent *event) override;

private slots:
    void onCustomContextMenu(const QPoint &pos);
    void updateEmptyState();
    void onOpenIconClicked(const QModelIndex &index);

private:
    void applyAcrylic();

    ClipboardListView *m_list = nullptr;
    ClipboardListModel *m_model = nullptr;
    ClipboardItemDelegate *m_delegate = nullptr;
    QWidget *m_emptyWidget = nullptr;
    QStackedLayout *m_stack = nullptr;
    QLabel *m_titleLabel = nullptr;
    QLabel *m_emptyText = nullptr;
    QLabel *m_logoLabel = nullptr;
    QLabel *m_countLabel = nullptr;
    QWidget *m_catBar = nullptr;
    QToolButton *m_catButtons[4] = { nullptr, nullptr, nullptr, nullptr };
    HoverIconButton *m_settingsBtn = nullptr;
    QIcon m_catDefaultIcons[4];
    QIcon m_catIcons[4];
    QString m_catNames[4];   // "all","file","img","txt"
    int m_catFilters[4] = { -1, 2, 1, 0 }; // 所有/文件/图片/文本
};
