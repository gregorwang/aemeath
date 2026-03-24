#pragma once

#include <memory>

#include <QDir>

class QLockFile;

class SingleInstanceGuard
{
public:
    explicit SingleInstanceGuard(const QDir &dataDir);
    ~SingleInstanceGuard();

    bool acquire();
    void release();

private:
    std::unique_ptr<QLockFile> m_lockFile;
};
