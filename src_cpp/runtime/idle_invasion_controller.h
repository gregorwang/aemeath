#pragma once

#include <QHash>
#include <QObject>
#include <QRect>
#include <QSet>
#include <QSize>

#include "runtime/app_config.h"

class QTimer;

class IdleInvasionController final : public QObject
{
    Q_OBJECT

public:
    enum class InvasionState {
        Inactive,
        Spawning,
        Saturated,
        Retreating,
    };
    Q_ENUM(InvasionState)

    explicit IdleInvasionController(QObject *parent = nullptr);

    void applyConfig(const IdleInvasionConfig &config);
    void setDndEnabled(bool enabled);
    bool triggerDebugInvasion();
    void shutdown();

    InvasionState state() const;
    int activeCount() const;

public Q_SLOTS:
    void onIdleTimeUpdated(qint64 idleMs);
    void onUserActivityDetected();

Q_SIGNALS:
    void stateChanged(IdleInvasionController::InvasionState state);
    void activeCountChanged(int activeCount);

private Q_SLOTS:
    void onSpawnTick();
    void onRetreatTimeout();
    void onDebugForceTimeout();
    void onParticleFinished(int particleId);

private:
    void setState(InvasionState state);
    void beginSpawning();
    void armSpawnTimer();
    int currentSpawnIntervalMs() const;
    void initGrid();
    void spawnOne();
    void beginRetreat();
    void dismissAllImmediate();
    void reset();
    void refreshGifSizes();
    QStringList resolveGifPaths() const;
    QSize readScaledGifSize(const QString &gifPath) const;
    quint64 cellKey(int col, int row) const;
    QPoint targetPositionForCell(int col, int row, const QSize &gifSize) const;
    QPoint randomOffscreenStartPosition(const QPoint &endPos, const QSize &size) const;
    QList<QObject *> sortedParticlesForRipple() const;

    IdleInvasionConfig m_config;
    InvasionState m_state = InvasionState::Inactive;
    QTimer *m_spawnTimer = nullptr;
    QTimer *m_retreatTimer = nullptr;
    QTimer *m_debugForceTimer = nullptr;
    qint64 m_idleTimeMs = 0;
    bool m_invasionStarted = false;
    bool m_debugForceMode = false;
    bool m_dndEnabled = false;
    QStringList m_gifPaths;
    QHash<QString, QSize> m_gifSizes;
    int m_maxGifWidth = 0;
    int m_maxGifHeight = 0;
    int m_gridCols = 0;
    int m_gridRows = 0;
    int m_cellWidth = 0;
    int m_cellHeight = 0;
    QRect m_screenGeometry;
    QSet<quint64> m_occupied;
    QHash<int, quint64> m_particleCells;
    QHash<int, QObject *> m_particles;
    int m_nextParticleId = 0;
};

Q_DECLARE_METATYPE(IdleInvasionController::InvasionState)
