#include "ThemeProvider.h"

#include "AppController.h"

ThemeProvider::ThemeProvider(AppController *app, QObject *parent)
    : QObject(parent)
    , m_dark(app->theme() == QStringLiteral("dark"))
{
    connect(app, &AppController::themeChanged, this, [this, app]() {
        const bool dark = app->theme() == QStringLiteral("dark");
        if (dark == m_dark)
            return;
        m_dark = dark;
        emit tokensChanged();
    });
}

QColor ThemeProvider::divider() const
{
    QColor c = text();
    c.setAlphaF(m_dark ? 0.22f : 0.16f);
    return c;
}

QVariantMap ThemeProvider::severityColors(const QString &sev) const
{
    if (sev == QStringLiteral("critical"))
        return { { "bg", accent900() }, { "fg", bg() }, { "border", accent900() } };
    if (sev == QStringLiteral("high"))
        return { { "bg", accent500() }, { "fg", bg() }, { "border", accent600() } };
    if (sev == QStringLiteral("medium"))
        return { { "bg", accent200() }, { "fg", accent800() }, { "border", accent300() } };
    if (sev == QStringLiteral("low"))
        return { { "bg", QColor(Qt::transparent) }, { "fg", accent700() }, { "border", accent400() } };
    return { { "bg", QColor(Qt::transparent) }, { "fg", neutral700() }, { "border", neutral400() } };
}

QVariantMap ThemeProvider::nodeStateColors(const QString &state) const
{
    // Board link state as reported by BoardConnection: connected | connecting
    // | offline. Offline is the one an operator must not miss, so it gets the
    // heaviest chip; connected stays quiet outline.
    if (state == QStringLiteral("connected")) {
        return { { "dot", accent() },
                 { "bg", QColor(Qt::transparent) },
                 { "fg", accent700() },
                 { "border", accent400() } };
    }
    if (state == QStringLiteral("connecting")) {
        return { { "dot", accent400() },
                 { "bg", accent200() },
                 { "fg", accent800() },
                 { "border", accent300() } };
    }
    return { { "dot", neutral500() },
             { "bg", accent900() },
             { "fg", bg() },
             { "border", accent900() } };
}
