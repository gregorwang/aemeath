#include "ui/entity_widget.h"

#include <QAbstractAnimation>
#include <QApplication>
#include <QEnterEvent>
#include <QEvent>
#include <QFileInfo>
#include <QGuiApplication>
#include <QHideEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QMovie>
#include <QPixmap>
#include <QPauseAnimation>
#include <QPropertyAnimation>
#include <QRandomGenerator>
#include <QScreen>
#include <QSequentialAnimationGroup>
#include <QShowEvent>
#include <QTimer>
#include <QtGlobal>
#include <QVBoxLayout>

#include "runtime/character_asset_catalog.h"

namespace {

constexpr int kDefaultMargin = 24;
constexpr int kPeekDurationMs = 680;
constexpr int kEnterDurationMs = 620;
constexpr int kFleeDurationMs = 480;
constexpr int kSummonPeekDurationMs = 760;
constexpr int kSummonPauseMs = 320;
constexpr int kSummonEnterDurationMs = 560;
constexpr int kRoamMinMs = 1600;
constexpr int kRoamMaxMs = 3200;
constexpr int kProbeOutMinMs = 700;
constexpr int kProbeOutMaxMs = 1200;
constexpr int kProbePauseMinMs = 400;
constexpr int kProbePauseMaxMs = 900;
constexpr int kProbeBackMinMs = 800;
constexpr int kProbeBackMaxMs = 1300;
constexpr int kRoamIntervalMinMs = 10000;
constexpr int kRoamIntervalMaxMs = 20000;
constexpr int kProbeIntervalMinMs = 14000;
constexpr int kProbeIntervalMaxMs = 24000;
constexpr int kClickRestoreMs = 3000;
constexpr double kGazeDeadzone = 0.08;
constexpr double kGazeSmoothing = 0.28;
constexpr double kGazeMaxXRatio = 0.22;
constexpr double kGazeMaxYRatio = 0.12;

int preferredWidgetWidthPx(int asciiWidth)
{
    return qBound(260, asciiWidth * 6, 720);
}

QString visualDefaultState(EntityWidget::VisualState state)
{
    switch (state) {
    case EntityWidget::VisualState::Hidden:
    case EntityWidget::VisualState::Idle:
        return QStringLiteral("state1");
    case EntityWidget::VisualState::Peeking:
        return QStringLiteral("state5");
    case EntityWidget::VisualState::Engaged:
        return QStringLiteral("state1");
    case EntityWidget::VisualState::Fleeing:
        return QStringLiteral("state4");
    case EntityWidget::VisualState::Commentary:
        return QStringLiteral("state2");
    }
    return QStringLiteral("state1");
}

}

EntityWidget::EntityWidget(QWidget *parent)
    : QWidget(parent)
    , m_assetCatalog(new CharacterAssetCatalog())
{
    m_assetCatalog->scanDefaultLocations();

    m_motionAnimation = new QPropertyAnimation(this, "pos", this);
    connect(m_motionAnimation, &QPropertyAnimation::finished, this, &EntityWidget::onDirectMotionFinished);

    m_roamTimer = new QTimer(this);
    m_roamTimer->setSingleShot(true);
    connect(m_roamTimer, &QTimer::timeout, this, &EntityWidget::onRoamTimeout);

    m_probeTimer = new QTimer(this);
    m_probeTimer->setSingleShot(true);
    connect(m_probeTimer, &QTimer::timeout, this, &EntityWidget::onProbeTimeout);

    m_clickRestoreTimer = new QTimer(this);
    m_clickRestoreTimer->setSingleShot(true);
    connect(m_clickRestoreTimer, &QTimer::timeout, this, &EntityWidget::onClickRestoreTimeout);

    setWindowFlags(
        Qt::FramelessWindowHint
        | Qt::WindowStaysOnTopHint
        | Qt::Tool
        | Qt::WindowDoesNotAcceptFocus);
    setAttribute(Qt::WA_TranslucentBackground, true);
    setAttribute(Qt::WA_ShowWithoutActivating, true);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(18, 18, 18, 18);
    layout->setSpacing(0);

    m_spriteLabel = new QLabel(this);
    m_spriteLabel->setObjectName(QStringLiteral("spriteLabel"));
    m_spriteLabel->setAlignment(Qt::AlignCenter);
    m_spriteLabel->setMinimumSize(220, 140);
    m_spriteLabel->setStyleSheet(QStringLiteral("QLabel { background: transparent; }"));

    m_titleLabel = new QLabel(QStringLiteral("CyberCompanionCpp"), this);
    m_titleLabel->setObjectName(QStringLiteral("titleLabel"));
    m_titleLabel->setAlignment(Qt::AlignCenter);

    m_detailLabel = new QLabel(QStringLiteral("原生 C++/Qt 运行时已接管语音、视觉、配置与行为链"), this);
    m_detailLabel->setObjectName(QStringLiteral("detailLabel"));
    m_detailLabel->setAlignment(Qt::AlignCenter);

    layout->addWidget(m_spriteLabel);
    layout->addWidget(m_titleLabel);
    layout->addWidget(m_detailLabel);

    resize(300, 250);
    applyAppearanceConfig(m_asciiWidth, m_fontSizePx);
    ensureSummonSequence();
    ensureRoamAnimation();
    ensureProbeSequence();
    applyVisualState();
    refreshSprite();
}

EntityWidget::~EntityWidget()
{
    delete m_movie;
    delete m_assetCatalog;
}

EntityWidget::VisualState EntityWidget::visualState() const
{
    return m_visualState;
}

QString EntityWidget::renderedStateName() const
{
    return m_renderedStateName;
}

void EntityWidget::moveToDefaultCorner()
{
    const QRect available = currentAvailableGeometry();
    if (!available.isValid()) {
        return;
    }

    const EntityPositions positions = EntityPositions::calculate(
        available,
        size(),
        available.bottom() - height() - kDefaultMargin,
        kDefaultMargin,
        m_edge);
    move(positions.engaged);
}

void EntityWidget::restoreWindowPosition(int x, int y)
{
    const QRect available = currentAvailableGeometry();
    if (!available.isValid()) {
        moveToDefaultCorner();
        return;
    }

    if (x != -1 && y != -1) {
        move(EntityPositions::clampPointToScreen(available, size(), QPoint(x, y)));
        return;
    }

    move(currentPositions().engaged);
}

void EntityWidget::setStatus(const QString &title, const QString &detail)
{
    if (m_titleLabel) {
        m_titleLabel->setText(title);
    }
    if (m_detailLabel) {
        m_detailLabel->setText(detail);
    }
}

void EntityWidget::setScreenEdge(EntityPositions::Edge edge)
{
    m_edge = edge;
}

void EntityWidget::applyAppearanceConfig(int asciiWidth, int fontSizePx)
{
    m_asciiWidth = qMax(20, asciiWidth);
    m_fontSizePx = qMax(6, fontSizePx);

    const int targetWidth = preferredWidgetWidthPx(m_asciiWidth);
    const int targetHeight = qMax(250, height());
    setMinimumWidth(targetWidth);
    resize(targetWidth, targetHeight);
    applyVisualState();
}

void EntityWidget::setVisualState(VisualState state)
{
    m_visualState = state;
    applyVisualState();
    refreshSprite();
}

bool EntityWidget::setStateByName(const QString &stateName, bool asBase)
{
    if (!m_assetCatalog) {
        return false;
    }

    const QString normalized = m_assetCatalog->normalizeStateName(stateName);
    if (normalized.isEmpty()) {
        return false;
    }

    if (asBase) {
        m_baseStateName = normalized;
    } else {
        m_clickOverrideStateName = normalized;
        m_clickRestoreTimer->start(kClickRestoreMs);
    }
    refreshSprite();
    return true;
}

void EntityWidget::setCharacterAssetRoot(const QString &characterRoot, const QString &previewImagePath)
{
    if (!m_assetCatalog) {
        return;
    }

    m_assetCatalog->clear();
    m_assetCatalog->scanDefaultLocations();
    m_assetCatalog->scanCharacterDirectory(characterRoot, previewImagePath);
    refreshSprite();
}

void EntityWidget::setScriptVisualOverride(const QString &spritePath, const QString &animSpeed)
{
    m_scriptSpritePath = spritePath.trimmed();
    m_scriptAnimSpeed = animSpeed.trimmed();
    refreshSprite();
}

void EntityWidget::clearScriptVisualOverride()
{
    if (m_scriptSpritePath.isEmpty() && m_scriptAnimSpeed.isEmpty()) {
        return;
    }
    m_scriptSpritePath.clear();
    m_scriptAnimSpeed.clear();
    refreshSprite();
}

void EntityWidget::setAutonomousEnabled(bool enabled)
{
    m_autonomousEnabled = enabled;
    if (!m_autonomousEnabled) {
        stopAutonomyTimers();
        stopRoamAnimation();
        stopProbeAnimation();
        refreshSprite();
        return;
    }
    scheduleAutonomyTimers();
}

void EntityWidget::applyGazeFollow(
    double faceX,
    double faceY,
    bool faceDetected,
    double confidence)
{
    if (!isVisible() || m_dragging) {
        return;
    }
    if (m_visualState != VisualState::Peeking
        && m_visualState != VisualState::Engaged
        && m_visualState != VisualState::Commentary) {
        return;
    }
    if (m_activeMotion != MotionKind::None || m_movingActive || !m_probeStateName.isEmpty()) {
        return;
    }

    const EntityPositions positions = currentPositions();
    QPoint base = positions.engaged;
    if (m_visualState == VisualState::Peeking) {
        base = positions.peeking;
    }

    double clampedX = faceDetected ? qBound(-1.0, faceX, 1.0) : 0.0;
    double clampedY = faceDetected ? qBound(-1.0, faceY, 1.0) : 0.0;
    if (qAbs(clampedX) < kGazeDeadzone) {
        clampedX = 0.0;
    }
    if (qAbs(clampedY) < kGazeDeadzone) {
        clampedY = 0.0;
    }

    const int maxXOffset = qMax(24, static_cast<int>(width() * kGazeMaxXRatio));
    const int maxYOffset = qMax(10, static_cast<int>(height() * kGazeMaxYRatio));
    const QPoint target = clampPointToCurrentScreen(
        QPoint(
            base.x() + static_cast<int>(clampedX * maxXOffset),
            base.y() + static_cast<int>(clampedY * maxYOffset)));

    const QPoint current = pos();
    const double blend = kGazeSmoothing * qBound(0.2, confidence, 1.0);
    const QPoint nextPoint = clampPointToCurrentScreen(
        QPoint(
            current.x() + static_cast<int>((target.x() - current.x()) * blend),
            current.y() + static_cast<int>((target.y() - current.y()) * blend)));
    if (nextPoint != current) {
        move(nextPoint);
    }
}

void EntityWidget::transitionToVisualState(VisualState state)
{
    const VisualState previous = m_visualState;

    if (state == VisualState::Hidden) {
        hideNow();
        return;
    }

    if (state == VisualState::Fleeing) {
        flee();
        return;
    }

    if (state == VisualState::Peeking) {
        if (previous == VisualState::Hidden || !isVisible()) {
            peek();
            return;
        }
        setVisualState(VisualState::Peeking);
        animateBetween(pos(), currentPositions().peeking, kPeekDurationMs, MotionKind::Peek, QEasingCurve::OutCubic);
        return;
    }

    if (state == VisualState::Engaged || state == VisualState::Commentary) {
        if (previous == VisualState::Hidden || !isVisible()) {
            setVisualState(state);
            summon();
            return;
        }
        setVisualState(state);
        enter();
        return;
    }

    setVisualState(VisualState::Idle);
    animateBetween(pos(), currentPositions().engaged, kEnterDurationMs, MotionKind::Enter, QEasingCurve::OutCubic);
}

void EntityWidget::peek()
{
    stopMotionAnimation();
    stopAutonomyTimers();
    setVisualState(VisualState::Peeking);
    const EntityPositions positions = currentPositions();
    move(positions.hidden);
    show();
    raise();
    animateBetween(positions.hidden, positions.peeking, kPeekDurationMs, MotionKind::Peek, QEasingCurve::OutCubic);
}

void EntityWidget::enter()
{
    stopAutonomyTimers();
    const VisualState state = (m_visualState == VisualState::Commentary)
        ? VisualState::Commentary
        : VisualState::Engaged;
    setVisualState(state);
    show();
    raise();
    animateBetween(pos(), currentPositions().engaged, kEnterDurationMs, MotionKind::Enter, QEasingCurve::OutCubic);
}

void EntityWidget::summon()
{
    stopMotionAnimation();
    stopAutonomyTimers();
    const EntityPositions positions = currentPositions();

    if (!m_summonSequence || !m_summonPeekAnimation || !m_summonPause || !m_summonEnterAnimation) {
        peek();
        return;
    }

    if (m_visualState != VisualState::Commentary) {
        setVisualState(VisualState::Peeking);
    }
    move(positions.hidden);
    show();
    raise();

    m_activeMotion = MotionKind::Summon;
    m_summonPeekAnimation->setStartValue(positions.hidden);
    m_summonPeekAnimation->setEndValue(positions.peeking);
    m_summonPeekAnimation->setDuration(kSummonPeekDurationMs);
    m_summonPause->setDuration(kSummonPauseMs);
    m_summonEnterAnimation->setStartValue(positions.peeking);
    m_summonEnterAnimation->setEndValue(positions.engaged);
    m_summonEnterAnimation->setDuration(kSummonEnterDurationMs);
    m_summonSequence->start();
}

void EntityWidget::flee()
{
    if (!isVisible()) {
        Q_EMIT fleeCompleted();
        return;
    }

    stopMotionAnimation();
    stopAutonomyTimers();
    setVisualState(VisualState::Fleeing);
    animateBetween(pos(), currentPositions().hidden, kFleeDurationMs, MotionKind::Flee, QEasingCurve::InCubic);
}

void EntityWidget::hideNow()
{
    stopMotionAnimation();
    stopAutonomyTimers();
    clearScriptVisualOverride();
    setVisualState(VisualState::Hidden);
    QWidget::hide();
}

QString EntityWidget::composeTargetStateName() const
{
    if (m_movingActive) {
        return QStringLiteral("state3");
    }
    if (!m_clickOverrideStateName.isEmpty()) {
        return m_clickOverrideStateName;
    }
    if (m_hoverActive) {
        return QStringLiteral("state5");
    }
    if (!m_probeStateName.isEmpty()) {
        return m_probeStateName;
    }
    if (m_visualState == VisualState::Commentary) {
        return QStringLiteral("state2");
    }
    if (m_visualState == VisualState::Peeking) {
        return QStringLiteral("state5");
    }
    if (m_visualState == VisualState::Fleeing) {
        return QStringLiteral("state4");
    }
    return m_baseStateName.isEmpty() ? visualDefaultState(m_visualState) : m_baseStateName;
}

void EntityWidget::applyVisualState()
{
    QString accent;
    QString detailColor = QStringLiteral("rgba(230, 240, 255, 220)");
    switch (m_visualState) {
    case VisualState::Hidden:
        accent = QStringLiteral("rgba(80, 80, 90, 120)");
        detailColor = QStringLiteral("rgba(180, 180, 190, 180)");
        break;
    case VisualState::Idle:
        accent = QStringLiteral("rgba(120, 180, 255, 120)");
        break;
    case VisualState::Peeking:
        accent = QStringLiteral("rgba(255, 210, 120, 150)");
        break;
    case VisualState::Engaged:
        accent = QStringLiteral("rgba(120, 255, 190, 150)");
        break;
    case VisualState::Fleeing:
        accent = QStringLiteral("rgba(255, 120, 120, 160)");
        break;
    case VisualState::Commentary:
        accent = QStringLiteral("rgba(200, 150, 255, 170)");
        break;
    }

    const int titleFontPx = qBound(10, m_fontSizePx + 10, 36);
    const int detailFontPx = qBound(6, m_fontSizePx + 4, 28);

    if (m_titleLabel) {
        m_titleLabel->setStyleSheet(QStringLiteral(
            "QLabel {"
            "  color: white;"
            "  background: rgba(20, 24, 32, 210);"
            "  border: 1px solid %1;"
            "  border-top-left-radius: 14px;"
            "  border-top-right-radius: 14px;"
            "  border-bottom-left-radius: 0px;"
            "  border-bottom-right-radius: 0px;"
            "  padding: 18px 28px 6px 28px;"
            "  font-size: %2px;"
            "  font-weight: 600;"
            "}").arg(accent).arg(titleFontPx));
    }

    if (m_detailLabel) {
        m_detailLabel->setStyleSheet(QStringLiteral(
            "QLabel {"
            "  color: %1;"
            "  background: rgba(20, 24, 32, 210);"
            "  border: 1px solid %2;"
            "  border-top: none;"
            "  border-top-left-radius: 0px;"
            "  border-top-right-radius: 0px;"
            "  border-bottom-left-radius: 14px;"
            "  border-bottom-right-radius: 14px;"
            "  padding: 2px 28px 18px 28px;"
            "  font-size: %3px;"
            "}").arg(detailColor, accent).arg(detailFontPx));
    }
}

void EntityWidget::animateBetween(
    const QPoint &start,
    const QPoint &end,
    int durationMs,
    MotionKind motionKind,
    QEasingCurve::Type curve)
{
    stopMotionAnimation();
    m_activeMotion = motionKind;
    show();
    raise();
    m_motionAnimation->setEasingCurve(curve);
    m_motionAnimation->setDuration(durationMs);
    m_motionAnimation->setStartValue(start);
    m_motionAnimation->setEndValue(end);
    if (pos() != start) {
        move(start);
    }
    m_motionAnimation->start();
}

QPoint EntityWidget::clampPointToCurrentScreen(
    const QPoint &point,
    double visibleXRatio,
    double visibleYRatio) const
{
    const QRect available = currentAvailableGeometry();
    if (!available.isValid()) {
        return point;
    }

    return EntityPositions::clampPointToScreen(available, size(), point, visibleXRatio, visibleYRatio);
}

EntityPositions EntityWidget::currentPositions() const
{
    const QRect available = currentAvailableGeometry();
    if (!available.isValid()) {
        return EntityPositions{QPoint(), QPoint(), QPoint()};
    }

    return EntityPositions::calculate(available, size(), targetY(), kDefaultMargin, m_edge);
}

QRect EntityWidget::currentAvailableGeometry() const
{
    const QScreen *targetScreen = screen() ? screen() : QGuiApplication::primaryScreen();
    return targetScreen ? targetScreen->availableGeometry() : QRect();
}

int EntityWidget::dragThreshold() const
{
    return qMax(1, QApplication::startDragDistance());
}

void EntityWidget::ensureSummonSequence()
{
    if (m_summonSequence) {
        return;
    }

    m_summonSequence = new QSequentialAnimationGroup(this);
    m_summonPeekAnimation = new QPropertyAnimation(this, "pos", m_summonSequence);
    m_summonPause = new QPauseAnimation(kSummonPauseMs, m_summonSequence);
    m_summonEnterAnimation = new QPropertyAnimation(this, "pos", m_summonSequence);

    m_summonPeekAnimation->setEasingCurve(QEasingCurve::OutCubic);
    m_summonEnterAnimation->setEasingCurve(QEasingCurve::OutCubic);
    m_summonSequence->addAnimation(m_summonPeekAnimation);
    m_summonSequence->addAnimation(m_summonPause);
    m_summonSequence->addAnimation(m_summonEnterAnimation);
    connect(m_summonSequence, &QSequentialAnimationGroup::finished, this, &EntityWidget::onSummonSequenceFinished);
}

void EntityWidget::ensureRoamAnimation()
{
    if (m_roamAnimation) {
        return;
    }

    m_roamAnimation = new QPropertyAnimation(this, "pos", this);
    m_roamAnimation->setEasingCurve(QEasingCurve::InOutCubic);
    connect(m_roamAnimation, &QPropertyAnimation::finished, this, &EntityWidget::onRoamFinished);
}

void EntityWidget::ensureProbeSequence()
{
    if (m_probeSequence) {
        return;
    }

    m_probeSequence = new QSequentialAnimationGroup(this);
    m_probeOutAnimation = new QPropertyAnimation(this, "pos", m_probeSequence);
    m_probePause = new QPauseAnimation(kSummonPauseMs, m_probeSequence);
    m_probeBackAnimation = new QPropertyAnimation(this, "pos", m_probeSequence);

    m_probeOutAnimation->setEasingCurve(QEasingCurve::OutCubic);
    m_probeBackAnimation->setEasingCurve(QEasingCurve::InOutCubic);
    m_probeSequence->addAnimation(m_probeOutAnimation);
    m_probeSequence->addAnimation(m_probePause);
    m_probeSequence->addAnimation(m_probeBackAnimation);
    connect(m_probeSequence, &QSequentialAnimationGroup::finished, this, &EntityWidget::onProbeFinished);
}

void EntityWidget::onDirectMotionFinished()
{
    const MotionKind finishedMotion = m_activeMotion;
    m_activeMotion = MotionKind::None;

    switch (finishedMotion) {
    case MotionKind::Peek:
        Q_EMIT peekCompleted();
        scheduleAutonomyTimers();
        break;
    case MotionKind::Enter:
        Q_EMIT enterCompleted();
        scheduleAutonomyTimers();
        break;
    case MotionKind::Flee:
        setVisualState(VisualState::Hidden);
        QWidget::hide();
        Q_EMIT fleeCompleted();
        break;
    case MotionKind::None:
    case MotionKind::Summon:
    case MotionKind::Roam:
    case MotionKind::Probe:
        break;
    }
}

void EntityWidget::onSummonSequenceFinished()
{
    if (m_activeMotion != MotionKind::Summon) {
        return;
    }

    m_activeMotion = MotionKind::None;
    if (m_visualState != VisualState::Commentary) {
        setVisualState(VisualState::Engaged);
    } else {
        refreshSprite();
    }
    Q_EMIT enterCompleted();
    scheduleAutonomyTimers();
}

void EntityWidget::onRoamTimeout()
{
    if (!canStartAutonomousAction()) {
        scheduleAutonomyTimers();
        return;
    }

    const QRect available = currentAvailableGeometry();
    if (!available.isValid()) {
        return;
    }

    ensureRoamAnimation();
    if (!m_roamAnimation) {
        return;
    }

    const QPoint start = clampPointToCurrentScreen(pos());
    const int maxX = available.left() + available.width() - width();
    const int maxY = available.top() + available.height() - height();
    if (maxX < available.left() || maxY < available.top()) {
        return;
    }

    QPoint target(
        QRandomGenerator::global()->bounded(available.left(), maxX + 1),
        QRandomGenerator::global()->bounded(available.top(), maxY + 1));
    if ((target - start).manhattanLength() < 40) {
        target = QPoint(
            QRandomGenerator::global()->bounded(available.left(), maxX + 1),
            QRandomGenerator::global()->bounded(available.top(), maxY + 1));
    }

    stopMotionAnimation();
    m_movingActive = true;
    refreshSprite();
    m_activeMotion = MotionKind::Roam;
    m_roamAnimation->setDuration(QRandomGenerator::global()->bounded(kRoamMinMs, kRoamMaxMs + 1));
    m_roamAnimation->setStartValue(start);
    m_roamAnimation->setEndValue(target);
    if (pos() != start) {
        move(start);
    }
    m_roamAnimation->start();
}

void EntityWidget::onRoamFinished()
{
    if (m_activeMotion != MotionKind::Roam) {
        return;
    }

    m_activeMotion = MotionKind::None;
    m_movingActive = false;
    move(clampPointToCurrentScreen(pos()));
    refreshSprite();
    scheduleAutonomyTimers();
}

void EntityWidget::onProbeTimeout()
{
    if (!canStartAutonomousAction()) {
        scheduleAutonomyTimers();
        return;
    }

    const QRect available = currentAvailableGeometry();
    if (!available.isValid() || !m_probeSequence || !m_probeOutAnimation || !m_probePause || !m_probeBackAnimation) {
        return;
    }

    const QPoint origin = clampPointToCurrentScreen(pos());
    const bool probeLeft = QRandomGenerator::global()->bounded(2) == 0;
    QPoint target = origin;
    if (probeLeft) {
        target.setX(available.left() - width() / 2);
    } else {
        target.setX(available.left() + available.width() - width() / 2);
    }
    target.setY(QRandomGenerator::global()->bounded(
        available.top(),
        qMax(available.top(), available.top() + available.height() - height()) + 1));
    target = clampPointToCurrentScreen(target, 0.5, 1.0);

    stopMotionAnimation();
    m_probeStateName = QStringLiteral("state1");
    refreshSprite();
    m_activeMotion = MotionKind::Probe;
    m_probeOutAnimation->setDuration(QRandomGenerator::global()->bounded(kProbeOutMinMs, kProbeOutMaxMs + 1));
    m_probeOutAnimation->setStartValue(origin);
    m_probeOutAnimation->setEndValue(target);
    m_probePause->setDuration(QRandomGenerator::global()->bounded(kProbePauseMinMs, kProbePauseMaxMs + 1));
    m_probeBackAnimation->setDuration(QRandomGenerator::global()->bounded(kProbeBackMinMs, kProbeBackMaxMs + 1));
    m_probeBackAnimation->setStartValue(target);
    m_probeBackAnimation->setEndValue(origin);
    if (pos() != origin) {
        move(origin);
    }
    m_probeSequence->start();
}

void EntityWidget::onProbeFinished()
{
    if (m_activeMotion != MotionKind::Probe) {
        return;
    }

    m_activeMotion = MotionKind::None;
    m_probeStateName.clear();
    move(clampPointToCurrentScreen(pos()));
    refreshSprite();
    scheduleAutonomyTimers();
}

void EntityWidget::onClickRestoreTimeout()
{
    m_clickOverrideStateName.clear();
    refreshSprite();
}

void EntityWidget::scheduleAutonomyTimers()
{
    if (!m_autonomousEnabled || !isVisible()) {
        return;
    }
    if (!m_roamTimer->isActive()) {
        m_roamTimer->start(QRandomGenerator::global()->bounded(kRoamIntervalMinMs, kRoamIntervalMaxMs + 1));
    }
    if (!m_probeTimer->isActive()) {
        m_probeTimer->start(QRandomGenerator::global()->bounded(kProbeIntervalMinMs, kProbeIntervalMaxMs + 1));
    }
}

void EntityWidget::stopAutonomyTimers()
{
    if (m_roamTimer->isActive()) {
        m_roamTimer->stop();
    }
    if (m_probeTimer->isActive()) {
        m_probeTimer->stop();
    }
}

void EntityWidget::stopRoamAnimation()
{
    if (m_roamAnimation && m_roamAnimation->state() == QAbstractAnimation::Running) {
        m_roamAnimation->stop();
    }
    m_movingActive = false;
}

void EntityWidget::stopProbeAnimation()
{
    if (m_probeSequence && m_probeSequence->state() == QAbstractAnimation::Running) {
        m_probeSequence->stop();
    }
    m_probeStateName.clear();
}

bool EntityWidget::canStartAutonomousAction() const
{
    if (!m_autonomousEnabled || !isVisible() || m_dragging) {
        return false;
    }
    if (m_activeMotion != MotionKind::None) {
        return false;
    }
    if (m_roamAnimation && m_roamAnimation->state() == QAbstractAnimation::Running) {
        return false;
    }
    if (m_probeSequence && m_probeSequence->state() == QAbstractAnimation::Running) {
        return false;
    }
    return true;
}

void EntityWidget::refreshSprite()
{
    if (!m_spriteLabel || !m_assetCatalog) {
        return;
    }

    const QString targetStateName = composeTargetStateName();
    const bool canUseScriptSprite = !m_scriptSpritePath.isEmpty()
        && !m_movingActive
        && m_clickOverrideStateName.isEmpty()
        && !m_hoverActive
        && m_probeStateName.isEmpty();
    const QString gifPath = canUseScriptSprite
        ? m_scriptSpritePath
        : m_assetCatalog->gifForStateName(targetStateName);
    if (gifPath.isEmpty()) {
        if (m_movie) {
            m_movie->stop();
            m_spriteLabel->setMovie(nullptr);
        }
        m_renderedStateName.clear();
        m_spriteLabel->setText(QStringLiteral("No GIF"));
        return;
    }

    if (canUseScriptSprite) {
        m_renderedStateName = QFileInfo(gifPath).fileName();
    } else {
        m_renderedStateName = m_assetCatalog->normalizeStateName(targetStateName);
    }

    const QString suffix = QFileInfo(gifPath).suffix().trimmed().toLower();
    const bool animated = suffix == QStringLiteral("gif");
    if (!animated) {
        if (m_movie) {
            m_movie->stop();
            m_spriteLabel->setMovie(nullptr);
            delete m_movie;
            m_movie = nullptr;
        }
        const QPixmap pixmap(gifPath);
        if (pixmap.isNull()) {
            m_renderedStateName.clear();
            m_spriteLabel->setText(QStringLiteral("No Sprite"));
            return;
        }
        m_spriteLabel->setText(QString());
        m_spriteLabel->setPixmap(pixmap.scaled(220, 140, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        return;
    }

    m_spriteLabel->setPixmap(QPixmap());
    if (!m_movie || m_movie->fileName() != gifPath) {
        delete m_movie;
        m_movie = new QMovie(gifPath, QByteArray(), this);
        m_movie->setCacheMode(QMovie::CacheAll);
        m_movie->setScaledSize(QSize(220, 140));
        m_spriteLabel->setMovie(m_movie);
    }

    m_spriteLabel->setText(QString());
    m_movie->setSpeed(movieSpeedPercentForAnimSpeed(canUseScriptSprite ? m_scriptAnimSpeed : QString()));
    if (m_movie->state() != QMovie::Running) {
        m_movie->start();
    }
}

int EntityWidget::movieSpeedPercentForAnimSpeed(const QString &animSpeed) const
{
    const QString normalized = animSpeed.trimmed().toLower();
    if (normalized == QStringLiteral("slow")) {
        return 75;
    }
    if (normalized == QStringLiteral("fast")) {
        return 135;
    }
    if (normalized == QStringLiteral("very_fast")) {
        return 165;
    }
    return 100;
}

QString EntityWidget::gifPathForState(VisualState state) const
{
    return m_assetCatalog ? m_assetCatalog->gifForStateName(visualDefaultState(state)) : QString();
}

int EntityWidget::targetY() const
{
    const QRect available = currentAvailableGeometry();
    if (!available.isValid()) {
        return y();
    }

    const int fallbackY = available.bottom() - height() - kDefaultMargin;
    const int currentY = (pos().y() == 0) ? fallbackY : y();
    return qBound(available.top(), currentY, available.bottom() - height() + 1);
}

void EntityWidget::stopMotionAnimation()
{
    if (m_motionAnimation && m_motionAnimation->state() == QAbstractAnimation::Running) {
        m_motionAnimation->stop();
    }
    if (m_summonSequence && m_summonSequence->state() == QAbstractAnimation::Running) {
        m_summonSequence->stop();
    }
    stopRoamAnimation();
    stopProbeAnimation();
    m_activeMotion = MotionKind::None;
}

void EntityWidget::enterEvent(QEnterEvent *event)
{
    m_hoverActive = true;
    refreshSprite();
    QWidget::enterEvent(event);
}

void EntityWidget::leaveEvent(QEvent *event)
{
    m_hoverActive = false;
    refreshSprite();
    QWidget::leaveEvent(event);
}

void EntityWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        stopMotionAnimation();
        stopAutonomyTimers();
        m_dragging = true;
        m_dragStarted = false;
        m_dragOffset = event->globalPosition().toPoint() - frameGeometry().topLeft();
        m_leftPressGlobal = event->globalPosition().toPoint();
        event->accept();
        return;
    }
    if (event->button() == Qt::RightButton) {
        Q_EMIT contextMenuRequested(event->globalPosition().toPoint());
        event->accept();
        return;
    }

    QWidget::mousePressEvent(event);
}

void EntityWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (m_dragging && (event->buttons() & Qt::LeftButton)) {
        const QPoint globalPoint = event->globalPosition().toPoint();
        if (!m_dragStarted && (globalPoint - m_leftPressGlobal).manhattanLength() >= dragThreshold()) {
            m_dragStarted = true;
        }
        if (m_dragStarted) {
            move(clampPointToCurrentScreen(globalPoint - m_dragOffset));
        }
        event->accept();
        return;
    }

    QWidget::mouseMoveEvent(event);
}

void EntityWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        const bool wasDrag = m_dragStarted;
        m_dragging = false;
        m_dragStarted = false;
        if (!wasDrag) {
            const bool excited = QRandomGenerator::global()->bounded(2) == 0;
            setStateByName(excited ? QStringLiteral("state2") : QStringLiteral("state6"), false);
        } else {
            scheduleAutonomyTimers();
        }
        event->accept();
        return;
    }

    QWidget::mouseReleaseEvent(event);
}

void EntityWidget::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        Q_EMIT doubleClicked();
        event->accept();
        return;
    }

    QWidget::mouseDoubleClickEvent(event);
}

void EntityWidget::showEvent(QShowEvent *event)
{
    refreshSprite();
    scheduleAutonomyTimers();
    QWidget::showEvent(event);
}

void EntityWidget::hideEvent(QHideEvent *event)
{
    stopAutonomyTimers();
    stopRoamAnimation();
    stopProbeAnimation();
    if (m_clickRestoreTimer->isActive()) {
        m_clickRestoreTimer->stop();
    }
    m_hoverActive = false;
    m_clickOverrideStateName.clear();
    m_probeStateName.clear();
    m_movingActive = false;
    QWidget::hideEvent(event);
}

