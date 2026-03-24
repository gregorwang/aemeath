#pragma once

#include <QTimer>

#include "services/service_contracts.h"

class WindowsFullscreenMonitorService final : public FullscreenMonitorService
{
    Q_OBJECT

public:
    explicit WindowsFullscreenMonitorService(QObject *parent = nullptr);

    bool isFullscreenActive() const override;

public Q_SLOTS:
    void start() override;
    void stop() override;

private Q_SLOTS:
    void poll();

private:
    bool detectFullscreen() const;

    QTimer m_timer;
    bool m_running = false;
    bool m_isFullscreenActive = false;
};
