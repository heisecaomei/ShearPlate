#pragma once

#include <QAbstractNativeEventFilter>
#include <QKeySequence>
#include <QObject>

// Windows 全局热键（RegisterHotKey + WM_HOTKEY），
// 即使程序不在前台也能触发呼出/隐藏剪贴板面板。
class GlobalHotkey : public QObject, public QAbstractNativeEventFilter
{
    Q_OBJECT
public:
    explicit GlobalHotkey(QObject *parent = nullptr);
    ~GlobalHotkey() override;

    bool registerHotkey(const QKeySequence &sequence);
    void unregisterHotkey();
    QKeySequence keySequence() const { return m_sequence; }

signals:
    void activated();

protected:
    bool nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result) override;

private:
    int m_id = 1;
    QKeySequence m_sequence;
};
