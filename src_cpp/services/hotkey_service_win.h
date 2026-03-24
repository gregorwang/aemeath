#pragma once

#include "services/service_contracts.h"

#include <QAbstractNativeEventFilter>

class QApplication;

class WindowsHotkeyService final : public HotkeyService, public QAbstractNativeEventFilter
{
    Q_OBJECT

public:
    explicit WindowsHotkeyService(QApplication &app, QObject *parent = nullptr);
    ~WindowsHotkeyService() override;

    bool isSummonRegistered() const override;
    bool isPushToTalkRegistered() const override;
    void start() override;
    void stop() override;
    bool nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result) override;

private:
    QApplication &m_app;
    bool m_started = false;
    bool m_summonRegistered = false;
    bool m_pushToTalkRegistered = false;
};
