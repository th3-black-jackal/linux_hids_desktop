#pragma once

#include <QDateTime>
#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QVector>

#include <optional>

// One scored window as broadcast by the on-device service.
//
// Source of truth is hids::DetectionEvent::to_json(include_features=true),
// called from TelemetryServer::broadcast() once per inference window. The
// service's own field names (see InferenceEngine::predict) are:
//
//     timestamp   ISO-8601 string, DetectionEvent::now_iso8601()
//     classIdx    int, argmax over the model's output classes
//     className   string, CLASS_NAMES[classIdx] or "unknown"
//     confidence  float, softmax/score of the winning class
//     host        string, Config::host of the board
//     features    float[], the *normalized* feature vector fed to the model
//
// NOTE: detection_event.hpp was not available when this was written, so the
// literal JSON key spellings emitted by to_json() are unconfirmed. parse()
// accepts the C++ member names plus common snake_case/short aliases, and
// reports anything it cannot interpret rather than silently yielding an
// empty event. If to_json() uses different names, add them to the alias
// lists in DetectionEvent.cpp — that is the only place they appear.
struct DetectionEvent {
    QString   timestampRaw;   // exactly as sent by the board
    QDateTime timestamp;      // parsed; invalid if unparseable
    QDateTime receivedAt;     // desktop-side arrival time
    int       classIdx = -1;
    QString   className;
    double    confidence = 0.0;
    QString   host;
    QVector<double> features; // normalized, model input order

    // Filled in by BoardConnection: which configured endpoint delivered this.
    QString endpoint;

    bool isValid() const { return classIdx >= 0 || !className.isEmpty(); }

    // Best available wall-clock for display/ordering.
    QDateTime effectiveTime() const { return timestamp.isValid() ? timestamp : receivedAt; }

    // Parses one telemetry frame payload. Returns nullopt and sets *error
    // when the object carries none of the expected fields.
    static std::optional<DetectionEvent> parse(const QJsonObject &obj, QString *error);
};

// How the desktop turns (className, confidence) into a triage severity.
//
// This is a *console-side presentation choice*, not something the service
// sends — the board reports a class and a confidence and nothing else. The
// board's own alerting (Config::alertStreak / Config::minConfidence, applied
// by the Debouncer before writing to the Wazuh log) is independent of this
// and is not visible over telemetry.
struct SeverityPolicy {
    double criticalAt = 0.90;
    double highAt     = 0.75;
    double mediumAt   = 0.50;  // set this to the board's min_confidence

    // Class names treated as "nothing happened". Compared case-insensitively.
    QStringList normalClasses{ "normal", "benign", "none", "clean", "idle" };

    bool isNormal(const QString &className) const;
    // Returns one of: info | low | medium | high | critical
    QString severityFor(const DetectionEvent &ev) const;
};
