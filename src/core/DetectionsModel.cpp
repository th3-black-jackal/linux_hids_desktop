#include "DetectionsModel.h"

DetectionsModel::DetectionsModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int DetectionsModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return m_filtered.size();
}

QVariant DetectionsModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_filtered.size())
        return {};

    const int origIdx = m_filtered.at(index.row());
    const Detection *d = m_all.at(origIdx);

    switch (role) {
    case NRole:              return QString("%1").arg(index.row() + 1, 2, 10, QChar('0'));
    case UidRole:            return d->uid();
    case TimeRole:           return d->time();
    case SeverityRole:       return d->severity();
    case NodeRole:           return d->node();
    case ClassNameRole:      return d->className();
    case ClassIdxRole:       return d->classIdx();
    case ConfidenceTextRole: return d->confidenceText();
    case StateRole:
        if (!d->disposition().isEmpty())
            return d->disposition();
        return d->acknowledged() ? QStringLiteral("acknowledged") : QStringLiteral("open");
    case OriginalIndexRole:  return origIdx;
    default:                 return {};
    }
}

QHash<int, QByteArray> DetectionsModel::roleNames() const
{
    return {
        { NRole, "n" },
        { UidRole, "uid" },
        { TimeRole, "time" },
        { SeverityRole, "severity" },
        { NodeRole, "node" },
        { ClassNameRole, "className" },
        { ClassIdxRole, "classIdx" },
        { ConfidenceTextRole, "confidenceText" },
        { StateRole, "state" },
        { OriginalIndexRole, "originalIndex" },
    };
}

void DetectionsModel::watch(Detection *d)
{
    auto touch = [this, d]() {
        const int orig = m_all.indexOf(d);
        const int row = m_filtered.indexOf(orig);
        if (row >= 0) {
            const QModelIndex idx = index(row);
            emit dataChanged(idx, idx, { StateRole });
        }
        recomputeCounts();
    };
    connect(d, &Detection::acknowledgedChanged, this, touch);
    connect(d, &Detection::dispositionChanged, this, touch);
}

void DetectionsModel::addDetection(Detection *detection)
{
    detection->setParent(this);
    watch(detection);

    beginResetModel();
    m_all.prepend(detection);
    while (m_all.size() > kMaxRetained) {
        Detection *old = m_all.takeLast();
        old->deleteLater();
    }
    endResetModel();

    applyFilter();
    recomputeCounts();
    emit detectionAdded();
}

Detection *DetectionsModel::detectionAt(int originalIndex) const
{
    if (originalIndex < 0 || originalIndex >= m_all.size())
        return nullptr;
    return m_all.at(originalIndex);
}

void DetectionsModel::toggleAcknowledge(int originalIndex)
{
    if (Detection *d = detectionAt(originalIndex))
        d->setAcknowledged(!d->acknowledged());
}

void DetectionsModel::acknowledgeAll()
{
    for (Detection *d : std::as_const(m_all))
        d->setAcknowledged(true);
}

void DetectionsModel::setSeverityFilter(const QString &sev)
{
    if (m_severityFilter == sev)
        return;
    m_severityFilter = sev;
    emit filterChanged();
    applyFilter();
}

void DetectionsModel::setSearchText(const QString &text)
{
    if (m_searchText == text)
        return;
    m_searchText = text;
    emit filterChanged();
    applyFilter();
}

void DetectionsModel::applyFilter()
{
    beginResetModel();
    m_filtered.clear();
    for (int i = 0; i < m_all.size(); ++i) {
        const Detection *d = m_all.at(i);

        bool matchesSeverity = true;
        if (m_severityFilter == QStringLiteral("open"))
            matchesSeverity = !d->acknowledged();
        else if (m_severityFilter != QStringLiteral("all"))
            matchesSeverity = d->severity() == m_severityFilter;

        bool matchesSearch = true;
        if (!m_searchText.isEmpty()) {
            matchesSearch = d->className().contains(m_searchText, Qt::CaseInsensitive)
                || d->node().contains(m_searchText, Qt::CaseInsensitive)
                || d->endpoint().contains(m_searchText, Qt::CaseInsensitive)
                || d->uid().contains(m_searchText, Qt::CaseInsensitive);
        }

        if (matchesSeverity && matchesSearch)
            m_filtered.append(i);
    }
    endResetModel();
}

void DetectionsModel::recomputeCounts()
{
    int total = 0, crit = 0, high = 0, med = 0;
    for (const Detection *d : std::as_const(m_all)) {
        if (d->acknowledged())
            continue;
        ++total;
        if (d->severity() == QStringLiteral("critical")) ++crit;
        else if (d->severity() == QStringLiteral("high")) ++high;
        else if (d->severity() == QStringLiteral("medium")) ++med;
    }
    if (total != m_openTotal || crit != m_openCritical || high != m_openHigh || med != m_openMedium) {
        m_openTotal = total;
        m_openCritical = crit;
        m_openHigh = high;
        m_openMedium = med;
        emit countsChanged();
    }
}
