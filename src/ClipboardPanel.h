#pragma once

#include <QWidget>

#include "ClipboardItem.h"

class ClipboardListModel;
class ClipboardItemDelegate;
class ClipboardListView;
class QStackedLayout;
class QLabel;

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
};
