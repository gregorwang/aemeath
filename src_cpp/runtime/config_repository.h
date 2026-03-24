#pragma once

#include <QString>

#include "runtime/app_config.h"

class ConfigRepository
{
public:
    explicit ConfigRepository(QString configFilePath);

    AppConfig load() const;
    bool save(const AppConfig &config, QString *errorMessage = nullptr) const;
    bool exists() const;
    bool bootstrapFromLegacy(const QString &legacyConfigFilePath) const;

private:
    QString m_configFilePath;
};
