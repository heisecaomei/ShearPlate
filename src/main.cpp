#include <QApplication>
#include <QCoreApplication>

#include "ClipboardApp.h"

int main(int argc, char *argv[])
{
    // 用于 QSettings / QStandardPaths（数据落盘到用户目录）
    QCoreApplication::setOrganizationName(QStringLiteral("ShearPlate"));
    QCoreApplication::setOrganizationDomain(QStringLiteral("shearplate.local"));
    QCoreApplication::setApplicationName(QStringLiteral("ShearPlate"));
    QCoreApplication::setApplicationVersion(QStringLiteral("0.1.11"));

    QApplication app(argc, argv);
    // 关闭所有窗口后程序仍驻留后台（常驻托盘）
    app.setQuitOnLastWindowClosed(false);

    ClipboardApp clipboardApp;
    // 单实例：若已有实例在运行，本次启动退出并通知已有实例激活面板
    if (!clipboardApp.acquireSingleInstance())
        return 0;

    return app.exec();
}
