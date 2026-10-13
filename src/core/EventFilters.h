#pragma once

#include <QObject>
#include <QString>
#include <QVariantMap>

// The Events screen's filter state, and the query string it renders to.
//
// Every property maps onto exactly one query param from SIEM_API_DOCUMENTATION
// §5, with no client-side interpretation: the console does not re-filter rows
// it already has, it asks the backend again. That keeps the row count in the
// header (`count` from the paginated envelope) meaning what it says — the
// number of matching rows in the database, not the number that happen to be
// loaded.
//
// Values are passed through as typed, deliberately. An unparseable `hours` is
// the backend's 400 to explain (§6 names the offending field), and inventing a
// second validator here would only produce a different error message for the
// same input.
class EventFilters : public QObject {
    Q_OBJECT
    // Time (§5.1). `quickRange` is the segmented control; since/until/date are
    // the explicit fields. Setting one clears the other, because the API would
    // otherwise apply both.
    Q_PROPERTY(QString quickRange READ quickRange WRITE setQuickRange NOTIFY changed)
    Q_PROPERTY(QString since READ since WRITE setSince NOTIFY changed)
    Q_PROPERTY(QString until READ until WRITE setUntil NOTIFY changed)
    Q_PROPERTY(QString date READ date WRITE setDate NOTIFY changed)

    // Type & attributes (§5.2).
    Q_PROPERTY(QString type READ type WRITE setType NOTIFY changed)
    Q_PROPERTY(QString className READ className WRITE setClassName NOTIFY changed)
    Q_PROPERTY(QString host READ host WRITE setHost NOTIFY changed)
    Q_PROPERTY(QString sourceTopic READ sourceTopic WRITE setSourceTopic NOTIFY changed)
    Q_PROPERTY(QString minConfidence READ minConfidence WRITE setMinConfidence NOTIFY changed)

    // Sorting & paging (§5.4).
    Q_PROPERTY(QString ordering READ ordering WRITE setOrdering NOTIFY changed)
    Q_PROPERTY(int pageSize READ pageSize WRITE setPageSize NOTIFY changed)

    Q_PROPERTY(bool isDefault READ isDefault NOTIFY changed)
    Q_PROPERTY(QString summary READ summary NOTIFY changed)

public:
    explicit EventFilters(QObject *parent = nullptr);

    QString quickRange() const { return m_quickRange; }
    void setQuickRange(const QString &v);
    QString since() const { return m_since; }
    void setSince(const QString &v);
    QString until() const { return m_until; }
    void setUntil(const QString &v);
    QString date() const { return m_date; }
    void setDate(const QString &v);

    QString type() const { return m_type; }
    void setType(const QString &v);
    QString className() const { return m_className; }
    void setClassName(const QString &v);
    QString host() const { return m_host; }
    void setHost(const QString &v);
    QString sourceTopic() const { return m_sourceTopic; }
    void setSourceTopic(const QString &v);
    QString minConfidence() const { return m_minConfidence; }
    void setMinConfidence(const QString &v);

    QString ordering() const { return m_ordering; }
    void setOrdering(const QString &v);
    int pageSize() const { return m_pageSize; }
    void setPageSize(int v);

    bool isDefault() const;
    QString summary() const;

    // Query params, empty values omitted. `ordering`/`page_size` are included
    // because both are meaningful defaults rather than absent filters.
    QVariantMap toQuery() const;

    // Toggles between ascending and descending on a column, matching the
    // "column header click" mapping in §7.1.
    Q_INVOKABLE void toggleOrdering(const QString &field);
    Q_INVOKABLE QString orderingFor(const QString &field) const;

    Q_INVOKABLE void clear();

signals:
    void changed();

private:
    void set(QString *slot, const QString &value);

    QString m_quickRange = QStringLiteral("24h"); // "" | 1h | 24h | 7d
    QString m_since, m_until, m_date;
    QString m_type, m_className, m_host, m_sourceTopic, m_minConfidence;
    QString m_ordering = QStringLiteral("-event_time");
    int m_pageSize = 100;
};
