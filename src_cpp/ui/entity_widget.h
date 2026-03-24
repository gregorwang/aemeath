#pragma once

#include <QEasingCurve>
#include <QPoint>
#include <QRect>
#include <QString>
#include <QWidget>

class QAbstractAnimation;
class CharacterAssetCatalog;
class QEnterEvent;
class QEvent;
class QHideEvent;
class QLabel;
class QMouseEvent;
class QMovie;
class QPauseAnimation;
class QPropertyAnimation;
class QSequentialAnimationGroup;
class QShowEvent;
class QTimer;

#include "ui/entity_positions.h"

class EntityWidget : public QWidget
{
    Q_OBJECT

public:
    enum class VisualState {
        Hidden,
        Idle,
        Peeking,
        Engaged,
        Fleeing,
        Commentary
    };

    explicit EntityWidget(QWidget *parent = nullptr);
    ~EntityWidget() override;

    VisualState visualState() const;
    QString renderedStateName() const;
    void moveToDefaultCorner();
    void restoreWindowPosition(int x, int y);
    void setStatus(const QString &title, const QString &detail);
    void setScreenEdge(EntityPositions::Edge edge);
    void applyAppearanceConfig(int asciiWidth, int fontSizePx);
    void setVisualState(VisualState state);
    bool setStateByName(const QString &stateName, bool asBase = true);
    void setCharacterAssetRoot(const QString &characterRoot, const QString &previewImagePath = QString());
    void setScriptVisualOverride(const QString &spritePath, const QString &animSpeed = QString());
    void clearScriptVisualOverride();
    void setAutonomousEnabled(bool enabled);
    void applyGazeFollow(
        double faceX,
        double faceY = 0.0,
        bool faceDetected = true,
        double confidence = 1.0);
    void transitionToVisualState(VisualState state);
    void peek();
    void enter();
    void summon();
    void flee();
    void hideNow();

Q_SIGNALS:
    void peekCompleted();
    void enterCompleted();
    void fleeCompleted();
    void doubleClicked();
    void contextMenuRequested(const QPoint &globalPos);

protected:
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;

private:
    enum class MotionKind {
        None,
        Peek,
        Enter,
        Flee,
        Summon,
        Roam,
        Probe
    };

    QString composeTargetStateName() const;
    void applyVisualState();
    void animateBetween(
        const QPoint &start,
        const QPoint &end,
        int durationMs,
        MotionKind motionKind,
        QEasingCurve::Type curve);
    QPoint clampPointToCurrentScreen(
        const QPoint &point,
        double visibleXRatio = 1.0,
        double visibleYRatio = 1.0) const;
    EntityPositions currentPositions() const;
    QRect currentAvailableGeometry() const;
    int dragThreshold() const;
    void ensureSummonSequence();
    void ensureRoamAnimation();
    void ensureProbeSequence();
    void onDirectMotionFinished();
    void onSummonSequenceFinished();
    void onRoamTimeout();
    void onRoamFinished();
    void onProbeTimeout();
    void onProbeFinished();
    void onClickRestoreTimeout();
    void scheduleAutonomyTimers();
    void stopAutonomyTimers();
    void stopRoamAnimation();
    void stopProbeAnimation();
    bool canStartAutonomousAction() const;
    void refreshSprite();
    int movieSpeedPercentForAnimSpeed(const QString &animSpeed) const;
    QString gifPathForState(VisualState state) const;
    int targetY() const;
    void stopMotionAnimation();

    QLabel *m_spriteLabel = nullptr;
    QLabel *m_titleLabel = nullptr;
    QLabel *m_detailLabel = nullptr;
    bool m_dragging = false;
    bool m_dragStarted = false;
    bool m_hoverActive = false;
    bool m_autonomousEnabled = false;
    bool m_movingActive = false;
    QPoint m_dragOffset;
    QPoint m_leftPressGlobal;
    VisualState m_visualState = VisualState::Idle;
    QString m_baseStateName = QStringLiteral("state1");
    QString m_renderedStateName = QStringLiteral("state1");
    QString m_clickOverrideStateName;
    QString m_probeStateName;
    QString m_scriptSpritePath;
    QString m_scriptAnimSpeed;
    CharacterAssetCatalog *m_assetCatalog = nullptr;
    int m_asciiWidth = 60;
    int m_fontSizePx = 8;
    QMovie *m_movie = nullptr;
    QPropertyAnimation *m_motionAnimation = nullptr;
    QSequentialAnimationGroup *m_summonSequence = nullptr;
    QPropertyAnimation *m_summonPeekAnimation = nullptr;
    QPauseAnimation *m_summonPause = nullptr;
    QPropertyAnimation *m_summonEnterAnimation = nullptr;
    QTimer *m_roamTimer = nullptr;
    QTimer *m_probeTimer = nullptr;
    QTimer *m_clickRestoreTimer = nullptr;
    QPropertyAnimation *m_roamAnimation = nullptr;
    QSequentialAnimationGroup *m_probeSequence = nullptr;
    QPropertyAnimation *m_probeOutAnimation = nullptr;
    QPauseAnimation *m_probePause = nullptr;
    QPropertyAnimation *m_probeBackAnimation = nullptr;
    EntityPositions::Edge m_edge = EntityPositions::Edge::Right;
    MotionKind m_activeMotion = MotionKind::None;
};
