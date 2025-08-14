#ifndef CONFIG_H
#define CONFIG_H

#include <QString>
#include <QJsonObject>

class ConfigManager {
public:
    ConfigManager();
    QJsonObject load() const;
    void update(const QJsonObject& config);
    
private:
    QJsonObject m_config;
};

#endif // CONFIG_H
