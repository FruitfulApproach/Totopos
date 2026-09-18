#include "tutor/ProofTutor.h"

#include <QDataStream>

#include "art/DiagramScene.h"
#include "tutor/TutorSession.h"
#include "art/Arrow.h"
#include "core/history/SceneHistory.h"
#include "core/history/Memento.h"

namespace
{
	// The labels a step was about, read back out of what it recorded. Only the
	// steps that made or took away nodes name any; a step of reasoning names
	// none, and lights nothing up.
	QStringList labelsIn(quint16 tag, const QByteArray& payload)
	{
		QStringList labels;
		if (tag != MementoTag::NodesCreated && tag != MementoTag::NodesRemoved)
			return labels;
		QDataStream in(payload);
		in.setVersion(QDataStream::Qt_6_0);
		qint32 count = 0;
		in >> count;
		for (qint32 i = 0; i < count && in.status() == QDataStream::Ok; ++i)
		{
			QString kind, id;
			QList<int> path;
			in >> kind >> id >> path;
			if (!id.isEmpty())
				labels << id;
		}
		return labels;
	}
}

ProofTutor::ProofTutor(DiagramScene* scene)
	: QObject(scene)
	, m_scene(scene)
{
	if (m_scene == nullptr || m_scene->history() == nullptr)
		return;
	for (Memento* step : m_scene->history()->mementos())
	{
		if (step == nullptr || step->isPureGraphical())
			continue;   // where something was dragged to is not part of the argument
		m_steps << step->describe();
		m_touched << labelsIn(step->typeTag(), step->payload());
	}
}

QString ProofTutor::tutorTitle() const
{
	const QString name = m_scene != nullptr ? m_scene->statementName() : QString();
	if (name.isEmpty())
		return QStringLiteral("The steps that made this");
	return name;
}

void ProofTutor::onBegin(TutorSession& session)
{
	if (m_steps.isEmpty())
	{
		session.say("This diagram was not built here, so it kept no steps. Anything drawn from now on will "
		            "be remembered, and can be walked through afterwards.", nullptr);
		return;
	}
	m_at = 0;
	showStep(session);
}

void ProofTutor::showStep(TutorSession& session)
{
	lightUp(false);
	if (m_at < 0 || m_at >= m_steps.size())
		return;

	// pick out what this step was about, so the eye knows where to look
	m_lit.clear();
	const QStringList wanted = m_touched.at(m_at);
	if (!wanted.isEmpty() && m_scene != nullptr)
		for (Node* node : m_scene->labelledNodes())
			if (wanted.contains(node->id()))
				m_lit << QPointer<Node>(node);
	lightUp(true);

	QGraphicsItem* pointAt = m_lit.isEmpty() || m_lit.first().isNull() ? nullptr : m_lit.first().data();
	session.say(QString("Step %1 of %2.\n\n%3").arg(m_at + 1).arg(m_steps.size()).arg(m_steps.at(m_at)),
	            pointAt);
}

bool ProofTutor::onDone(TutorSession& session)
{
	if (m_steps.isEmpty())
		return true;
	++m_at;
	if (m_at >= m_steps.size())
	{
		lightUp(false);
		session.say("That is the whole of it.", nullptr);
		return true;   // the session closes
	}
	showStep(session);
	return false;      // stay open for the next one
}

void ProofTutor::onCancel(TutorSession&)
{
	lightUp(false);
}

void ProofTutor::lightUp(bool on)
{
	for (const QPointer<Node>& node : m_lit)
		if (!node.isNull())
			node->setHighlight(on);
	if (!on)
		m_lit.clear();
}
