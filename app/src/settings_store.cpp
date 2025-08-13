#include <QObject>
#include <QSettings>
#include <QStringList>
#include <QJsonArray>
#include <QJsonObject>
#include <QDir>
#include <algorithm>
#include <KConfig>
#include <KConfigGroup>
#include <QKeychain/ReadPasswordJob>
#include <QKeychain/WritePasswordJob>
#include <QKeychain/DeletePasswordJob>

class ApiClient; // forward declaration

class SettingsStore : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool watchModeEnabled READ watchModeEnabled WRITE setWatchModeEnabled NOTIFY watchModeEnabledChanged)
    Q_PROPERTY(double gpuThreshold READ gpuThreshold WRITE setGpuThreshold NOTIFY gpuThresholdChanged)
    Q_PROPERTY(QString primaryApiKey READ primaryApiKey WRITE setPrimaryApiKey NOTIFY primaryApiKeyChanged)
    Q_PROPERTY(QString secondaryApiKey READ secondaryApiKey WRITE setSecondaryApiKey NOTIFY secondaryApiKeyChanged)
    Q_PROPERTY(QStringList exportFormats READ exportFormats WRITE setExportFormats NOTIFY exportFormatsChanged)
    Q_PROPERTY(QString theme READ theme WRITE setTheme NOTIFY themeChanged)
    Q_PROPERTY(int captureFramerate READ captureFramerate WRITE setCaptureFramerate NOTIFY captureFramerateChanged)
    Q_PROPERTY(QString daemonUrl READ daemonUrl WRITE setDaemonUrl NOTIFY daemonUrlChanged)

public:
    explicit SettingsStore(ApiClient *apiClient, QObject *parent = nullptr);

    bool watchModeEnabled() const { return m_watchModeEnabled; }
    double gpuThreshold() const { return m_gpuThreshold; }
    QString primaryApiKey() const { return m_primaryApiKey; }
    QString secondaryApiKey() const { return m_secondaryApiKey; }
    QStringList exportFormats() const { return m_exportFormats; }
    QString theme() const { return m_theme; }
    int captureFramerate() const { return m_captureFramerate; }
    QString daemonUrl() const { return m_daemonUrl; }

    Q_INVOKABLE void loadApiKey(const QString &keyName);
    Q_INVOKABLE void saveApiKey(const QString &keyName, const QString &value);
    Q_INVOKABLE void resetToDefaults();

    QJsonObject getDaemonConfig() const;

public slots:
    void setWatchModeEnabled(bool enabled);
    void setGpuThreshold(double threshold);
    void setPrimaryApiKey(const QString &value);
    void setSecondaryApiKey(const QString &value);
    void setExportFormats(const QStringList &formats);
    void setTheme(const QString &theme);
    void setCaptureFramerate(int fps);
    void setDaemonUrl(const QString &url);

signals:
    void watchModeEnabledChanged(bool enabled);
    void gpuThresholdChanged(double threshold);
    void primaryApiKeyChanged(const QString &value);
    void secondaryApiKeyChanged(const QString &value);
    void exportFormatsChanged(const QStringList &formats);
    void themeChanged(const QString &theme);
    void captureFramerateChanged(int fps);
    void daemonUrlChanged(const QString &url);

private:
    void syncDaemonConfig();

    ApiClient *m_apiClient;
    QSettings m_qsettings;
    KConfig m_config;
    KConfigGroup m_group;

    bool m_watchModeEnabled{};
    double m_gpuThreshold{};
    QString m_primaryApiKey;
    QString m_secondaryApiKey;
    QStringList m_exportFormats;
    QString m_theme;
    int m_captureFramerate{};
    QString m_daemonUrl;
};

SettingsStore::SettingsStore(ApiClient *apiClient, QObject *parent)
    : QObject(parent),
      m_apiClient(apiClient),
      m_qsettings("VibeNote", "VibeNote"),
      m_config(QDir::homePath() + "/.config/VibeNote/config.yml", KConfig::SimpleConfig),
      m_group(&m_config, "General") {
    m_watchModeEnabled = m_group.readEntry("watchModeEnabled", false);
    m_gpuThreshold = m_group.readEntry("gpuThreshold", 80.0);
    m_exportFormats = m_group.readEntry("exportFormats", QStringList{"raw"});
    m_theme = m_group.readEntry("theme", QStringLiteral("auto"));
    m_captureFramerate = m_group.readEntry("captureFramerate", 5);
    m_daemonUrl = m_group.readEntry("daemonUrl", QStringLiteral("http://127.0.0.1:3030"));

    loadApiKey(QStringLiteral("primaryApiKey"));
    loadApiKey(QStringLiteral("secondaryApiKey"));
}

void SettingsStore::setWatchModeEnabled(bool enabled) {
    if (m_watchModeEnabled == enabled)
        return;
    m_watchModeEnabled = enabled;
    m_group.writeEntry("watchModeEnabled", m_watchModeEnabled);
    m_group.sync();
    emit watchModeEnabledChanged(m_watchModeEnabled);
    syncDaemonConfig();
}

void SettingsStore::setGpuThreshold(double threshold) {
    threshold = std::clamp(threshold, 0.0, 100.0);
    if (qFuzzyCompare(m_gpuThreshold, threshold))
        return;
    m_gpuThreshold = threshold;
    m_group.writeEntry("gpuThreshold", m_gpuThreshold);
    m_group.sync();
    emit gpuThresholdChanged(m_gpuThreshold);
    syncDaemonConfig();
}

void SettingsStore::setPrimaryApiKey(const QString &value) { saveApiKey("primaryApiKey", value); }

void SettingsStore::setSecondaryApiKey(const QString &value) { saveApiKey("secondaryApiKey", value); }

void SettingsStore::setExportFormats(const QStringList &formats) {
    if (m_exportFormats == formats)
        return;
    m_exportFormats = formats;
    m_group.writeEntry("exportFormats", m_exportFormats);
    m_group.sync();
    emit exportFormatsChanged(m_exportFormats);
    syncDaemonConfig();
}

void SettingsStore::setTheme(const QString &theme) {
    if (m_theme == theme)
        return;
    m_theme = theme;
    m_group.writeEntry("theme", m_theme);
    m_group.sync();
    emit themeChanged(m_theme);
}

void SettingsStore::setCaptureFramerate(int fps) {
    fps = std::clamp(fps, 1, 10);
    if (m_captureFramerate == fps)
        return;
    m_captureFramerate = fps;
    m_group.writeEntry("captureFramerate", m_captureFramerate);
    m_group.sync();
    emit captureFramerateChanged(m_captureFramerate);
    syncDaemonConfig();
}

void SettingsStore::setDaemonUrl(const QString &url) {
    if (m_daemonUrl == url)
        return;
    m_daemonUrl = url;
    m_group.writeEntry("daemonUrl", m_daemonUrl);
    m_group.sync();
    emit daemonUrlChanged(m_daemonUrl);
    syncDaemonConfig();
}

void SettingsStore::loadApiKey(const QString &keyName) {
    auto *job = new QKeychain::ReadPasswordJob(QStringLiteral("VibeNote"), this);
    job->setKey(keyName);
    connect(job, &QKeychain::ReadPasswordJob::finished, this, [this, keyName](QKeychain::Job *baseJob) {
        auto *readJob = qobject_cast<QKeychain::ReadPasswordJob *>(baseJob);
        if (!readJob)
            return;
        if (readJob->error()) {
            if (keyName == "primaryApiKey") {
                m_primaryApiKey.clear();
                emit primaryApiKeyChanged(m_primaryApiKey);
            } else if (keyName == "secondaryApiKey") {
                m_secondaryApiKey.clear();
                emit secondaryApiKeyChanged(m_secondaryApiKey);
            }
        } else {
            if (keyName == "primaryApiKey") {
                m_primaryApiKey = readJob->textData();
                emit primaryApiKeyChanged(m_primaryApiKey);
            } else if (keyName == "secondaryApiKey") {
                m_secondaryApiKey = readJob->textData();
                emit secondaryApiKeyChanged(m_secondaryApiKey);
            }
        }
    });
    job->start();
}

void SettingsStore::saveApiKey(const QString &keyName, const QString &value) {
    auto *job = new QKeychain::WritePasswordJob(QStringLiteral("VibeNote"), this);
    job->setKey(keyName);
    job->setTextData(value);
    connect(job, &QKeychain::WritePasswordJob::finished, this, [this, keyName, value](QKeychain::Job *baseJob) {
        auto *writeJob = qobject_cast<QKeychain::WritePasswordJob *>(baseJob);
        if (!writeJob)
            return;
        if (writeJob->error()) {
            return; // error is logged by QKeychain
        }
        if (keyName == "primaryApiKey") {
            m_primaryApiKey = value;
            emit primaryApiKeyChanged(m_primaryApiKey);
        } else if (keyName == "secondaryApiKey") {
            m_secondaryApiKey = value;
            emit secondaryApiKeyChanged(m_secondaryApiKey);
        }
    });
    job->start();
}

void SettingsStore::resetToDefaults() {
    m_group.deleteGroup();
    m_config.sync();
    m_qsettings.clear();
    m_qsettings.sync();

    m_watchModeEnabled = false;
    m_gpuThreshold = 80.0;
    m_exportFormats = QStringList{"raw"};
    m_theme = QStringLiteral("auto");
    m_captureFramerate = 5;
    m_daemonUrl = QStringLiteral("http://127.0.0.1:3030");
    m_primaryApiKey.clear();
    m_secondaryApiKey.clear();

    emit watchModeEnabledChanged(m_watchModeEnabled);
    emit gpuThresholdChanged(m_gpuThreshold);
    emit exportFormatsChanged(m_exportFormats);
    emit themeChanged(m_theme);
    emit captureFramerateChanged(m_captureFramerate);
    emit daemonUrlChanged(m_daemonUrl);
    emit primaryApiKeyChanged(m_primaryApiKey);
    emit secondaryApiKeyChanged(m_secondaryApiKey);

    auto *delPrimary = new QKeychain::DeletePasswordJob(QStringLiteral("VibeNote"), this);
    delPrimary->setKey("primaryApiKey");
    delPrimary->start();
    auto *delSecondary = new QKeychain::DeletePasswordJob(QStringLiteral("VibeNote"), this);
    delSecondary->setKey("secondaryApiKey");
    delSecondary->start();

    syncDaemonConfig();
}

QJsonObject SettingsStore::getDaemonConfig() const {
    QJsonObject obj;
    obj.insert(QStringLiteral("watchModeEnabled"), m_watchModeEnabled);
    obj.insert(QStringLiteral("gpuThreshold"), m_gpuThreshold);
    obj.insert(QStringLiteral("exportFormats"), QJsonArray::fromStringList(m_exportFormats));
    obj.insert(QStringLiteral("captureFramerate"), m_captureFramerate);
    obj.insert(QStringLiteral("daemonUrl"), m_daemonUrl);
    return obj;
}

void SettingsStore::syncDaemonConfig() {
    if (m_apiClient) {
        m_apiClient->updateConfig(getDaemonConfig());
    }
}

#include "settings_store.moc"
