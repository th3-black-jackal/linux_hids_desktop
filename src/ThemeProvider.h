#pragma once

#include <QObject>
#include <QColor>
#include <QString>
#include <QVariantMap>

class AppController;

// Design-system tokens, ported from the Claude Design bundle's styles.css
// + the dc.html prototype's light/dark THEMES map. Registered as the QML
// singleton "Theme" (see main.cpp) — a C++ singleton rather than a
// composite `pragma Singleton` QML type, since a composite QtObject
// singleton combined with this project's qt_add_qml_module setup was
// observed at runtime to resolve to an empty placeholder object (every
// property undefined) even though the type itself loaded without error.
// A C++-backed singleton sidesteps whatever that composite-singleton
// quirk was.
//
// Important nuance carried over intentionally: only bg/surface/text/
// divider/neutral200/neutral300/neutral600/accent/accent700/accent900
// actually swap between themes. The rest of the accent/neutral ramp (used
// for severity chips and node-state chips) stays fixed at its light-mode
// value in both themes — that's what makes the severity tags read as
// "light chips" even on the dark background (see chats/chat1.md).
class ThemeProvider : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool dark READ dark NOTIFY tokensChanged)

    Q_PROPERTY(QColor bg READ bg NOTIFY tokensChanged)
    Q_PROPERTY(QColor surface READ surface NOTIFY tokensChanged)
    Q_PROPERTY(QColor text READ text NOTIFY tokensChanged)
    Q_PROPERTY(QColor divider READ divider NOTIFY tokensChanged)
    Q_PROPERTY(QColor neutral200 READ neutral200 NOTIFY tokensChanged)
    Q_PROPERTY(QColor neutral300 READ neutral300 NOTIFY tokensChanged)
    Q_PROPERTY(QColor neutral600 READ neutral600 NOTIFY tokensChanged)
    Q_PROPERTY(QColor accent READ accent NOTIFY tokensChanged)
    Q_PROPERTY(QColor accent700 READ accent700 NOTIFY tokensChanged)
    Q_PROPERTY(QColor accent900 READ accent900 NOTIFY tokensChanged)

    Q_PROPERTY(QColor textMuted READ textMuted NOTIFY tokensChanged)
    Q_PROPERTY(QColor textMutedFaint READ textMutedFaint NOTIFY tokensChanged)
    Q_PROPERTY(QColor rowHover READ rowHover NOTIFY tokensChanged)
    Q_PROPERTY(QColor rowSelected READ rowSelected NOTIFY tokensChanged)
    Q_PROPERTY(QColor cellBorder READ cellBorder NOTIFY tokensChanged)

    Q_PROPERTY(QColor neutral100 READ neutral100 CONSTANT)
    Q_PROPERTY(QColor neutral400 READ neutral400 CONSTANT)
    Q_PROPERTY(QColor neutral500 READ neutral500 CONSTANT)
    Q_PROPERTY(QColor neutral700 READ neutral700 CONSTANT)
    Q_PROPERTY(QColor neutral800 READ neutral800 CONSTANT)
    Q_PROPERTY(QColor neutral900 READ neutral900 CONSTANT)
    Q_PROPERTY(QColor accent100 READ accent100 CONSTANT)
    Q_PROPERTY(QColor accent200 READ accent200 CONSTANT)
    Q_PROPERTY(QColor accent300 READ accent300 CONSTANT)
    Q_PROPERTY(QColor accent400 READ accent400 CONSTANT)
    Q_PROPERTY(QColor accent500 READ accent500 CONSTANT)
    Q_PROPERTY(QColor accent600 READ accent600 CONSTANT)
    Q_PROPERTY(QColor accent800 READ accent800 CONSTANT)

    Q_PROPERTY(QString fontBody READ fontBody CONSTANT)
    Q_PROPERTY(QString fontHeading READ fontHeading CONSTANT)

public:
    explicit ThemeProvider(AppController *app, QObject *parent = nullptr);

    bool dark() const { return m_dark; }

    QColor bg() const { return m_dark ? QColor("#191c1f") : QColor("#f2f2f3"); }
    QColor surface() const { return m_dark ? QColor("#23272c") : QColor("#e9e9ea"); }
    QColor text() const { return m_dark ? QColor("#e9eaec") : QColor("#1d1f20"); }
    QColor divider() const;
    QColor neutral200() const { return m_dark ? QColor("#23272c") : QColor("#e7e7ea"); }
    QColor neutral300() const { return m_dark ? QColor("#383d43") : QColor("#d4d4d7"); }
    QColor neutral600() const { return m_dark ? QColor("#9aa1a8") : QColor("#7a7a7d"); }
    QColor accent() const { return m_dark ? QColor("#94bce3") : QColor("#5980a6"); }
    QColor accent700() const { return m_dark ? QColor("#b5d9fd") : QColor("#416180"); }
    QColor accent900() const { return m_dark ? QColor("#d6ebff") : QColor("#1d2d3d"); }

    QColor textMuted() const { QColor c = text(); c.setAlphaF(0.55f); return c; }
    QColor textMutedFaint() const { QColor c = text(); c.setAlphaF(0.60f); return c; }
    QColor rowHover() const { QColor c = text(); c.setAlphaF(0.06f); return c; }
    QColor rowSelected() const { QColor c = accent(); c.setAlphaF(0.14f); return c; }
    QColor cellBorder() const { QColor c = text(); c.setAlphaF(0.08f); return c; }

    QColor neutral100() const { return QColor("#f5f5f8"); }
    QColor neutral400() const { return QColor("#b7b7ba"); }
    QColor neutral500() const { return QColor("#98989b"); }
    QColor neutral700() const { return QColor("#5d5d60"); }
    QColor neutral800() const { return QColor("#424244"); }
    QColor neutral900() const { return QColor("#2b2b2d"); }
    QColor accent100() const { return QColor("#eef6ff"); }
    QColor accent200() const { return QColor("#d6ebff"); }
    QColor accent300() const { return QColor("#b5d9fd"); }
    QColor accent400() const { return QColor("#94bce3"); }
    QColor accent500() const { return QColor("#749dc4"); }
    QColor accent600() const { return QColor("#597ea3"); }
    QColor accent800() const { return QColor("#2c455d"); }

    QString fontBody() const { return QStringLiteral("Barlow"); }
    QString fontHeading() const { return QStringLiteral("Barlow Condensed"); }

    Q_INVOKABLE QVariantMap severityColors(const QString &sev) const;
    Q_INVOKABLE QVariantMap nodeStateColors(const QString &state) const;

signals:
    void tokensChanged();

private:
    bool m_dark = false;
};
