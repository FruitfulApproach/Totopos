#pragma once

#include <QObject>
#include <QPointer>
#include "tutor/Tutor.h"
#include "art/Node.h"

class DiagramScene;

// Placing an arrow, step by step: click where it comes from, click where it
// goes to. With tutor mode off it still runs - it is what takes the clicks -
// only without the remarks and the pointer, so drawing an arrow feels the
// same either way.
class ArrowTutor : public QObject, public Tutor
{
	Q_OBJECT

public:
	// `from` is known already when the arrow was started from a node's handle
	explicit ArrowTutor(DiagramScene* scene, Node* from = nullptr);

	QString tutorTitle() const override { return QStringLiteral("Draw an arrow"); }

protected:
	void onBegin(TutorSession& session) override;
	bool onPick(TutorSession& session, Node* node) override;
	bool onDone(TutorSession& session) override;
	void onCancel(TutorSession& session) override;

private:
	DiagramScene* m_scene = nullptr;
	QPointer<Node> m_from;
	QPointer<Node> m_to;
};
