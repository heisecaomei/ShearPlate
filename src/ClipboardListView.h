#pragma once

#include <QListView>
#include <QModelIndex>
#include <QPoint>

class QLabel;

// 继承 QListView：感知拖拽起止、处理“打开”图标点击、图片条目悬停大图预览、给列表内容留白。
class ClipboardListView : public QListView
{
    Q_OBJECT
public:
    explicit ClipboardListView(QWidget *parent = nullptr);
    ~ClipboardListView() override;

signals:
    void dragStarted();
    void dragFinished();
    void openIconClicked(const QModelIndex &index);
    void itemMoved(const QModelIndex &index);   // 剪切（移动）成功，请求删除该条目

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void startDrag(Qt::DropActions supportedActions) override;

private:
    void loadPreviewImage(const QString &path);
    void positionPreview(const QPoint &globalPos);
    void hidePreview();

    QPoint m_pressPos; // 鼠标左键按下位置（用于拖拽缩略图锚点）
    QLabel *m_previewLabel = nullptr; // 图片条目悬停时的大图预览窗
    QString m_previewPath;            // 当前预览的图片路径（避免重复加载）
};
