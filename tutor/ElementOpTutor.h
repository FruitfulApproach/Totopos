#pragma once

#include <QObject>
#include <QPointer>
#include "tutor/Tutor.h"
#include "art/Node.h"

class DiagramScene;
class Object;

// Adding two elements of a module, or subtracting one from another. Press +
// or - beside an element and this takes the clicks from there: point at the
// second element and x + y is drawn beside them, in the same object they are
// elements of.
//
// The second may be the FIRST one over again - x + x is a perfectly good
// element, and so is x - x - so picking the one you started from is allowed
// rather than refused.
class ElementOpTutor : public QObject, public Tutor
{
	Q_OBJECT

public:
	// Plus and Minus BUILD a new element out of two; Equals draws nothing new
	// and joins the two that are there with a double line, saying they are the
	// same element.
	enum class Operation { Plus, Minus, Equals };

	ElementOpTutor(DiagramScene* scene, Node* first, Operation operation);

	QString tutorTitle() const override;

protected:
	void onBegin(TutorSession& session) override;
	bool onPick(TutorSession& session, Node* node) override;
	bool onDone(TutorSession& session) override;
	void onCancel(TutorSession& session) override;

private:
	// the sign as it is written between the two names
	QString sign() const;
	// the object both elements belong to, or nullptr when they do not share one
	Object* home() const;

	DiagramScene* m_scene = nullptr;
	QPointer<Node> m_first;
	QPointer<Node> m_second;
	Operation m_operation = Operation::Plus;
};
