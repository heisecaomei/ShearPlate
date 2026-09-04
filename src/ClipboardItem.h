#pragma once

#include <QDate>
#include <QDateTime>
#include <QFileInfo>
#include <QImage>
#include <QMetaType>
#include <QString>
#include <QStringList>

// 一条剪贴板记录。轻量：文本存 text，图片存磁盘路径 imagePath，文件存路径列表 filePaths。
// image 字段仅用于“刚捕获、尚未落盘”的图片，落盘后即清空以节省内存。
class ClipboardItem
{
public:
    enum Type {
        Text = 0,
        Image = 1,
        Files = 2
    };

    QString id;
    Type type = Text;
    QString text;          // 文本内容（Text 类型）
    QString imagePath;     // 图片绝对路径（Image 类型）
    QStringList filePaths; // 本地文件路径（Files 类型）
    qint64 timestampMs = 0;
    QImage image;          // 暂存：捕获时的新图片
    bool screenshot = false; // 是否为截图
    bool isPinned = false; // 是否置顶

    // 文件类型分类（用于多文件预览文案）
    static QString fileCategory(const QString &path)
    {
        const QFileInfo fi(path);
        if (fi.isDir())
            return QStringLiteral("文件夹");

        const QString ext = fi.suffix().toLower();

        static const QStringList videos = {
            QStringLiteral("mp4"), QStringLiteral("avi"), QStringLiteral("mkv"),
            QStringLiteral("mov"), QStringLiteral("wmv"), QStringLiteral("flv"),
            QStringLiteral("mpeg"), QStringLiteral("mpg"), QStringLiteral("webm"),
            QStringLiteral("rmvb"), QStringLiteral("ts"), QStringLiteral("m4v"),
            QStringLiteral("3gp"), QStringLiteral("rm")
        };
        static const QStringList audios = {
            QStringLiteral("mp3"), QStringLiteral("wav"), QStringLiteral("flac"),
            QStringLiteral("aac"), QStringLiteral("ogg"), QStringLiteral("wma"),
            QStringLiteral("m4a"), QStringLiteral("ape")
        };
        static const QStringList images = {
            QStringLiteral("png"), QStringLiteral("jpg"), QStringLiteral("jpeg"),
            QStringLiteral("gif"), QStringLiteral("bmp"), QStringLiteral("webp"),
            QStringLiteral("svg"), QStringLiteral("ico"), QStringLiteral("tiff"),
            QStringLiteral("tif")
        };
        static const QStringList executables = {
            QStringLiteral("exe"), QStringLiteral("msi"), QStringLiteral("bat"),
            QStringLiteral("cmd"), QStringLiteral("com"), QStringLiteral("scr")
        };
        static const QStringList documents = {
            QStringLiteral("doc"), QStringLiteral("docx"), QStringLiteral("xls"),
            QStringLiteral("xlsx"), QStringLiteral("ppt"), QStringLiteral("pptx"),
            QStringLiteral("pdf"), QStringLiteral("txt"), QStringLiteral("md"),
            QStringLiteral("csv"), QStringLiteral("rtf")
        };
        static const QStringList archives = {
            QStringLiteral("zip"), QStringLiteral("rar"), QStringLiteral("7z"),
            QStringLiteral("tar"), QStringLiteral("gz"), QStringLiteral("bz2")
        };

        if (videos.contains(ext)) return QStringLiteral("视频");
        if (audios.contains(ext)) return QStringLiteral("音频");
        if (images.contains(ext)) return QStringLiteral("图片");
        if (executables.contains(ext)) return QStringLiteral("可执行文件");
        if (documents.contains(ext)) return QStringLiteral("文档");
        if (archives.contains(ext)) return QStringLiteral("压缩文件");
        return QStringLiteral("文件");
    }

    // 列表预览文案
    QString preview() const
    {
        if (type == Image)
            return QStringLiteral("图片");

        if (type == Files) {
            const int count = filePaths.size();
            if (count == 1)
                return QFileInfo(filePaths.first()).fileName();

            // 多个：统计分类
            bool allDirs = true;
            bool allSame = true;
            QString category;
            for (const QString &p : filePaths) {
                const QString cat = fileCategory(p);
                if (cat != QStringLiteral("文件夹"))
                    allDirs = false;
                if (category.isEmpty())
                    category = cat;
                else if (category != cat)
                    allSame = false;
            }
            if (allDirs)
                return QStringLiteral("共%1个文件夹").arg(count);
            if (allSame && !category.isEmpty())
                return QStringLiteral("共%1个%2").arg(count).arg(category);
            return QStringLiteral("共%1个文件").arg(count);
        }

        QString s = text;
        s.replace(QStringLiteral("\r\n"), QStringLiteral(" "));
        s.replace(QLatin1Char('\n'), QLatin1Char(' '));
        s.replace(QLatin1Char('\r'), QLatin1Char(' '));
        s = s.trimmed();
        if (s.length() > 120)
            s = s.left(120) + QStringLiteral("…");
        return s.isEmpty() ? QStringLiteral("(空文本)") : s;
    }

    // 第一个文件路径（Files 类型，用于取图标、打开所在目录）
    QString firstFilePath() const
    {
        return filePaths.isEmpty() ? QString() : filePaths.first();
    }

    // 是否使用 folder.png 图标：仅当剪贴板中为“多个不同类型”混合时；
    // 同类型（含多个同类型文件、文件夹、单文件）使用系统默认图标。
    bool useFolderIcon() const
    {
        if (type != Files || filePaths.isEmpty())
            return false;

        QString category;
        for (const QString &p : filePaths) {
            const QString cat = fileCategory(p);
            if (category.isEmpty())
                category = cat;
            else if (category != cat)
                return true; // 混合类型
        }
        return false;
    }

    // 时间展示（保留，供其它场景使用）
    QString displayTime() const
    {
        const QDateTime dt = QDateTime::fromMSecsSinceEpoch(timestampMs);
        if (dt.date() == QDate::currentDate())
            return dt.toString(QStringLiteral("HH:mm:ss"));
        return dt.toString(QStringLiteral("MM-dd HH:mm"));
    }

    enum LinkType {
        NoLink = 0,
        WebLink = 1,    // 网页链接
        FolderLink = 2  // 本地路径（文件夹/文件）
    };

    // 判断文本是否为网页链接或本地路径
    LinkType linkType() const
    {
        if (type != Text)
            return NoLink;
        const QString t = text.trimmed();
        if (t.isEmpty())
            return NoLink;
        if (t.startsWith(QLatin1String("http://"), Qt::CaseInsensitive)
            || t.startsWith(QLatin1String("https://"), Qt::CaseInsensitive)
            || t.startsWith(QLatin1String("www."), Qt::CaseInsensitive))
            return WebLink;
        if (!t.contains(QLatin1Char('\n'))) {
            const QFileInfo fi(t);
            if (fi.isAbsolute() && fi.exists()) // 仅绝对路径才视为文件夹地址
                return FolderLink;
        }
        return NoLink;
    }

    // 判断与另一条记录内容是否一致（用于去重：相同则不新增，改为刷新时间并置顶）
    bool sameAs(const ClipboardItem &other) const
    {
        if (type != other.type)
            return false;
        if (type == Text)
            return text == other.text;
        if (type == Files) {
            if (filePaths.size() != other.filePaths.size())
                return false;
            QStringList a = filePaths;
            QStringList b = other.filePaths;
            a.sort();
            b.sort();
            return a == b;
        }
        if (type == Image)
            return imagePath == other.imagePath;
        return false;
    }
};

Q_DECLARE_METATYPE(ClipboardItem)
