#include "AlertExplainer.h"

AlertExplainer::AlertExplainer(BackendSettings *backend, QObject *parent)
    : QObject(parent)
    , m_backend(backend)
    , m_chat(new ChatClient(backend, this))
{
    connect(m_chat, &ChatClient::sourcesReceived, this, [this](const QVariantList &s) {
        m_sources = s;
        emit answerChanged();
    });
    // The *visible* text, not the raw deltas: concatenating tokenReceived
    // would put the markdown "**Sources:**" footer back into the body, where
    // the chips below already show it.
    connect(m_chat, &ChatClient::answerTextChanged, this, [this](const QString &visible) {
        m_answer = visible;
        if (m_state != QStringLiteral("streaming"))
            m_state = QStringLiteral("streaming");
        emit answerChanged();
    });
    connect(m_chat, &ChatClient::answerFinished, this, [this]() {
        setState(QStringLiteral("done"));
    });
    connect(m_chat, &ChatClient::errorOccurred, this, [this](const QString &d) {
        m_error = d;
        setState(QStringLiteral("error"));
    });
    connect(m_chat, &ChatClient::busyChanged, this, &AlertExplainer::answerChanged);
    connect(m_backend, &BackendSettings::changed, this, &AlertExplainer::changed);
}

void AlertExplainer::setState(const QString &s)
{
    m_state = s;
    emit answerChanged();
}

void AlertExplainer::explain(const QString &uid, const QString &host,
                             const QString &className, int classIdx,
                             const QString &confidenceText, const QString &timestampRaw,
                             const QString &severity)
{
    // A second right-click while the first answer is still streaming replaces
    // it: the operator has moved on to a different row, and finishing the
    // previous answer into a popup nobody is looking at only holds the
    // backend's single generation slot.
    m_chat->cancel();
    m_chat->newConversation();

    m_active = true;
    m_subject = className.isEmpty() ? QStringLiteral("(no class)") : className;
    m_subjectDetail = QStringLiteral("%1 · %2 · %3")
                          .arg(host, confidenceText, uid);
    m_severity = severity;

    m_answer.clear();
    m_sources.clear();
    m_error.clear();

    // The same construction as the Detections inspector's handoff: only
    // fields the event carries, and an explicit statement that the class name
    // is the *model's* label rather than an ATT&CK identifier the backend
    // already knows. The popup asks for a short answer because it has room
    // for a short answer; "Open in Assistant" is there for the long version.
    m_question =
        QStringLiteral("A host-based IDS on board \"%1\" classified a system-call window as "
                       "\"%2\" (the model's own class label, index %3) with %4 confidence at %5. "
                       "Briefly: which MITRE ATT&CK techniques match that behaviour on a Linux "
                       "edge device, and what should an analyst check first? Keep it to a few "
                       "sentences.")
            .arg(host, className)
            .arg(classIdx)
            .arg(confidenceText, timestampRaw);

    emit changed();

    if (!m_backend->configured()) {
        m_error = QStringLiteral("No backend configured. Set the base URL on the "
                                 "Integrations screen.");
        setState(QStringLiteral("error"));
        return;
    }

    setState(QStringLiteral("waiting"));
    m_chat->ask(m_question);

    // ask() refuses when the shared generation slot is held; it reports that
    // through errorOccurred, which has already moved the state to "error".
}

void AlertExplainer::retry()
{
    if (m_question.isEmpty() || m_chat->busy())
        return;
    m_answer.clear();
    m_sources.clear();
    m_error.clear();
    m_chat->newConversation();
    setState(QStringLiteral("waiting"));
    m_chat->ask(m_question);
}

void AlertExplainer::cancel()
{
    m_chat->cancel();
    if (m_state == QStringLiteral("waiting") || m_state == QStringLiteral("streaming"))
        setState(QStringLiteral("cancelled"));
}

void AlertExplainer::close()
{
    m_chat->cancel();
    m_active = false;
    m_answer.clear();
    m_sources.clear();
    m_error.clear();
    m_state = QStringLiteral("idle");
    emit answerChanged();
    emit changed();
}
