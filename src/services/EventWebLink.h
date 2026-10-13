#pragma once

#include <QObject>
#include <QString>
#include <QUrl>

// Opens one event in the external web app, in the operator's browser.
//
// The URL is a **template**, not a hard-coded path, because the web app's
// deep-link format is not something this console can know. The default puts
// the event's identifying fields in the query string:
//
//     https://orchicyb-ai-attack.lovable.app/?event={id}&host={host}&class={class}&time={time}
//
// Query parameters rather than a path segment is a deliberate choice for the
// default: a single-page app ignores query parameters it does not recognise
// and still renders, whereas an unrecognised *path* is a 404 or a blank
// catch-all route. So the worst case here is "the app opens on its front
// page" rather than "the app breaks". Change the template on the
// Integrations screen once the real format is known — that is the only place
// it appears.
//
// Placeholders, all percent-encoded on substitution:
//
//     {id}          server-side SIEM event id, empty for a live MQTT-only row
//     {uid}         console reference (IW-DB-42, or IW-<date>-<n> when live)
//     {host}        board hostname
//     {class}       model class label      {classIdx}   its index
//     {confidence}  0.000–1.000            {severity}   console-side grading
//     {time}        event time, ISO-8601 UTC as the board sent it
//
// A parameter whose placeholder resolves to nothing is dropped from the
// query, so the same template works for a database row and for a live
// detection that has no server-side id.
class EventWebLink : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString urlTemplate READ urlTemplate WRITE setUrlTemplate NOTIFY changed)
    Q_PROPERTY(bool enabled READ enabled NOTIFY changed)
    Q_PROPERTY(QString lastUrl READ lastUrl NOTIFY lastUrlChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastUrlChanged)

public:
    explicit EventWebLink(QObject *parent = nullptr);

    QString urlTemplate() const { return m_template; }
    void setUrlTemplate(const QString &v);
    bool enabled() const { return !m_template.isEmpty(); }

    QString lastUrl() const { return m_lastUrl; }
    QString lastError() const { return m_lastError; }

    // Builds the URL without opening it — used by the UI to show the operator
    // exactly where a row will send them, and by the tests.
    Q_INVOKABLE QString buildUrl(const QString &id, const QString &uid, const QString &host,
                                 const QString &className, int classIdx,
                                 const QString &confidenceText, const QString &timestampRaw,
                                 const QString &severity) const;

    Q_INVOKABLE void openEvent(const QString &id, const QString &uid, const QString &host,
                               const QString &className, int classIdx,
                               const QString &confidenceText, const QString &timestampRaw,
                               const QString &severity);

    Q_INVOKABLE void resetTemplate();

    void loadSettings();
    void saveSettings();

    static QString defaultTemplate();

signals:
    void changed();
    void lastUrlChanged();
    void logged(const QString &message);

private:
    QString m_template;
    QString m_lastUrl;
    QString m_lastError;
    bool m_loading = false;
};
