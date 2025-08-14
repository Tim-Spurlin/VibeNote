#pragma once

#include <QDebug>

#define LOG_DEBUG(msg) qDebug() << "[DEBUG]" << msg
#define LOG_INFO(msg) qInfo() << "[INFO]" << msg
#define LOG_WARNING(msg) qWarning() << "[WARNING]" << msg
#define LOG_ERROR(msg) qCritical() << "[ERROR]" << msg
