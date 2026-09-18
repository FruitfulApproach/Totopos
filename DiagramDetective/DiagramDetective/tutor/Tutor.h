#pragma once

#include <QString>

class DiagramScene;
class Node;
class TutorSession;

// A mixin for anything that can walk the user through an interaction on the
// scene: remarks in a bubble, an arrow at what to look at, clicks captured as
// picks. Not a QObject (a Prop already is one) — the TutorSession does the
// QObject work and calls back into these hooks.
class Tutor
{
public:
	virtual ~Tutor();

	// start a session on the scene (any other session is cancelled)
	TutorSession* teach(DiagramScene* scene);
	virtual QString tutorTitle() const = 0;
	// what the button that ends it says
	virtual QString doneLabel() const { return QStringLiteral("Done"); }
	// Whether this tutor TAKES the clicks. A tutor that walks the user
	// through picking things does; one that only explains what to draw must
	// not, or the user could not draw anything.
	virtual bool capturesClicks() const { return true; }

	// Tutor mode, app-wide and remembered between runs. Off, a session still
	// runs (picks, Done / Cancel) but keeps quiet: no remarks, no arrow.
	static bool isEnabled();
	static void setEnabled(bool on);

protected:
	friend class TutorSession;
	// the first remark(s)
	virtual void onBegin(TutorSession& session) = 0;
	// a node was clicked; true takes it as a pick (it gets a numbered badge)
	virtual bool onPick(TutorSession& session, Node* node) { Q_UNUSED(session); Q_UNUSED(node); return false; }
	// Done pressed (or Enter); true when finished, false to keep going
	virtual bool onDone(TutorSession& session) = 0;
	virtual void onCancel(TutorSession& session) { Q_UNUSED(session); }
};
