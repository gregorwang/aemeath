#include <QDir>
#include <QFile>
#include <QImage>
#include <QLabel>
#include <QTemporaryDir>
#include <QSignalSpy>
#include <QtTest>

#include "ui/entity_widget.h"

class EntityWidgetTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void setStateByNameUsesCanonicalRendering();
    void transientClickOverrideChangesRenderedState();
    void scriptVisualOverrideUsesCustomGifPath();
    void clearScriptVisualOverrideRestoresStateRendering();
    void characterAssetRootCanUseStaticPngSprite();
    void appearanceConfigAdjustsWidgetWidthAndTypography();
    void contextMenuSignalIsEmittedOnRightClick();
    void doubleClickSignalIsEmitted();
};

void EntityWidgetTest::setStateByNameUsesCanonicalRendering()
{
    EntityWidget widget;

    QVERIFY(widget.setStateByName(QStringLiteral("roaming")));
    QCOMPARE(widget.renderedStateName(), QStringLiteral("state3"));

    QVERIFY(widget.setStateByName(QStringLiteral("engaged")));
    QCOMPARE(widget.renderedStateName(), QStringLiteral("state6"));
}

void EntityWidgetTest::transientClickOverrideChangesRenderedState()
{
    EntityWidget widget;

    QVERIFY(widget.setStateByName(QStringLiteral("idle")));
    QCOMPARE(widget.renderedStateName(), QStringLiteral("state1"));

    QVERIFY(widget.setStateByName(QStringLiteral("greeting"), false));
    QCOMPARE(widget.renderedStateName(), QStringLiteral("state6"));
}

void EntityWidgetTest::scriptVisualOverrideUsesCustomGifPath()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    QFile spriteFile(tempDir.filePath(QStringLiteral("override.gif")));
    QVERIFY(spriteFile.open(QIODevice::WriteOnly));
    spriteFile.write("gif");
    spriteFile.close();

    EntityWidget widget;
    QVERIFY(widget.setStateByName(QStringLiteral("idle")));

    widget.setScriptVisualOverride(spriteFile.fileName(), QStringLiteral("slow"));

    QCOMPARE(widget.renderedStateName(), QStringLiteral("override.gif"));
}

void EntityWidgetTest::clearScriptVisualOverrideRestoresStateRendering()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    QFile spriteFile(tempDir.filePath(QStringLiteral("override.gif")));
    QVERIFY(spriteFile.open(QIODevice::WriteOnly));
    spriteFile.write("gif");
    spriteFile.close();

    EntityWidget widget;
    QVERIFY(widget.setStateByName(QStringLiteral("engaged")));
    widget.setScriptVisualOverride(spriteFile.fileName(), QStringLiteral("fast"));
    QCOMPARE(widget.renderedStateName(), QStringLiteral("override.gif"));

    widget.clearScriptVisualOverride();

    QCOMPARE(widget.renderedStateName(), QStringLiteral("state6"));
}

void EntityWidgetTest::characterAssetRootCanUseStaticPngSprite()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    QDir root(tempDir.path());
    QVERIFY(root.mkpath(QStringLiteral("assets/sprites")));

    QImage image(32, 32, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::red);
    const QString idlePath = root.filePath(QStringLiteral("assets/sprites/idle.png"));
    QVERIFY(image.save(idlePath));

    EntityWidget widget;
    widget.setCharacterAssetRoot(root.absolutePath(), QString());
    QVERIFY(widget.setStateByName(QStringLiteral("idle")));

    bool foundPixmap = false;
    const QList<QLabel *> labels = widget.findChildren<QLabel *>();
    for (QLabel *label : labels) {
        const QPixmap pixmap = label ? label->pixmap() : QPixmap();
        if (!pixmap.isNull()) {
            foundPixmap = true;
            break;
        }
    }

    QVERIFY(foundPixmap);
    QCOMPARE(widget.renderedStateName(), QStringLiteral("state1"));
}

void EntityWidgetTest::appearanceConfigAdjustsWidgetWidthAndTypography()
{
    EntityWidget widget;
    widget.applyAppearanceConfig(80, 12);

    auto *titleLabel = widget.findChild<QLabel *>(QStringLiteral("titleLabel"));
    auto *detailLabel = widget.findChild<QLabel *>(QStringLiteral("detailLabel"));
    QVERIFY(titleLabel != nullptr);
    QVERIFY(detailLabel != nullptr);

    QVERIFY(widget.width() >= 400);
    QVERIFY(titleLabel->styleSheet().contains(QStringLiteral("font-size: 22px")));
    QVERIFY(detailLabel->styleSheet().contains(QStringLiteral("font-size: 16px")));
}

void EntityWidgetTest::contextMenuSignalIsEmittedOnRightClick()
{
    EntityWidget widget;
    widget.show();
    QSignalSpy spy(&widget, &EntityWidget::contextMenuRequested);

    QTest::mouseClick(&widget, Qt::RightButton, Qt::NoModifier, QPoint(20, 20));

    QCOMPARE(spy.count(), 1);
}

void EntityWidgetTest::doubleClickSignalIsEmitted()
{
    EntityWidget widget;
    widget.show();
    QSignalSpy spy(&widget, &EntityWidget::doubleClicked);

    QTest::mouseDClick(&widget, Qt::LeftButton, Qt::NoModifier, QPoint(20, 20));

    QCOMPARE(spy.count(), 1);
}

QTEST_MAIN(EntityWidgetTest)

#include "test_entity_widget.moc"
