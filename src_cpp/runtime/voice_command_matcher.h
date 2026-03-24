#pragma once

#include <QString>

struct VoiceCommandMatch
{
    QString action;
    int score = -1;
    QString phrase;
    QString transcript;
};

class VoiceCommandMatcher
{
public:
    static QString normalizeText(const QString &text);
    static VoiceCommandMatch match(const QString &transcript, int minScore = 68);
    static bool containsScreenIntent(const QString &transcript);

private:
    static int similarityScore(const QString &left, const QString &right);
};
