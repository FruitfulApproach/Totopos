#include "tutor/SetupTutor.h"

#include "art/DiagramScene.h"
#include "tutor/TutorSession.h"
#include "art/Category.h"

SetupTutor::SetupTutor(DiagramScene* scene)
	: QObject(scene)
	, m_scene(scene)
{
}

void SetupTutor::onBegin(TutorSession& session)
{
	const QString ambient = m_scene->ambientCategory() != nullptr ? m_scene->ambientCategory()->id() : QString("the canvas");
	session.say(QString(
		"Draw the setup you want to chase, in %1.\n\n"
		"•  Double-click the canvas to place an object, or right-click ▸ Add object.\n"
		"•  Drag an object to move it.\n"
		"•  Double-click an object's back surface - not its name, which opens the editor - to "
		"draw an arrow out of it. The arrow then follows the mouse; click what it goes to, or "
		"right-click or press Esc to abandon it. Both ends must be drawn in the same category.\n"
		"•  An object is part of another one? Draw them side by side and give the arrow between "
		"them the Inclusion style, from its right-click menu.\n"
		"•  Turn on Commutes in the panel if every path with the same ends is meant to be equal.\n\n"
		"When the setup is drawn, press Start the chase (Ctrl+Shift+Enter). From then on anything you "
		"add is forced into the hypotheses.").arg(ambient),
		m_scene->ambientCategory());
}

bool SetupTutor::onDone(TutorSession&)
{
	m_scene->setChasing(true);
	return true;
}
