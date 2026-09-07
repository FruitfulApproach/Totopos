#include "SetupTutor.h"

#include "DiagramScene.h"
#include "TutorSession.h"
#include "Category.h"

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
		"•  Double-click the canvas to place an object.\n"
		"•  Double-click an object to place one INSIDE it - objects are categories in their own "
		"right, so the nesting goes as deep as you like.\n"
		"•  Click an object and press the ▶ handle beside it, then click where the arrow goes. "
		"Both ends must be drawn in the same category.\n"
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
