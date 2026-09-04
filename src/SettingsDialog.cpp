#include "SettingsDialog.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QFormLayout>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QKeySequenceEdit>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QStackedWidget>
#include <QStyleHints>
#include <QVBoxLayout>

#include "AppSettings.h"
#include "Theme.h"

SettingsDialog::SettingsDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("设置"));
    setMinimumWidth(520);

    // ---------- 常规页 ----------
    auto *generalPage = new QWidget(this);
    auto *generalForm = new QFormLayout(generalPage);
    generalForm->setContentsMargins(20, 20, 20, 20);
    generalForm->setSpacing(14);
    generalForm->setHorizontalSpacing(18);

    m_retentionSpin = new QSpinBox(generalPage);
    m_retentionSpin->setRange(1, 720);
    m_retentionSpin->setSuffix(QStringLiteral(" 小时"));
    m_retentionSpin->setValue(AppSettings::retentionHours());
    m_retentionSpin->setMaximumWidth(150);
    generalForm->addRow(QStringLiteral("保留时长"), m_retentionSpin);

    m_autoStartCheck = new QCheckBox(QStringLiteral("开机自动启动"), generalPage);
    m_autoStartCheck->setChecked(AppSettings::autoStart());
    generalForm->addRow(QStringLiteral("后台常驻"), m_autoStartCheck);

    m_clickToTopCheck = new QCheckBox(QStringLiteral("点击内容时置顶并刷新保留时长"), generalPage);
    m_clickToTopCheck->setChecked(AppSettings::clickToTop());
    generalForm->addRow(QStringLiteral("点击行为"), m_clickToTopCheck);

    m_textIconCheck = new QCheckBox(QStringLiteral("文本类型显示图标"), generalPage);
    m_textIconCheck->setChecked(AppSettings::textShowIcon());
    generalForm->addRow(QStringLiteral("文本显示"), m_textIconCheck);

    auto *sensitiveWidget = new QWidget(generalPage);
    auto *sensitiveLayout = new QVBoxLayout(sensitiveWidget);
    sensitiveLayout->setContentsMargins(0, 0, 0, 0);
    sensitiveLayout->setSpacing(2);
    m_sensitiveCheck = new QCheckBox(QStringLiteral("启用敏感内容过滤"), sensitiveWidget);
    m_sensitiveCheck->setChecked(AppSettings::sensitiveFilter());
    m_sensitiveHint = new QLabel(
        QStringLiteral("复制敏感内容时不记录；本程序写入时排除系统剪贴板历史/云剪贴板"),
        sensitiveWidget);
    m_sensitiveHint->setWordWrap(true);
    sensitiveLayout->addWidget(m_sensitiveCheck);
    sensitiveLayout->addWidget(m_sensitiveHint);
    generalForm->addRow(QStringLiteral("敏感过滤"), sensitiveWidget);

    // ---------- 外观页 ----------
    auto *appearancePage = new QWidget(this);
    auto *appearanceForm = new QFormLayout(appearancePage);
    appearanceForm->setContentsMargins(20, 20, 20, 20);
    appearanceForm->setSpacing(14);
    appearanceForm->setHorizontalSpacing(18);

    m_themeCombo = new QComboBox(appearancePage);
    m_themeCombo->addItem(QStringLiteral("跟随系统"), 0);
    m_themeCombo->addItem(QStringLiteral("浅色"), 1);
    m_themeCombo->addItem(QStringLiteral("深色"), 2);
    m_themeCombo->setCurrentIndex(AppSettings::themeMode());
    m_themeCombo->setMaximumWidth(160);
    appearanceForm->addRow(QStringLiteral("主题"), m_themeCombo);

    m_glassCheck = new QCheckBox(QStringLiteral("毛玻璃（半透明）效果"), appearancePage);
    m_glassCheck->setChecked(AppSettings::glassEffect());
    appearanceForm->addRow(QStringLiteral("界面效果"), m_glassCheck);

    // ---------- 快捷键页 ----------
    auto *hotkeyPage = new QWidget(this);
    auto *hotkeyLayout = new QVBoxLayout(hotkeyPage);
    hotkeyLayout->setContentsMargins(20, 20, 20, 20);
    hotkeyLayout->setSpacing(12);

    m_hotkeyHint = new QLabel(
        QStringLiteral("点击下方输入框后按下任意组合键（如 Ctrl+Alt+C），即可全局呼出/隐藏剪切板面板。"),
        hotkeyPage);
    m_hotkeyHint->setWordWrap(true);
    hotkeyLayout->addWidget(m_hotkeyHint);

    m_hotkeyEdit = new QKeySequenceEdit(QKeySequence(AppSettings::hotkey()), hotkeyPage);
    m_hotkeyEdit->setMinimumHeight(38);
    m_hotkeyEdit->setMaximumWidth(240);
    hotkeyLayout->addWidget(m_hotkeyEdit);

    m_shortcutsLabel = new QLabel(
        QStringLiteral("· 左键拖动条目：复制到目标位置\n"
                       "· Shift + 左键拖动：剪切（移动）到目标位置\n"
                       "· 左键点击条目：粘贴上屏；Shift + 左键：粘贴后删除该条目\n"
                       "· 点击文字末尾图标：打开链接 / 所在文件夹\n"
                       "· 右键点击条目：置顶 / 删除菜单"),
        hotkeyPage);
    m_shortcutsLabel->setWordWrap(true);
    hotkeyLayout->addSpacing(6);
    hotkeyLayout->addWidget(m_shortcutsLabel);

    // ---------- 关于页 ----------
    auto *aboutPage = new QWidget(this);
    auto *aboutForm = new QFormLayout(aboutPage);
    aboutForm->setContentsMargins(20, 20, 20, 20);
    aboutForm->setSpacing(14);
    aboutForm->setHorizontalSpacing(18);

    auto *authorLabel = new QLabel(QStringLiteral("黑色草莓"), aboutPage);
    auto *qqLabel = new QLabel(QStringLiteral("316767225"), aboutPage);
    auto *versionLabel = new QLabel(QCoreApplication::applicationVersion(), aboutPage);
    aboutForm->addRow(QStringLiteral("作者"), authorLabel);
    aboutForm->addRow(QStringLiteral("联系QQ"), qqLabel);
    aboutForm->addRow(QStringLiteral("版本"), versionLabel);

    // ---------- 左侧导航 + 内容栈 ----------
    auto *nav = new QWidget(this);
    nav->setFixedWidth(96);
    auto *navLayout = new QVBoxLayout(nav);
    navLayout->setContentsMargins(0, 0, 0, 0);
    navLayout->setSpacing(10);

    auto *navGroup = new QButtonGroup(this);
    navGroup->setExclusive(true);
    const QStringList navNames = { QStringLiteral("常规"), QStringLiteral("外观"),
                                   QStringLiteral("快捷键"), QStringLiteral("关于") };
    for (int i = 0; i < navNames.size(); ++i) {
        auto *btn = new QPushButton(navNames.at(i), nav);
        btn->setObjectName(QStringLiteral("navBtn"));
        btn->setCheckable(true);
        btn->setCursor(Qt::PointingHandCursor);
        navGroup->addButton(btn, i);
        navLayout->addWidget(btn); // 高度按文字+padding，不拉伸
    }
    navLayout->addStretch(1);      // 底部留白，tabs 靠设置页顶部往下排列

    auto *stack = new QStackedWidget(this);
    stack->addWidget(generalPage);
    stack->addWidget(appearancePage);
    stack->addWidget(hotkeyPage);
    stack->addWidget(aboutPage);

    auto *rootLayout = new QHBoxLayout(this);
    rootLayout->setContentsMargins(12, 12, 12, 12);
    rootLayout->setSpacing(12);
    rootLayout->addWidget(nav);
    rootLayout->addWidget(stack, 1);

    connect(navGroup, qOverload<int>(&QButtonGroup::idClicked), stack, &QStackedWidget::setCurrentIndex);
    navGroup->button(0)->setChecked(true);

    // ---------- 自动保存：控件变化即写入设置并通知外部 ----------
    connect(m_retentionSpin, qOverload<int>(&QSpinBox::valueChanged), this, [this](int v) {
        AppSettings::setRetentionHours(v);
        emit settingsChanged();
    });
    connect(m_autoStartCheck, &QCheckBox::toggled, this, [this](bool on) {
        AppSettings::setAutoStart(on);
        emit settingsChanged();
    });
    connect(m_clickToTopCheck, &QCheckBox::toggled, this, [this](bool on) {
        AppSettings::setClickToTop(on);
        emit settingsChanged();
    });
    connect(m_textIconCheck, &QCheckBox::toggled, this, [this](bool on) {
        AppSettings::setTextShowIcon(on);
        emit settingsChanged();
    });
    connect(m_sensitiveCheck, &QCheckBox::toggled, this, [this](bool on) {
        AppSettings::setSensitiveFilter(on);
        emit settingsChanged();
    });
    connect(m_themeCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int idx) {
        AppSettings::setThemeMode(m_themeCombo->itemData(idx).toInt());
        refreshTheme(); // 设置页自身立即换肤
        emit settingsChanged();
    });
    connect(m_glassCheck, &QCheckBox::toggled, this, [this](bool on) {
        AppSettings::setGlassEffect(on);
        emit settingsChanged();
    });
    connect(m_hotkeyEdit, &QKeySequenceEdit::keySequenceChanged, this, [this](const QKeySequence &seq) {
        if (!seq.isEmpty()) {
            AppSettings::setHotkey(seq.toString());
            emit settingsChanged();
        }
    });

    // 跟随系统深浅色变化时刷新（主题设为“跟随系统”时生效）
    connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged, this,
            [this](Qt::ColorScheme) { refreshTheme(); });

    refreshTheme(); // 应用当前主题样式
}

void SettingsDialog::refreshTheme()
{
    const ThemeColors c = Theme::current();
    setStyleSheet(QStringLiteral(
        "QDialog{background:%1;}"
        "QLabel{color:%2;}"
        "QCheckBox{color:%2;spacing:6px;}"
        "QSpinBox{background:%4;border:1px solid %3;border-radius:3px;padding:5px 10px;color:%2;min-height:22px;}"
        "QKeySequenceEdit{background:%4;border:1px solid %3;border-radius:3px;padding:5px 10px;color:%2;}"
        "QComboBox{background:%4;border:1px solid %3;border-radius:3px;padding:5px 10px;color:%2;min-height:22px;}"
        "QPushButton#navBtn{background:transparent;border:none;border-radius:6px;color:%2;text-align:left;padding:6px 14px;}"
        "QPushButton#navBtn:checked{background:%4;color:%2;font-weight:bold;}"
        "QPushButton#navBtn:hover{background:%5;}")
        .arg(c.panelBg.name(),
             c.textPrimary.name(),
             c.cardBorder.name(),
             c.cardBg.name(),
             c.cardBgHover.name()));

    const QString secondary = QStringLiteral("color:%1;").arg(c.textSecondary.name());
    if (m_sensitiveHint)
        m_sensitiveHint->setStyleSheet(secondary + QStringLiteral("font-size:9pt;"));
    if (m_hotkeyHint)
        m_hotkeyHint->setStyleSheet(secondary);
    if (m_shortcutsLabel)
        m_shortcutsLabel->setStyleSheet(secondary);
}
