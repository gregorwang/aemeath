#pragma once

#include <QString>
#include <QtGlobal>

class MoodSystem
{
public:
    explicit MoodSystem(double initialMood = 0.5)
        : m_mood(qBound(0.0, initialMood, 1.0))
    {
    }

    double mood() const
    {
        return m_mood;
    }

    QString moodLabel() const
    {
        if (m_mood < 0.2) {
            return QStringLiteral("愤怒");
        }
        if (m_mood < 0.4) {
            return QStringLiteral("不满");
        }
        if (m_mood < 0.6) {
            return QStringLiteral("平静");
        }
        if (m_mood < 0.8) {
            return QStringLiteral("开心");
        }
        return QStringLiteral("兴奋");
    }

    void onDismissed()
    {
        m_mood = qMax(0.0, m_mood - 0.05);
    }

    void onInteracted()
    {
        m_mood = qMin(1.0, m_mood + 0.1);
    }

    void onEngaged()
    {
        m_mood = qMin(1.0, m_mood + 0.15);
    }

    void applyDelta(double delta)
    {
        m_mood = qBound(0.0, m_mood + delta, 1.0);
    }

    void naturalDecay()
    {
        if (m_mood > 0.5) {
            m_mood = qMax(0.5, m_mood - 0.02);
        } else if (m_mood < 0.5) {
            m_mood = qMin(0.5, m_mood + 0.02);
        }
    }

private:
    double m_mood = 0.5;
};
