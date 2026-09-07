#pragma once

#include <QObject>
#include "Tutor.h"

class DiagramScene;

// What the program says when it opens: draw the setup you want to chase. It
// only explains - every click goes to the canvas - and its Done button starts
// the chase.
class SetupTutor : public QObject, public Tutor
{
	Q_OBJECT

public:
	explicit SetupTutor(DiagramScene* scene);

	QString tutorTitle() const override { return QStringLiteral("Draw your setup"); }
	QString doneLabel() const override { return QStringLiteral("Start the chase"); }
	bool capturesClicks() const override { return false; }

protected:
	void onBegin(TutorSession& session) override;
	bool onDone(TutorSession& session) override;

private:
	DiagramScene* m_scene = nullptr;
};
