#include "ClipboardStore.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QStandardPaths>

#include <algorithm>

ClipboardStore::ClipboardStore(QObject *parent)
    : QObject(parent)
{
    QDir().mkpath(dataDir());
}

QString ClipboardStore::dataDir() const
{
    const QString base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    return base + QStringLiteral("/clips");
}

QString ClipboardStore::jsonPath(const QString &id) const
{
    return QDir(dataDir()).filePath(id + QStringLiteral(".json"));
}

QString ClipboardStore::imagePath(const QString &id) const
{
    return QDir(dataDir()).filePath(id + QStringLiteral(".png"));
}

void ClipboardStore::setRetentionHours(int hours)
{
    m_retentionHours = qMax(1, hours);
}

int ClipboardStore::retentionHours() const
{
    return m_retentionHours;
}

QList<ClipboardItem> ClipboardStore::loadAll() const
{
    QList<ClipboardItem> items;
    const QDir dir(dataDir());
    const QStringList files = dir.entryList(QStringList() << QStringLiteral("*.json"), QDir::Files);

    for (const QString &fileName : files) {
        QFile f(dir.absoluteFilePath(fileName));
        if (!f.open(QIODevice::ReadOnly))
            continue;

        QJsonParseError err;
        const QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &err);
        f.close();

        if (err.error != QJsonParseError::NoError || !doc.isObject())
            continue;

        const QJsonObject obj = doc.object();

        ClipboardItem item;
        item.id = obj.value(QStringLiteral("id")).toString();
        if (item.id.isEmpty())
            item.id = QFileInfo(fileName).completeBaseName();

        item.timestampMs = static_cast<qint64>(obj.value(QStringLiteral("timestampMs")).toDouble());
        item.isPinned = obj.value(QStringLiteral("pinned")).toBool();

        const QString type = obj.value(QStringLiteral("type")).toString();
        if (type == QStringLiteral("image")) {
            item.type = ClipboardItem::Image;
            item.imagePath = dir.absoluteFilePath(obj.value(QStringLiteral("image")).toString());
            item.screenshot = obj.value(QStringLiteral("screenshot")).toBool();
        } else if (type == QStringLiteral("files")) {
            item.type = ClipboardItem::Files;
            const QJsonArray arr = obj.value(QStringLiteral("files")).toArray();
            for (const QJsonValue &v : arr)
                item.filePaths << v.toString();
        } else {
            item.type = ClipboardItem::Text;
            item.text = obj.value(QStringLiteral("text")).toString();
        }

        items.append(item);
    }

    std::sort(items.begin(), items.end(), [](const ClipboardItem &a, const ClipboardItem &b) {
        return a.timestampMs > b.timestampMs; // 新的在前
    });

    return items;
}

bool ClipboardStore::save(ClipboardItem &item)
{
    QJsonObject obj;
    obj.insert(QStringLiteral("id"), item.id);
    obj.insert(QStringLiteral("timestampMs"), static_cast<double>(item.timestampMs));
    obj.insert(QStringLiteral("pinned"), item.isPinned);

    if (item.type == ClipboardItem::Image) {
        item.imagePath = imagePath(item.id);
        if (!item.image.isNull()) {
            if (!item.image.save(item.imagePath, "PNG"))
                return false;
        }
        obj.insert(QStringLiteral("type"), QStringLiteral("image"));
        obj.insert(QStringLiteral("image"), QFileInfo(item.imagePath).fileName());
        obj.insert(QStringLiteral("screenshot"), item.screenshot);
    } else if (item.type == ClipboardItem::Files) {
        obj.insert(QStringLiteral("type"), QStringLiteral("files"));
        QJsonArray arr;
        for (const QString &p : item.filePaths)
            arr.append(p);
        obj.insert(QStringLiteral("files"), arr);
    } else {
        obj.insert(QStringLiteral("type"), QStringLiteral("text"));
        obj.insert(QStringLiteral("text"), item.text);
    }

    QFile f(jsonPath(item.id));
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;
    f.write(QJsonDocument(obj).toJson(QJsonDocument::Compact));
    f.close();
    return true;
}

bool ClipboardStore::remove(const QString &id)
{
    bool ok = true;
    if (QFile::exists(jsonPath(id)))
        ok = QFile::remove(jsonPath(id)) && ok;
    if (QFile::exists(imagePath(id)))
        ok = QFile::remove(imagePath(id)) && ok;
    return ok;
}

bool ClipboardStore::refreshTimestamp(ClipboardItem &item)
{
    item.timestampMs = QDateTime::currentMSecsSinceEpoch();
    return save(item);
}

QStringList ClipboardStore::cleanupExpired()
{
    QStringList removed;
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const qint64 expireMs = static_cast<qint64>(m_retentionHours) * 3600000LL;

    const QDir dir(dataDir());
    const QStringList files = dir.entryList(QStringList() << QStringLiteral("*.json"), QDir::Files);

    for (const QString &fileName : files) {
        const QString path = dir.absoluteFilePath(fileName);
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly))
            continue;
        QJsonParseError err;
        const QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &err);
        f.close();
        if (err.error != QJsonParseError::NoError || !doc.isObject())
            continue;

        const qint64 ts = static_cast<qint64>(doc.object().value(QStringLiteral("timestampMs")).toDouble());
        if (now - ts > expireMs) {
            const QString id = QFileInfo(fileName).completeBaseName();
            remove(id);
            removed << id;
        }
    }

    return removed;
}
