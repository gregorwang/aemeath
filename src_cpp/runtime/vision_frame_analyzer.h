#pragma once

#include <QByteArray>
#include <QImage>

#include "services/service_contracts.h"

struct VisionFrameAnalysis
{
    GazeSample sample;
    QByteArray lumaSamples;
};

class VisionFrameAnalyzer
{
public:
    static VisionFrameAnalysis analyze(const QImage &image, const QByteArray &previousLuma);
};
