#include "runtime/idle_invasion_controller.h"

#include <algorithm>
#include <limits>

#include <QCursor>
#include <QFileInfo>
#include <QGuiApplication>
#include <QLabel>
#include <QMovie>
#include <QPointer>
#include <QPropertyAnimation>
#include <QRandomGenerator>
#include <QScreen>
#include <QSet>
#include <QTimer>
#include <QWidget>
#include <QtGlobal>

#include "runtime/app_paths.h"

namespace {

constexpr int kRetreatTimeoutMs = 5000;
constexpr int kDebugForceTimeoutMs = 120000;

class IdleInvaderWidget final : public QWidget
{
    Q_OBJECT

public:
    IdleInvaderWidget(
        int particleId,
        const QString &gifPath,
        const QSize &scaledSize,
        const QPoint &targetPos,
        QWidget *parent = nullptr)
        : QWidget(parent)
        , m_particleId(particleId)
        , m_targetPos(targetPos)
        , m_movie(new QMovie(gifPath))
        , m_label(new QLabel(this))
    {
        setWindowFlags(
            Qt::FramelessWindowHint
            | Qt::WindowStaysOnTopHint
            | Qt::Tool
            | Qt::WindowDoesNotAcceptFocus);
        setAttribute(Qt::WA_TranslucentBackground, true);
        setAttribute(Qt::WA_ShowWithoutActivating, true);
        setAttribute(Qt::WA_TransparentForMouseEvents, true);
        setWindowOpacity(0.75 + (QRandomGenerator::global()->generateDouble() * 0.25));

        m_label->setAlignment(Qt::AlignCenter);
        m_label->setStyleSheet(QStringLiteral("QLabel { background: transparent; }"));

        if (m_movie->isValid()) {
            m_movie->setCacheMode(QMovie::CacheAll);
            if (scaledSize.isValid()) {
                m_movie->setScaledSize(scaledSize);
            }
            m_label->setMovie(m_movie);
            m_movie->start();
        }

        m_label->adjustSize();
        const QSize widgetSize = m_label->size().expandedTo(QSize(1, 1));
        setFixedSize(widgetSize);
    }

    int particleId() const
    {
        return m_particleId;
    }

    void spawn(const QPoint &startPos)
    {
        move(startPos);
        show();
        raise();

        m_enterAnimation = new QPropertyAnimation(this, "pos", this);
        m_enterAnimation->setDuration(QRandomGenerator::global()->bounded(700) + 800);
        m_enterAnimation->setStartValue(startPos);
        m_enterAnimation->setEndValue(m_targetPos);
        m_enterAnimation->setEasingCurve(QEasingCurve::OutCubic);
        m_enterAnimation->start(QAbstractAnimation::DeleteWhenStopped);
    }

    void startRetreat()
    {
        if (m_retreating) {
            return;
        }
        m_retreating = true;

        const QRect geometry = QGuiApplication::primaryScreen()
            ? QGuiApplication::primaryScreen()->availableGeometry()
            : QRect(0, 0, 1920, 1080);

        const QPoint current = pos();
        const QHash<QString, int> distances = {
            { QStringLiteral("left"), current.x() - geometry.x() },
            { QStringLiteral("right"), (geometry.x() + geometry.width()) - (current.x() + width()) },
            { QStringLiteral("top"), current.y() - geometry.y() },
            { QStringLiteral("bottom"), (geometry.y() + geometry.height()) - (current.y() + height()) },
        };

        QString nearestEdge = QStringLiteral("left");
        int nearestDistance = std::numeric_limits<int>::max();
        for (auto it = distances.cbegin(); it != distances.cend(); ++it) {
            if (it.value() < nearestDistance) {
                nearestDistance = it.value();
                nearestEdge = it.key();
            }
        }

        QPoint exitPos = current;
        if (nearestEdge == QStringLiteral("left")) {
            exitPos.setX(geometry.x() - width() - 20);
        } else if (nearestEdge == QStringLiteral("right")) {
            exitPos.setX(geometry.x() + geometry.width() + 20);
        } else if (nearestEdge == QStringLiteral("top")) {
            exitPos.setY(geometry.y() - height() - 20);
        } else {
            exitPos.setY(geometry.y() + geometry.height() + 20);
        }

        auto *exitAnimation = new QPropertyAnimation(this, "pos", this);
        exitAnimation->setDuration(QRandomGenerator::global()->bounded(500) + 600);
        exitAnimation->setStartValue(current);
        exitAnimation->setEndValue(exitPos);
        exitAnimation->setEasingCurve(QEasingCurve::InCubic);
        connect(exitAnimation, &QPropertyAnimation::finished, this, &IdleInvaderWidget::finishNow);
        exitAnimation->start(QAbstractAnimation::DeleteWhenStopped);
    }

    void forceDismiss()
    {
        finishNow();
    }

Q_SIGNALS:
    void finished(int particleId);

private Q_SLOTS:
    void finishNow()
    {
        if (m_finished) {
            return;
        }
        m_finished = true;
        if (m_movie) {
            m_movie->stop();
        }
        hide();
        Q_EMIT finished(m_particleId);
        deleteLater();
    }

private:
    int m_particleId = 0;
    QPoint m_targetPos;
    QMovie *m_movie = nullptr;
    QLabel *m_label = nullptr;
    QPointer<QPropertyAnimation> m_enterAnimation;
    bool m_retreating = false;
    bool m_finished = false;
};

QStringList defaultGifCandidates()
{
    return {
        QStringLiteral("state1.gif"),
        QStringLiteral("state2.gif"),
        QStringLiteral("state5.gif"),
        QStringLiteral("state6.gif"),
        QStringLiteral("aemeath.gif"),
    };
}

} // namespace

IdleInvasionController::IdleInvasionController(QObject *parent)
    : QObject(parent)
    , m_spawnTimer(new QTimer(this))
    , m_retreatTimer(new QTimer(this))
    , m_debugForceTimer(new QTimer(this))
{
    m_spawnTimer->setSingleShot(true);
    m_retreatTimer->setSingleShot(true);
    m_debugForceTimer->setSingleShot(true);

    connect(m_spawnTimer, &QTimer::timeout, this, &IdleInvasionController::onSpawnTick);
    connect(m_retreatTimer, &QTimer::timeout, this, &IdleInvasionController::onRetreatTimeout);
    connect(m_debugForceTimer, &QTimer::timeout, this, &IdleInvasionController::onDebugForceTimeout);

    qRegisterMetaType<IdleInvasionController::InvasionState>("IdleInvasionController::InvasionState");
    m_gifPaths = resolveGifPaths();
    refreshGifSizes();
}

void IdleInvasionController::applyConfig(const IdleInvasionConfig &config)
{
    m_config = config;
    if (m_config.participatingGifs.isEmpty()) {
        m_config.participatingGifs = defaultGifCandidates();
    }
    m_gifPaths = resolveGifPaths();
    refreshGifSizes();
    qInfo().nospace()
        << "[IdleInvasion] Config applied: enabled=" << m_config.enabled
        << " start_delay_ms=" << m_config.startDelayMs
        << " initial_spawn_interval_ms=" << m_config.initialSpawnIntervalMs
        << " min_spawn_interval_ms=" << m_config.minSpawnIntervalMs
        << " max_invaders=" << m_config.maxInvaders
        << " retreat_style=" << m_config.retreatStyle;

    if (m_state == InvasionState::Inactive) {
        m_invasionStarted = false;
    }

    if ((!m_config.enabled || m_dndEnabled) && m_state != InvasionState::Inactive) {
        beginRetreat();
    }
}

void IdleInvasionController::setDndEnabled(bool enabled)
{
    const bool target = enabled;
    if (m_dndEnabled == target) {
        return;
    }
    m_dndEnabled = target;
    qInfo().nospace() << "[IdleInvasion] DND changed: enabled=" << m_dndEnabled;
    if (m_dndEnabled) {
        m_debugForceMode = false;
        m_idleTimeMs = 0;
        m_invasionStarted = false;
        m_spawnTimer->stop();
        if (m_state == InvasionState::Spawning || m_state == InvasionState::Saturated) {
            beginRetreat();
        }
    }
}

bool IdleInvasionController::triggerDebugInvasion()
{
    m_debugForceTimer->stop();
    m_spawnTimer->stop();
    m_retreatTimer->stop();
    dismissAllImmediate();
    setState(InvasionState::Inactive);

    m_debugForceMode = true;
    m_invasionStarted = true;
    m_idleTimeMs = qMax<qint64>(m_idleTimeMs, static_cast<qint64>(m_config.startDelayMs) + 10 * 60000);
    m_gifPaths = resolveGifPaths();
    refreshGifSizes();
    beginSpawning();

    const bool started = m_state == InvasionState::Spawning || m_state == InvasionState::Saturated;
    if (started) {
        m_debugForceTimer->start(kDebugForceTimeoutMs);
    } else {
        m_debugForceMode = false;
    }
    qInfo().nospace()
        << "[IdleInvasion] Debug trigger requested -> started=" << started
        << " state=" << static_cast<int>(m_state);
    return started;
}

void IdleInvasionController::shutdown()
{
    m_debugForceTimer->stop();
    m_spawnTimer->stop();
    m_retreatTimer->stop();
    dismissAllImmediate();
    setState(InvasionState::Inactive);
}

IdleInvasionController::InvasionState IdleInvasionController::state() const
{
    return m_state;
}

int IdleInvasionController::activeCount() const
{
    return m_particles.size();
}

void IdleInvasionController::onIdleTimeUpdated(qint64 idleMs)
{
    if (m_dndEnabled) {
        m_idleTimeMs = 0;
        m_invasionStarted = false;
        return;
    }

    if (m_debugForceMode) {
        m_idleTimeMs = qMax<qint64>(static_cast<qint64>(m_config.startDelayMs) + 10 * 60000, idleMs);
        return;
    }

    m_idleTimeMs = idleMs;
    if (!m_config.enabled || m_state == InvasionState::Retreating) {
        return;
    }

    if (m_invasionStarted && idleMs < 1000) {
        if (m_state == InvasionState::Spawning || m_state == InvasionState::Saturated) {
            beginRetreat();
        } else {
            m_invasionStarted = false;
        }
        return;
    }

    if (m_state == InvasionState::Inactive) {
        if (m_invasionStarted) {
            return;
        }
        if (idleMs >= m_config.startDelayMs) {
            m_invasionStarted = true;
            beginSpawning();
        }
    }
}

void IdleInvasionController::onUserActivityDetected()
{
    if (m_debugForceMode) {
        return;
    }

    if (m_state == InvasionState::Spawning || m_state == InvasionState::Saturated) {
        beginRetreat();
    } else if (m_state == InvasionState::Inactive) {
        m_invasionStarted = false;
        m_idleTimeMs = 0;
    }
}

void IdleInvasionController::onSpawnTick()
{
    if (m_state != InvasionState::Spawning) {
        return;
    }
    spawnOne();
    if (m_state == InvasionState::Spawning) {
        armSpawnTimer();
    }
}

void IdleInvasionController::onRetreatTimeout()
{
    if (m_state == InvasionState::Retreating) {
        dismissAllImmediate();
        reset();
    }
}

void IdleInvasionController::onDebugForceTimeout()
{
    if (!m_debugForceMode) {
        return;
    }
    m_debugForceMode = false;
    if (m_state == InvasionState::Spawning || m_state == InvasionState::Saturated) {
        beginRetreat();
    }
}

void IdleInvasionController::onParticleFinished(int particleId)
{
    QObject *particle = m_particles.take(particleId);
    if (particle != nullptr) {
        particle->disconnect(this);
    }
    const quint64 cell = m_particleCells.take(particleId);
    if (cell != 0) {
        m_occupied.remove(cell);
    }
    Q_EMIT activeCountChanged(m_particles.size());

    if (m_state == InvasionState::Retreating && m_particles.isEmpty()) {
        m_retreatTimer->stop();
        reset();
    }
}

void IdleInvasionController::setState(InvasionState state)
{
    if (m_state == state) {
        return;
    }
    m_state = state;
    Q_EMIT stateChanged(m_state);
}

void IdleInvasionController::beginSpawning()
{
    if (m_gifPaths.isEmpty()) {
        qWarning() << "[IdleInvasion] No valid GIF paths, cannot start invasion.";
        return;
    }
    initGrid();
    qInfo().nospace() << "[IdleInvasion] Invasion started - idle_ms=" << m_idleTimeMs;
    setState(InvasionState::Spawning);
    spawnOne();
    if (m_state == InvasionState::Spawning) {
        armSpawnTimer();
    }
}

void IdleInvasionController::armSpawnTimer()
{
    m_spawnTimer->start(currentSpawnIntervalMs());
}

int IdleInvasionController::currentSpawnIntervalMs() const
{
    const int extraIdleMs = qMax<qint64>(0, m_idleTimeMs - m_config.startDelayMs);
    const int initialMs = qMax(500, m_config.initialSpawnIntervalMs);
    const int minMs = qMax(500, m_config.minSpawnIntervalMs);
    if (initialMs == minMs) {
        return initialMs;
    }

    int low = initialMs;
    int high = initialMs;
    if (extraIdleMs < 3 * 60000) {
        low = static_cast<int>(initialMs * 0.8);
        high = static_cast<int>(initialMs * 1.2);
    } else if (extraIdleMs < 5 * 60000) {
        low = static_cast<int>(initialMs * 0.5);
        high = static_cast<int>(initialMs * 0.8);
    } else if (extraIdleMs < 10 * 60000) {
        low = static_cast<int>(initialMs * 0.3);
        high = static_cast<int>(initialMs * 0.5);
    } else {
        low = minMs;
        high = static_cast<int>(initialMs * 0.3);
    }

    low = qMax(minMs, low);
    high = qMax(low, high);
    return QRandomGenerator::global()->bounded(low, high + 1);
}

void IdleInvasionController::initGrid()
{
    QScreen *screen = QGuiApplication::primaryScreen();
    if (!screen) {
        m_screenGeometry = QRect(0, 0, 1920, 1080);
        qWarning() << "[IdleInvasion] No primary screen found. Falling back to 1920x1080.";
    } else {
        m_screenGeometry = screen->availableGeometry();
    }

    refreshGifSizes();
    m_cellWidth = qMax(1, m_maxGifWidth + m_config.cellPadding);
    m_cellHeight = qMax(1, m_maxGifHeight + m_config.cellPadding);
    m_gridCols = qMax(1, m_screenGeometry.width() / qMax(1, m_cellWidth));
    m_gridRows = qMax(1, m_screenGeometry.height() / qMax(1, m_cellHeight));
    m_occupied.clear();
    m_particleCells.clear();
    qInfo().nospace()
        << "[IdleInvasion] Grid initialised: "
        << m_gridCols << "x" << m_gridRows
        << " cells (" << m_cellWidth << "x" << m_cellHeight << " px each)"
        << " max=" << (m_gridCols * m_gridRows)
        << " screen=" << m_screenGeometry.width() << "x" << m_screenGeometry.height();
}

void IdleInvasionController::spawnOne()
{
    if (m_particles.size() >= m_config.maxInvaders) {
        qInfo().nospace() << "[IdleInvasion] Reached max invaders (" << m_config.maxInvaders << "), entering SATURATED.";
        setState(InvasionState::Saturated);
        m_spawnTimer->stop();
        return;
    }

    QList<QPair<int, int>> freeCells;
    freeCells.reserve(m_gridCols * m_gridRows);
    for (int col = 0; col < m_gridCols; ++col) {
        for (int row = 0; row < m_gridRows; ++row) {
            const quint64 key = cellKey(col, row);
            if (!m_occupied.contains(key)) {
                freeCells.push_back(qMakePair(col, row));
            }
        }
    }
    if (freeCells.isEmpty()) {
        qInfo() << "[IdleInvasion] No free cells remaining, entering SATURATED.";
        setState(InvasionState::Saturated);
        m_spawnTimer->stop();
        return;
    }

    const auto cell = freeCells.at(QRandomGenerator::global()->bounded(freeCells.size()));
    const QString gifPath = m_gifPaths.at(QRandomGenerator::global()->bounded(m_gifPaths.size()));
    const QSize gifSize = m_gifSizes.value(gifPath, QSize(m_maxGifWidth, m_maxGifHeight));
    const QPoint targetPos = targetPositionForCell(cell.first, cell.second, gifSize);
    const QPoint startPos = randomOffscreenStartPosition(targetPos, gifSize);

    const int particleId = m_nextParticleId++;
    auto *particle = new IdleInvaderWidget(particleId, gifPath, gifSize, targetPos);
    connect(particle, &IdleInvaderWidget::finished, this, &IdleInvasionController::onParticleFinished);
    m_particles.insert(particleId, particle);
    m_occupied.insert(cellKey(cell.first, cell.second));
    m_particleCells.insert(particleId, cellKey(cell.first, cell.second));
    Q_EMIT activeCountChanged(m_particles.size());
    qDebug().nospace()
        << "[IdleInvasion] Spawned invader pid=" << particleId
        << " cell=(" << cell.first << "," << cell.second << ")"
        << " gif=" << QFileInfo(gifPath).fileName()
        << " total=" << m_particles.size();
    particle->spawn(startPos);
}

void IdleInvasionController::beginRetreat()
{
    m_debugForceMode = false;
    m_debugForceTimer->stop();
    m_spawnTimer->stop();
    setState(InvasionState::Retreating);
    qInfo().nospace()
        << "[IdleInvasion] Retreat triggered - dismissing "
        << m_particles.size()
        << " invaders (style=" << m_config.retreatStyle << ").";

    if (m_particles.isEmpty()) {
        dismissAllImmediate();
        reset();
        return;
    }

    const QString style = m_config.retreatStyle.trimmed().toLower();
    if (style == QStringLiteral("instant")) {
        dismissAllImmediate();
        reset();
        return;
    }

    QList<QObject *> particles = m_particles.values();
    if (style == QStringLiteral("ripple")) {
        particles = sortedParticlesForRipple();
        for (int index = 0; index < particles.size(); ++index) {
            QObject *particle = particles.at(index);
            QTimer::singleShot(
                qMin(2200, index * 60 + QRandomGenerator::global()->bounded(40)),
                particle,
                [particle]() {
                    if (auto *widget = qobject_cast<IdleInvaderWidget *>(particle)) {
                        widget->startRetreat();
                    }
                });
        }
    } else {
        for (QObject *particle : particles) {
            QTimer::singleShot(
                QRandomGenerator::global()->bounded(50, 201),
                particle,
                [particle]() {
                    if (auto *widget = qobject_cast<IdleInvaderWidget *>(particle)) {
                        widget->startRetreat();
                    }
                });
        }
    }

    m_retreatTimer->start(kRetreatTimeoutMs);
}

void IdleInvasionController::dismissAllImmediate()
{
    const QList<QObject *> particles = m_particles.values();
    m_particles.clear();
    m_particleCells.clear();
    m_occupied.clear();
    for (QObject *particle : particles) {
        if (auto *widget = qobject_cast<IdleInvaderWidget *>(particle)) {
            widget->forceDismiss();
        }
    }
    Q_EMIT activeCountChanged(0);
}

void IdleInvasionController::reset()
{
    m_debugForceTimer->stop();
    setState(InvasionState::Inactive);
    m_debugForceMode = false;
    m_invasionStarted = false;
    m_idleTimeMs = 0;
    m_occupied.clear();
    m_particleCells.clear();
    qInfo() << "[IdleInvasion] Reset to INACTIVE.";
}

void IdleInvasionController::refreshGifSizes()
{
    m_gifSizes.clear();
    m_maxGifWidth = qMax(1, static_cast<int>(120 * m_config.scale));
    m_maxGifHeight = qMax(1, static_cast<int>(120 * m_config.scale));

    for (const QString &gifPath : m_gifPaths) {
        const QSize size = readScaledGifSize(gifPath);
        m_gifSizes.insert(gifPath, size);
        m_maxGifWidth = qMax(m_maxGifWidth, size.width());
        m_maxGifHeight = qMax(m_maxGifHeight, size.height());
    }
}

QStringList IdleInvasionController::resolveGifPaths() const
{
    QStringList paths;
    const QStringList candidates = m_config.participatingGifs.isEmpty()
        ? defaultGifCandidates()
        : m_config.participatingGifs;
    for (const QString &fileName : candidates) {
        QString resolvedPath;
        const QFileInfo rawInfo(fileName);
        if (rawInfo.exists()) {
            resolvedPath = rawInfo.absoluteFilePath();
        } else {
            resolvedPath = AppPaths::resolveOptionalAsset(QStringLiteral("characters/%1").arg(fileName));
        }
        if (!resolvedPath.isEmpty()) {
            paths.push_back(resolvedPath);
        } else {
            qDebug().nospace() << "[IdleInvasion] GIF not found, skipping: " << fileName;
        }
    }
    return paths;
}

QSize IdleInvasionController::readScaledGifSize(const QString &gifPath) const
{
    const int defaultSize = qMax(1, static_cast<int>(120 * m_config.scale));
    QMovie movie(gifPath);
    if (!movie.isValid()) {
        return QSize(defaultSize, defaultSize);
    }

    QSize size = movie.frameRect().size();
    if (!size.isValid() || size.width() <= 0 || size.height() <= 0) {
        movie.jumpToFrame(0);
        size = movie.currentImage().size();
    }

    const int width = qMax(1, static_cast<int>(qMax(1, size.width()) * m_config.scale));
    const int height = qMax(1, static_cast<int>(qMax(1, size.height()) * m_config.scale));
    return QSize(width, height);
}

quint64 IdleInvasionController::cellKey(int col, int row) const
{
    return (static_cast<quint64>(static_cast<quint32>(col)) << 32)
        | static_cast<quint32>(row);
}

QPoint IdleInvasionController::targetPositionForCell(int col, int row, const QSize &gifSize) const
{
    const int cellX = m_screenGeometry.x() + col * m_cellWidth;
    const int cellY = m_screenGeometry.y() + row * m_cellHeight;
    const int maxOffsetX = qMax(0, m_cellWidth - gifSize.width());
    const int maxOffsetY = qMax(0, m_cellHeight - gifSize.height());
    return QPoint(
        cellX + QRandomGenerator::global()->bounded(maxOffsetX + 1),
        cellY + QRandomGenerator::global()->bounded(maxOffsetY + 1));
}

QPoint IdleInvasionController::randomOffscreenStartPosition(const QPoint &endPos, const QSize &size) const
{
    const QStringList edges = {
        QStringLiteral("top"),
        QStringLiteral("bottom"),
        QStringLiteral("left"),
        QStringLiteral("right"),
    };
    const QString edge = edges.at(QRandomGenerator::global()->bounded(edges.size()));
    if (edge == QStringLiteral("top")) {
        return QPoint(endPos.x(), m_screenGeometry.y() - size.height() - 20);
    }
    if (edge == QStringLiteral("bottom")) {
        return QPoint(endPos.x(), m_screenGeometry.y() + m_screenGeometry.height() + 20);
    }
    if (edge == QStringLiteral("left")) {
        return QPoint(m_screenGeometry.x() - size.width() - 20, endPos.y());
    }
    return QPoint(m_screenGeometry.x() + m_screenGeometry.width() + 20, endPos.y());
}

QList<QObject *> IdleInvasionController::sortedParticlesForRipple() const
{
    QList<QObject *> particles = m_particles.values();
    const QPoint cursor = QCursor::pos();
    std::sort(particles.begin(), particles.end(), [cursor](QObject *lhs, QObject *rhs) {
        const QWidget *leftWidget = qobject_cast<QWidget *>(lhs);
        const QWidget *rightWidget = qobject_cast<QWidget *>(rhs);
        if (!leftWidget || !rightWidget) {
            return false;
        }
        const QPoint leftPos = leftWidget->pos();
        const QPoint rightPos = rightWidget->pos();
        const qint64 leftDistance =
            static_cast<qint64>(leftPos.x() - cursor.x()) * (leftPos.x() - cursor.x())
            + static_cast<qint64>(leftPos.y() - cursor.y()) * (leftPos.y() - cursor.y());
        const qint64 rightDistance =
            static_cast<qint64>(rightPos.x() - cursor.x()) * (rightPos.x() - cursor.x())
            + static_cast<qint64>(rightPos.y() - cursor.y()) * (rightPos.y() - cursor.y());
        return leftDistance < rightDistance;
    });
    return particles;
}

#include "idle_invasion_controller.moc"
