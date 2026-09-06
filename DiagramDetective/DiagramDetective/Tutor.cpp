#include "Tutor.h"
#include "TutorSession.h"
#include "DiagramScene.h"
#include <QSettings>

namespace
{
	const char* const kTutorKey = "tutor/enabled";
}

bool Tutor::isEnabled()
{
	return QSettings("DiagramDetective", "DiagramDetective").value(kTutorKey, true).toBool();
}

void Tutor::setEnabled(bool on)
{
	QSettings("DiagramDetective", "DiagramDetective").setValue(kTutorKey, on);
}

Tutor::~Tutor()
{
}

TutorSession* Tutor::teach(DiagramScene* scene)
{
	if (scene == nullptr)
		return nullptr;
	auto* session = new TutorSession(this, scene);
	scene->beginSession(session);   // cancels a session already running
	session->start();
	return session;
}
