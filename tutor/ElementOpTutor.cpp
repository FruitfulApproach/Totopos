#include "tutor/ElementOpTutor.h"

#include "art/DiagramScene.h"
#include "art/AtomicElement.h"
#include "art/Object.h"
#include "art/Arrow.h"
#include "tutor/TutorSession.h"

ElementOpTutor::ElementOpTutor(DiagramScene* scene, Node* first, Operation operation)
	: QObject(scene)
	, m_scene(scene)
	, m_first(first)
	, m_operation(operation)
{
}

QString ElementOpTutor::sign() const
{
	switch (m_operation)
	{
	case Operation::Plus:   return QStringLiteral("+");
	case Operation::Equals: return QStringLiteral("=");
	case Operation::Minus:  break;
	}
	return QStringLiteral("-");
}

QString ElementOpTutor::tutorTitle() const
{
	switch (m_operation)
	{
	case Operation::Plus:   return QStringLiteral("Add two elements");
	case Operation::Equals: return QStringLiteral("Say two elements are equal");
	case Operation::Minus:  break;
	}
	return QStringLiteral("Subtract one element from another");
}

Object* ElementOpTutor::home() const
{
	if (m_first.isNull())
		return nullptr;
	return dynamic_cast<Object*>(m_first->parentItem());
}

void ElementOpTutor::onBegin(TutorSession& session)
{
	if (m_first.isNull())
		return;
	const QString where = home() != nullptr ? home()->id() : QStringLiteral("it");
	if (m_operation == Operation::Equals)
	{
		session.say(QString("%1 = what? Click the element of %2 that %1 is equal to. Esc cancels.")
			.arg(m_first->id(), where), m_first.data());
		return;
	}
	session.say(QString("%1 %2 what? Click another element of %3 - or %1 again, which gives "
	                    "%1 %2 %1. Esc cancels.")
		.arg(m_first->id(), sign(), where), m_first.data());
}

bool ElementOpTutor::onPick(TutorSession& session, Node* node)
{
	if (node == nullptr || m_first.isNull())
		return false;

	if (dynamic_cast<AtomicElement*>(node) == nullptr)
	{
		session.say("That is not an element. Elements are the things drawn inside an object - "
		            "point at one of those.", node);
		return false;
	}
	// Both have to be elements of the SAME object: x in M and z in N are not
	// things that can be added, there being no one module they both live in.
	if (node->parentItem() != m_first->parentItem())
	{
		session.say(QString("%1 is not an element of %2, so there is nowhere the two of them could "
		                    "be added. Pick one drawn inside %2.")
			.arg(node->id(), home() != nullptr ? home()->id() : QStringLiteral("it")), node);
		return false;
	}

	// The same one over again is allowed: x + x is an element like any other.
	// Not for an equals, though - x = x is drawn as a line from a thing to
	// itself, and it says nothing that was not already so.
	if (m_operation == Operation::Equals && node == m_first.data())
	{
		session.say(QString("%1 is equal to itself whatever else is true. Point at the OTHER element "
		                    "it is equal to.").arg(m_first->id()), node);
		return false;
	}
	m_second = node;
	QMetaObject::invokeMethod(&session, "done", Qt::QueuedConnection);
	return true;
}

bool ElementOpTutor::onDone(TutorSession& session)
{
	Object* into = home();
	if (m_first.isNull() || m_second.isNull() || into == nullptr)
	{
		session.say("Still need the second element.", m_first.data());
		return false;
	}

	if (m_operation == Operation::Equals)
	{
		// Nothing new is made: the two that are there are joined, with a line
		// that has no head because it does not go anywhere - it says the thing
		// at one end IS the thing at the other.
		auto* equals = new Arrow(QString(), m_first.data(), m_second.data(), into);
		equals->setStyle(Arrow::Style::Equals);
		equals->setZValue(2);
		equals->refreshDepthAppearance();
		equals->refreshFrame();
		m_scene->recordCreation(QString("Said %1 = %2").arg(m_first->id(), m_second->id()), { equals });
		return true;
	}

	// Between the two, and a little below, so it does not land on either.
	const QPointF between = (m_first->pos() + m_second->pos()) / 2.0 + QPointF(0, 40);
	AtomicElement* made = into->createElement(into->mapToScene(between));
	if (made == nullptr)
	{
		session.say("That element could not be made.", nullptr);
		return true;
	}

	// A label built out of the two it is made of, not a string copied from
	// them: rename x and this follows, the way a composite arrow does.
	made->setDerivedLabel(QString("%1 %2 %3").arg("%1", sign(), "%2"),
	                      { m_first.data(), m_second.data() });
	m_scene->recordCreation(QString("Put %1 in %2").arg(made->id(), into->id()), { made });
	return true;
}

void ElementOpTutor::onCancel(TutorSession&)
{
	m_scene->hideHandles();
}
