#ifndef SETTINGS_STORE_H
#define SETTINGS_STORE_H

#include <QObject>
#include <QVariant>
#include <QStringList>

class SettingsStore : public QObject {
    Q_OBJECT

public:
    explicit SettingsStore(QObject *parent = nullptr);
    ~SettingsStore();

    QVariant value(const QString &key, const QVariant &defaultValue = QVariant()) const;
    void setValue(const QString &key, const QVariant &value);
    void remove(const QString &key);
    void clear();
    bool contains(const QString &key) const;
    QStringList allKeys() const;

Q_SIGNALS:
    void valueChanged(const QString &key, const QVariant &value);

private:
    void load();
    void save();
    
    class Private;
    Private *d;
};

#endif // SETTINGS_STORE_H
