#include "runtime/single_instance_guard.h"

#include <QLockFile>

SingleInstanceGuard::SingleInstanceGuard(const QDir &dataDir)
    : m_lockFile(std::make_unique<QLockFile>(dataDir.filePath(QStringLiteral("cybercompanioncpp.lock"))))
{
    m_lockFile->setStaleLockTime(0);
}

SingleInstanceGuard::~SingleInstanceGuard()
{
    release();
}

bool SingleInstanceGuard::acquire()
{
    return m_lockFile->tryLock();
}

void SingleInstanceGuard::release()
{
    if (m_lockFile && m_lockFile->isLocked()) {
        m_lockFile->unlock();
    }
}
