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
	if (m_from.isNull())
		return nullptr;   // nothing to run an arrow from yet

	// It goes in the category the arrow STARTS in - the two ends of an arrow
	// belong to one category, so there is only one place a new end could go.
	Category* home = m_from->surroundingCategory();
	if (home == nullptr)
		return nullptr;
	if (!home->mapRectToScene(home->boxRect()).contains(scenePos))
	{
		session.say(QString("An arrow joins two objects of one category. Put it down inside %1, "
		                    "where %2 is.").arg(home->id(), m_from->id()), home);
		return nullptr;
	}

	Object* made = home->createCanvasObject(scenePos);
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
