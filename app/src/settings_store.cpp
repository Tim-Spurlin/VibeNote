#include "settings_store.h"
#include <QSettings>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>

class SettingsStore::Private {
public:
    QSettings settings;
    QJsonObject cache;
    QString configPath;
};

SettingsStore::SettingsStore(QObject *parent)
    : QObject(parent), d(new Private) {
    
    QString configDir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    QDir().mkpath(configDir);
    d->configPath = configDir + QStringLiteral("/settings.json");
    
    load();
}

SettingsStore::~SettingsStore() {
    save();
    delete d;
}

void SettingsStore::load() {
    QFile file(d->configPath);
    if (file.open(QIODevice::ReadOnly)) {
        QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
        d->cache = doc.object();
    }
}

void SettingsStore::save() {
    QFile file(d->configPath);
    if (file.open(QIODevice::WriteOnly)) {
        QJsonDocument doc(d->cache);
        file.write(doc.toJson());
    }
}

QVariant SettingsStore::value(const QString &key, const QVariant &defaultValue) const {
    if (d->cache.contains(key)) {
        return d->cache[key].toVariant();
    }
    return defaultValue;
}

void SettingsStore::setValue(const QString &key, const QVariant &value) {
    d->cache[key] = QJsonValue::fromVariant(value);
    save();
    Q_EMIT valueChanged(key, value);
}

void SettingsStore::remove(const QString &key) {
    d->cache.remove(key);
    save();
}

void SettingsStore::clear() {
    d->cache = QJsonObject();
    save();
}

bool SettingsStore::contains(const QString &key) const {
    return d->cache.contains(key);
}

QStringList SettingsStore::allKeys() const {
    return d->cache.keys();
}
