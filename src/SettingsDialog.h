#pragma once

#include <QDialog>

class QSpinBox;
class QCheckBox;
class QKeySequenceEdit;
class QComboBox;
class QLabel;

class SettingsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit SettingsDialog(QWidget *parent = nullptr);

    // 重新应用当前主题样式（主题切换或系统深浅色变化时调用）
    void refreshTheme();

signals:
    void settingsChanged(); // 任一设置项变化（已自动持久化）时发出，供外部即时刷新

private:
    QSpinBox *m_retentionSpin = nullptr;
    QCheckBox *m_autoStartCheck = nullptr;
    QCheckBox *m_clickToTopCheck = nullptr;
    QCheckBox *m_textIconCheck = nullptr;
    QCheckBox *m_sensitiveCheck = nullptr;
    QCheckBox *m_glassCheck = nullptr;
    QKeySequenceEdit *m_hotkeyEdit = nullptr;
    QComboBox *m_themeCombo = nullptr;
    QLabel *m_sensitiveHint = nullptr;
    QLabel *m_hotkeyHint = nullptr;
    QLabel *m_shortcutsLabel = nullptr;
};
