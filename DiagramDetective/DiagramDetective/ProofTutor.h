#pragma once

#include <QObject>
#include <QList>
#include <QPointer>
#include "Tutor.h"
#include "Node.h"

class DiagramScene;

// Walking a proof, one step at a time. Every diagram keeps the steps that
// made it, so any of them can be gone through again afterwards - which is
// what "teach me this proof" comes to. It takes none of the clicks: the
// diagram stays yours while it talks.
class ProofTutor : public QObject, public Tutor
{
	Q_OBJECT

public:
	explicit ProofTutor(DiagramScene* scene);

	QString tutorTitle() const override;
	QString doneLabel() const override { return QStringLiteral("Next"); }
	bool capturesClicks() const override { return false; }

protected:
	void onBegin(TutorSession& session) override;
	bool onDone(TutorSession& session) override;
	void onCancel(TutorSession& session) override;

private:
	void showStep(TutorSession& session);
	void lightUp(bool on);

	DiagramScene* m_scene = nullptr;
	QList<QString> m_steps;          // what each one says
	QList<QStringList> m_touched;    // and what it was about
	QList<QPointer<Node>> m_lit;
	int m_at = 0;
};
