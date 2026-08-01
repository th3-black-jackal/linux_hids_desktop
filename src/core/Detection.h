#pragma once

#include <QObject>
#include <QVariantList>

#include "DetectionEvent.h"

// A non-normal scored window promoted into the triage queue.
//
// Everything here comes from the wire event; nothing is synthesized except
// `uid` (a console-local reference, since the service does not assign event
// IDs) and `severity` (a console-side mapping of className + confidence, see
// SeverityPolicy).
//
// Fields the design mocked up that the protocol does NOT carry, and which are
// therefore absent: rule ID, process name/path, PID/UID, the syscall window
// trace, and Wazuh delivery status. The board's Wazuh logger writes to a local
// file for an agent to tail; the console has no visibility into that at all.
class Detection : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString uid READ uid CONSTANT)
    Q_PROPERTY(QString time READ time CONSTANT)
    Q_PROPERTY(QString timestampRaw READ timestampRaw CONSTANT)
    Q_PROPERTY(QString severity READ severity CONSTANT)
    Q_PROPERTY(QString node READ node CONSTANT)
    Q_PROPERTY(QString endpoint READ endpoint CONSTANT)
    Q_PROPERTY(QString className READ className CONSTANT)
    Q_PROPERTY(int classIdx READ classIdx CONSTANT)
    Q_PROPERTY(double confidence READ confidence CONSTANT)
    Q_PROPERTY(QString confidenceText READ confidenceText CONSTANT)
    Q_PROPERTY(QVariantList features READ features CONSTANT)
    Q_PROPERTY(int featureCount READ featureCount CONSTANT)
    Q_PROPERTY(bool acknowledged READ acknowledged WRITE setAcknowledged NOTIFY acknowledgedChanged)
    Q_PROPERTY(QString disposition READ disposition WRITE setDisposition NOTIFY dispositionChanged)

public:
    Detection(const DetectionEvent &ev, QString uid, QString severity,
              QStringList featureNames, QObject *parent = nullptr);

    QString uid() const { return m_uid; }
    QString time() const;
    QString timestampRaw() const { return m_ev.timestampRaw; }
    QString severity() const { return m_severity; }
    QString node() const { return m_ev.host.isEmpty() ? m_ev.endpoint : m_ev.host; }
    QString endpoint() const { return m_ev.endpoint; }
    QString className() const { return m_ev.className; }
    int classIdx() const { return m_ev.classIdx; }
    double confidence() const { return m_ev.confidence; }
    QString confidenceText() const;
    int featureCount() const { return m_ev.features.size(); }

    // Top features by |normalized value|, as {name, value, valueText, pct}.
    // `pct` is |value| scaled against the largest |value| in this event, so the
    // bars are a within-event ranking — the model does not report attributions,
    // and this must not be read as one.
    QVariantList features() const { return m_features; }

    const DetectionEvent &event() const { return m_ev; }

    bool acknowledged() const { return m_acked; }
    QString disposition() const { return m_disposition; }
    void setAcknowledged(bool v);
    void setDisposition(const QString &v);

signals:
    void acknowledgedChanged();
    void dispositionChanged();

private:
    DetectionEvent m_ev;
    QString m_uid;
    QString m_severity;
    QVariantList m_features;
    bool m_acked = false;
    QString m_disposition; // "" | benign | escalated
};
