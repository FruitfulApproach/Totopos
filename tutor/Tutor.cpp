#include "tutor/Tutor.h"
#include "tutor/TutorSession.h"
#include "art/DiagramScene.h"
#include <QSettings>

namespace
{
	const char* const kTutorKey = "tutor/enabled";
}

bool Tutor::isEnabled()
{
	return QSettings("Totopos", "Totopos").value(kTutorKey, true).toBool();
}

void Tutor::setEnabled(bool on)
{
	QSettings("Totopos", "Totopos").setValue(kTutorKey, on);
}

Tutor::~Tutor()
{
}

TutorSession* Tutor::teach(DiagramScene* scene)
{
	if (scene == nullptr)
		return nullptr;
	auto* session = new TutorSession(this, scene);
	session->setCapturesClicks(capturesClicks());
	scene->beginSession(session);   // cancels a session already running
	session->start();
	return session;
}
