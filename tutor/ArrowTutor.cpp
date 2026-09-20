#include "tutor/ArrowTutor.h"

#include "art/DiagramScene.h"
#include "tutor/TutorSession.h"
#include "art/Arrow.h"
#include "art/Category.h"
#include "art/Object.h"

ArrowTutor::ArrowTutor(DiagramScene* scene, Node* from)
	: QObject(scene)
	, m_scene(scene)
	, m_from(from)
{
}

void ArrowTutor::onBegin(TutorSession& session)
{
	if (m_from.isNull())
	{
		session.say("Click the object the arrow comes FROM, then the object it goes TO. Esc cancels.", nullptr);
		return;
	}
	m_scene->setArrowPreviewSource(m_from.data());
	session.say(QString("From %1. Now click the object the arrow goes TO - it must be drawn in the same category. "
	                    "Esc cancels.").arg(m_from->id()), m_from.data());
}

bool ArrowTutor::onPick(TutorSession& session, Node* node)
{
	if (node == nullptr)
		return false;
	if (node == m_scene->ambientCategory())
	{
		session.say("That is the canvas itself. Pick one of the objects drawn in it.", nullptr);
		return false;
	}

	if (m_from.isNull())
	{
		m_from = node;
		m_scene->setArrowPreviewSource(node);
		session.say(QString("From %1. Now click the object it goes TO.").arg(node->id()), node);
		return true;
	}

	if (node == m_from)
	{
		session.say("That is where the arrow starts. Pick the object it goes to. "
		            "(An arrow from an object to itself is not drawn this way.)", node);
		return false;
	}

	// both ends must be drawn in the same category: that category is what
	// makes the arrow, and it decides what kind of arrow it is
	Category* home = m_from->surroundingCategory();
	if (home == nullptr || home != node->surroundingCategory())
	{
		session.say(QString("%1 is not in the same category as %2, so no arrow of one category joins them. "
		                    "Pick something drawn beside %2.").arg(node->id(), m_from->id()), node);
		return false;
	}

	m_to = node;
	// that is everything: finish without making the user press Done
	QMetaObject::invokeMethod(&session, "done", Qt::QueuedConnection);
	return true;
}

Node* ArrowTutor::onPlace(TutorSession& session, const QPointF& scenePos)
{
	Q_UNUSED(session);   // nothing to refuse any more: it goes down where it was put
	if (m_from.isNull())
		return nullptr;   // nothing to run an arrow from yet

	// IT GOES WHERE IT WAS PUT DOWN, IN WHATEVER THE OTHER END IS DRAWN IN.
	//
	// Whatever is drawn is drawn IN something, and that something decides what
	// may be put beside it: an object of a category, an element of something
	// whose objects are sets. So the holder is asked rather than guessed - the
	// nearest thing above the domain that can hold children.
	Object* home = nullptr;
	for (QGraphicsItem* up = m_from->parentItem(); up != nullptr && home == nullptr; up = up->parentItem())
		if (auto* holder = dynamic_cast<Object*>(up); holder != nullptr && holder->canHoldNamedChildren())
			home = holder;
	if (home == nullptr)
		home = m_from->surroundingCategory();
	if (home == nullptr)
		return nullptr;

	// OUTSIDE ITS FRAME IS NOT OUTSIDE IT.
	//
	// This used to refuse a point beyond the holder's box, on the grounds
	// that an object of a category should not sit outside the thing it is an
	// object of. But a box is not what makes it one: a node is in a category
	// because it is drawn in it, and the frame is only the union of what is
	// held - so putting one down out here simply grows the frame to reach it,
	// which is what happens when an object already inside is dragged out.
	//
	// Refusing was the whole of the trouble: the arrow is dragged CLEAR of
	// the box precisely to leave room for the other end, so the commonest
	// place to put it down was the one place that did nothing. It goes down
	// exactly where it was asked for.
	const QString name = [home] {
		if (auto* category = dynamic_cast<Category*>(home))
			return category->nextObjectName();
		return home->nextElementName();
	}();
	Object* made = home->createNamedChild(name, scenePos);
	if (made == nullptr)
		return nullptr;
	m_scene->recordCreation(QString("Placed %1 in %2").arg(made->id(), home->id()), { made });
	return made;
}

bool ArrowTutor::onDone(TutorSession& session)
{
	if (m_from.isNull())
	{
		session.say("Nothing picked yet: click the object the arrow comes from.", nullptr);
		return false;
	}
	if (m_to.isNull())
	{
		session.say(QString("Still need the object the arrow goes to, from %1.").arg(m_from->id()), m_from.data());
		return false;
	}
	m_scene->setArrowPreviewSource(m_from.data());   // finishArrow reads the source from here
	const bool made = m_scene->finishArrow(m_to.data()) != nullptr;
	if (!made)
		session.say("That arrow could not be made.", nullptr);
	return true;   // either way this session is over
}

void ArrowTutor::onCancel(TutorSession&)
{
	m_scene->hideArrowPreview();
	m_scene->hideHandles();
}
