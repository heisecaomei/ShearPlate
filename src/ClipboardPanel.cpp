#include "ClipboardPanel.h"

#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QFrame>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QMenu>
#include <QPainter>
#include <QPropertyAnimation>
#include <QScreen>
#include <QStackedLayout>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>

#include "ClipboardItemDelegate.h"
#include "ClipboardListModel.h"
#include "ClipboardListView.h"
#include "HoverIconButton.h"
#include "Theme.h"
#include "Util.h"
#include "WinUtil.h"

ClipboardPanel::ClipboardPanel(QWidget *parent)
    : QWidget(parent)
{
    // 无边框 + 工具窗口（不占任务栏）+ 置顶 + 透明背景（实现圆角）
    setWindowFlags(Qt::FramelessWindowHint | Qt::Tool | Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating); // 显示时不抢焦点，保持原窗口光标
    setFixedSize(330, 529); // 高度 534 → 529，减 5px

    m_delegate = new ClipboardItemDelegate(this);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ---------- 标题栏 ----------
    auto *header = new QWidget(this);
    auto *headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(16, 12, 12, 8);
    headerLayout->setSpacing(8);

    m_logoLabel = new QLabel(header);
    m_logoLabel->setPixmap(loadThemedPixmap(QStringLiteral("logo.png"), 18, 18));
    m_logoLabel->setFixedSize(18, 18);

    m_titleLabel = new QLabel(QStringLiteral("剪切板"), header);
    QFont titleFont = m_titleLabel->font();
    titleFont.setBold(true);
    titleFont.setPointSizeF(titleFont.pointSizeF() * 1.1);
    m_titleLabel->setFont(titleFont);

    headerLayout->addWidget(m_logoLabel);
    headerLayout->addWidget(m_titleLabel);

    m_countLabel = new QLabel(QStringLiteral("（共0条）"), header);
    headerLayout->addWidget(m_countLabel);

    headerLayout->addStretch(1);

    // 关闭按钮
    auto *closeBtn = new HoverIconButton(loadThemedIcon(QStringLiteral("close_default.png")),
                                         loadThemedIcon(QStringLiteral("close_active.png")),
                                         header);
    connect(closeBtn, &QToolButton::clicked, this, &ClipboardPanel::slideOut);
    headerLayout->addWidget(closeBtn);

    // ---------- 列表 ----------
    m_list = new ClipboardListView(this);
    m_list->setItemDelegate(m_delegate);
    m_list->setFrameShape(QFrame::NoFrame);
    m_list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_list->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_list->setSelectionMode(QAbstractItemView::SingleSelection);
    m_list->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_list->setMouseTracking(true); // 悬停高亮
    m_list->setDragEnabled(true);
    m_list->setDragDropMode(QAbstractItemView::DragOnly);
    m_list->setDefaultDropAction(Qt::CopyAction);
    m_list->setContextMenuPolicy(Qt::CustomContextMenu);
    // 滚动条样式由 refreshTheme() 按主题设置

    // 单击条目（文字区域）：统一上屏——链接条目上屏的是链接文字，而非打开；
    // 按住 Shift 单击 = 剪切语义（粘贴后删除该历史条目）
    connect(m_list, &QListView::clicked, this, [this](const QModelIndex &index) {
        if (!index.isValid() || !m_model)
            return;
        const ClipboardItem item = m_model->itemAt(index.row());
        if (item.missing)
            return; // 失效条目不可上屏/复制
        const bool shift = QGuiApplication::keyboardModifiers() & Qt::ShiftModifier;
        emit pasteRequested(item, shift);
    });
    connect(m_list, &QWidget::customContextMenuRequested, this, &ClipboardPanel::onCustomContextMenu);
    connect(m_list, &ClipboardListView::openIconClicked, this, &ClipboardPanel::onOpenIconClicked);
    connect(m_list, &ClipboardListView::itemMoved, this, [this](const QModelIndex &index) {
        if (!index.isValid() || !m_model)
            return;
        emit deleteRequested(m_model->itemAt(index.row()).id); // 剪切成功后删除该条目
    });

    // ---------- 空状态 ----------
    m_emptyWidget = new QWidget(this);
    auto *emptyLayout = new QVBoxLayout(m_emptyWidget);
    emptyLayout->addStretch(1);
    auto *emptyImg = new QLabel(m_emptyWidget);
    emptyImg->setPixmap(loadPixmap(QStringLiteral("images/nothing.png"), 110, 110));
    emptyImg->setAlignment(Qt::AlignCenter);
    m_emptyText = new QLabel(QStringLiteral("剪切板无内容"), m_emptyWidget);
    m_emptyText->setAlignment(Qt::AlignCenter);
    QFont tf = m_emptyText->font();
    tf.setPointSizeF(tf.pointSizeF() * 1.05);
    m_emptyText->setFont(tf);
    emptyLayout->addWidget(emptyImg);
    emptyLayout->addSpacing(6);
    emptyLayout->addWidget(m_emptyText);
    emptyLayout->addStretch(1);

    m_stack = new QStackedLayout();
    m_stack->setContentsMargins(0, 0, 0, 0);
    m_stack->setSpacing(0);
    m_stack->addWidget(m_list);
    m_stack->addWidget(m_emptyWidget);

    // ---------- 主体：条目列表左右铺满整个面板 ----------
    auto *body = new QVBoxLayout();
    body->setContentsMargins(0, 0, 0, 0);
    body->setSpacing(0);
    body->addLayout(m_stack, 1);

    root->addWidget(header);
    root->addLayout(body, 1);

    refreshTheme();
}

void ClipboardPanel::setModel(ClipboardListModel *model)
{
    m_model = model;
    m_list->setModel(model);

    connect(model, &QAbstractItemModel::rowsInserted, this, &ClipboardPanel::updateEmptyState);
    connect(model, &QAbstractItemModel::rowsRemoved, this, &ClipboardPanel::updateEmptyState);
    connect(model, &QAbstractItemModel::modelReset, this, &ClipboardPanel::updateEmptyState);
    updateEmptyState();
}

void ClipboardPanel::setTextShowIcon(bool enabled)
{
    if (m_delegate)
        m_delegate->setTextShowIcon(enabled);
}

void ClipboardPanel::refreshTheme()
{
    const ThemeColors c = Theme::current();

    m_list->setStyleSheet(QStringLiteral("QListView{background:transparent;border:none;}"));

    if (m_titleLabel) {
        const QString titleColor = Theme::isDark() ? QStringLiteral("#ffffff") : QStringLiteral("#515151");
        m_titleLabel->setStyleSheet(QStringLiteral("color:%1;").arg(titleColor));
    }

    if (m_countLabel)
        m_countLabel->setStyleSheet(QStringLiteral("color:%1;").arg(c.textSecondary.name()));

    if (m_emptyText)
        m_emptyText->setStyleSheet(QStringLiteral("color:%1;").arg(c.textSecondary.name()));

    if (m_logoLabel)
        m_logoLabel->setPixmap(loadThemedPixmap(QStringLiteral("logo.png"), 18, 18));

    if (m_delegate)
        m_delegate->reloadIcons();

    if (isVisible())
        applyAcrylic();

    update();
}

void ClipboardPanel::applyAcrylic()
{
    const ThemeColors c = Theme::current();
    QColor tint = c.panelBg;
    tint.setAlpha(190);
    WinUtil::enableAcrylic(quintptr(winId()), tint);
}

void ClipboardPanel::updateEmptyState()
{
    if (!m_stack)
        return;
    const bool empty = !m_model || m_model->rowCount() == 0;
    m_stack->setCurrentWidget(empty ? m_emptyWidget : m_list);

    if (m_countLabel)
        m_countLabel->setText(QStringLiteral("（共%1条）").arg(m_model ? m_model->rowCount() : 0));
}

void ClipboardPanel::onOpenIconClicked(const QModelIndex &index)
{
    if (!index.isValid() || !m_model)
        return;
    const ClipboardItem item = m_model->itemAt(index.row());
    if (item.missing)
        return; // 失效条目不可打开

    // 文本链接条目：点击图标打开网页 / 本地路径
    if (item.type == ClipboardItem::Text) {
        const QString t = item.text.trimmed();
        if (item.linkType() == ClipboardItem::WebLink) {
            QDesktopServices::openUrl(QUrl(t));
            return;
        }
        if (item.linkType() == ClipboardItem::FolderLink) {
            QDesktopServices::openUrl(QUrl::fromLocalFile(t));
            return;
        }
        return;
    }

    // 文件条目：点击图标打开所在目录
    if (item.filePaths.isEmpty())
        return;

    if (item.filePaths.size() == 1) {
        // 单个文件：explorer /select 预选
        const QStringList args = {
            QStringLiteral("/select,"),
            QDir::toNativeSeparators(item.filePaths.first())
        };
        QProcess::startDetached(QStringLiteral("explorer.exe"), args);
        return;
    }

    // 多个文件/文件夹：PowerShell + Shell COM 打开目录并全选
    const QString dir = QDir::toNativeSeparators(QFileInfo(item.filePaths.first()).absolutePath());
    QString escapedDir = dir;
    escapedDir.replace(QLatin1Char('\''), QStringLiteral("''"));
    QStringList quotedNames;
    for (const QString &p : item.filePaths) {
        quotedNames << QStringLiteral("'%1'")
            .arg(QFileInfo(p).fileName().replace(QLatin1Char('\''), QStringLiteral("''")));
    }

    const QString script = QStringLiteral(
        "$dir = '%1'; "
        "$names = @(%2); "
        "$shell = New-Object -ComObject Shell.Application; "
        "$shell.Open($dir); "
        "Start-Sleep -Milliseconds 700; "
        "$win = $shell.Windows() | Where-Object { $_.Document -and $_.Document.Folder -and $_.Document.Folder.Self.Path -eq $dir } | Select-Object -First 1; "
        "if ($win) { foreach ($n in $names) { $item = $win.Document.Folder.ParseName($n); if ($item) { $win.Document.SelectItem($item, 1) } } }")
        .arg(escapedDir,
             quotedNames.join(QLatin1String(", ")));

    QProcess::startDetached(QStringLiteral("powershell.exe"),
                            QStringList() << QStringLiteral("-NoProfile")
                                          << QStringLiteral("-Command") << script);
}

void ClipboardPanel::showPanel()
{
    // 目标位置（positionNear 已设置）
    const QPoint target = pos();

    QScreen *screen = QGuiApplication::screenAt(target);
    if (!screen)
        screen = QGuiApplication::primaryScreen();
    const QRect avail = screen->availableGeometry();

    // 先移到屏幕右边缘外
    move(avail.right(), target.y());
    show();
    raise();
    applyAcrylic();

    // 从右侧滑入
    auto *anim = new QPropertyAnimation(this, "pos");
    anim->setDuration(220);
    anim->setStartValue(QPoint(avail.right(), target.y()));
    anim->setEndValue(target);
    anim->setEasingCurve(QEasingCurve::OutCubic);
    anim->start(QAbstractAnimation::DeleteWhenStopped);
}

void ClipboardPanel::slideOut()
{
    if (!isVisible())
        return;

    QScreen *screen = QGuiApplication::screenAt(pos());
    if (!screen)
        screen = QGuiApplication::primaryScreen();
    const QRect avail = screen->availableGeometry();

    auto *anim = new QPropertyAnimation(this, "pos");
    anim->setDuration(200);
    anim->setStartValue(pos());
    anim->setEndValue(QPoint(avail.right(), y()));
    anim->setEasingCurve(QEasingCurve::InCubic);
    connect(anim, &QPropertyAnimation::finished, this, &QWidget::hide);
    anim->start(QAbstractAnimation::DeleteWhenStopped);
}

void ClipboardPanel::positionNear(const QRect &trayGeometry)
{
    const int w = width();
    const int h = height();
    const int margin = 10; // 右侧与底部统一间距

    QScreen *screen = QGuiApplication::primaryScreen();
    if (!trayGeometry.isNull())
        screen = QGuiApplication::screenAt(trayGeometry.center());
    if (!screen)
        screen = QGuiApplication::primaryScreen();

    const QRect avail = screen->availableGeometry();

    // 靠屏幕右侧，右侧留白与底部离任务栏的间距一致
    const int x = avail.right() - w - margin;
    const int y = avail.bottom() - h - margin;

    move(x, y);
}

void ClipboardPanel::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    const ThemeColors c = Theme::current();
    QColor bg = c.panelBg;
    bg.setAlpha(AppSettings::glassEffect() ? 190 : 255);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const QRectF r = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    p.setPen(QPen(c.panelBorder, 1));
    p.setBrush(bg);
    p.drawRoundedRect(r, 8, 8);
}

void ClipboardPanel::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape) {
        slideOut();
        return;
    }
    QWidget::keyPressEvent(event);
}

void ClipboardPanel::hideEvent(QHideEvent *event)
{
    emit panelHidden();
    QWidget::hideEvent(event);
}

void ClipboardPanel::onCustomContextMenu(const QPoint &pos)
{
    const QModelIndex index = m_list->indexAt(pos);
    if (!index.isValid() || !m_model)
        return;

    const ClipboardItem item = m_model->itemAt(index.row());
    QMenu menu(this);
    QAction *pinAction = nullptr;
    if (!item.missing) // 失效条目不提供置顶（始终沉底）
        pinAction = menu.addAction(item.isPinned ? QStringLiteral("取消置顶") : QStringLiteral("置顶"));
    QAction *deleteAction = menu.addAction(QStringLiteral("删除"));
    QAction *chosen = menu.exec(m_list->viewport()->mapToGlobal(pos));

    if (chosen && chosen == deleteAction)
        emit deleteRequested(item.id);
    else if (pinAction && chosen == pinAction)
        emit togglePinRequested(item.id);

    raise();
}
