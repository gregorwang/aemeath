#pragma once

#include "services/service_contracts.h"

class QTimer;

class IdleMonitorServiceNative final : public IdleMonitorService
{
    Q_OBJECT

public:
    explicit IdleMonitorServiceNative(QObject *parent = nullptr);

public Q_SLOTS:
    void start() override;
    void stop() override;
    void setThresholdMs(int thresholdMs) override;
    void resetToStandby() override;

private Q_SLOTS:
    void pollIdleState();

private:
    enum class IdleState {
        Standby,
        PreIdle,
        IdleTriggered,
        Active
    };

    qint64 currentIdleMs() const;
    void setState(IdleState state);

    QTimer *m_pollTimer = nullptr;
    IdleState m_state = IdleState::Standby;
    int m_idleThresholdMs = 180'000;
};
