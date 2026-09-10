#pragma once

#include <QList>
#include <QPointer>
#include <QPointF>
#include <QBrush>
#include <QPen>
#include "Memento.h"
#include "../Node.h"
#include <QGraphicsObject>

class QGraphicsItem;
class Arrow;
class Category;
class DiagramScene;

// ---------------------------------------------------------------- purely graphical

// Where a list of items sits. Dragging a node about proves nothing.
class ItemsMoved : public PureGraphical
{
public:
	struct Move
	{
		QPointer<Node> node;
		QPointF before;
		QPointF after;
	};

	ItemsMoved(const QString& description, const QList<Move>& moves)
		: PureGraphical(description), m_moves(moves) {}

	void undo() override;
	void redo() override;
	quint16 typeTag() const override { return MementoTag::ItemsMoved; }
	const QList<Move>& moves() const { return m_moves; }

private:
	QList<Move> m_moves;
};

// What a node is painted with.
class StyleChanged : public PureGraphical
{
public:
	StyleChanged(const QString& description, Node* node,
	             const QBrush& fillBefore, const QPen& borderBefore,
	             const QBrush& fillAfter, const QPen& borderAfter,
	             qreal radiusBefore = -1, qreal radiusAfter = -1)
		: PureGraphical(description), m_node(node)
		, m_fillBefore(fillBefore), m_borderBefore(borderBefore)
		, m_fillAfter(fillAfter), m_borderAfter(borderAfter)
		, m_radiusBefore(radiusBefore), m_radiusAfter(radiusAfter) {}

	void undo() override;
	void redo() override;
	quint16 typeTag() const override { return MementoTag::StyleChanged; }

private:
	QPointer<Node> m_node;
	QBrush m_fillBefore, m_fillAfter;
	QPen m_borderBefore, m_borderAfter;
	qreal m_radiusBefore = -1;   // below zero: this change did not touch the corners
	qreal m_radiusAfter = -1;
};

// The points an arrow is pulled through. Where a line is drawn says nothing
// about what it means.
class ArrowBent : public PureGraphical
{
public:
	ArrowBent(const QString& description, Node* arrow, const QList<QPointF>& before, const QList<QPointF>& after)
		: PureGraphical(description), m_arrow(arrow), m_before(before), m_after(after) {}

	void undo() override;
	void redo() override;
	quint16 typeTag() const override { return MementoTag::ArrowBent; }

private:
	QPointer<Node> m_arrow;
	QList<QPointF> m_before;
	QList<QPointF> m_after;
};

// Where an arrow's label was put. It says nothing about the mathematics - the
// arrow is the same arrow wherever its name is written - so it is one of the
// changes a proof reading leaves out.
class LabelMoved : public PureGraphical
{
public:
	LabelMoved(const QString& description, Arrow* arrow, const QPointF& before, const QPointF& after);

	void undo() override;
	void redo() override;
	quint16 typeTag() const override { return MementoTag::LabelMoved; }

private:
	QPointer<Arrow> m_arrow;
	QPointF m_before;
	QPointF m_after;
};

// Whether the rows (or the columns) of the diagram in a category are asserted
// to be exact. Not a look: it is half of what a picture like the definition of
// projective actually says.
class ExactnessChanged : public Memento
{
public:
	ExactnessChanged(const QString& description, Category* category, bool rows, bool before, bool after);

	void undo() override;
	void redo() override;
	quint16 typeTag() const override { return MementoTag::Exactness; }
	QByteArray payload() const override;

private:
	QPointer<Category> m_category;
	bool m_rows;
	bool m_before;
	bool m_after;
};

// What the whole picture is put forward as, and what it is called. A step of
// the first order: it is the difference between a drawing and a claim.
class StatementDeclared : public Memento
{
public:
	StatementDeclared(const QString& description, DiagramScene* scene,
	                  int kindBefore, int kindAfter,
	                  const QString& nameBefore, const QString& nameAfter);

	void undo() override;
	void redo() override;
	quint16 typeTag() const override { return MementoTag::Statement; }
	QByteArray payload() const override;

private:
	QPointer<DiagramScene> m_scene;
	int m_kindBefore;
	int m_kindAfter;
	QString m_nameBefore;
	QString m_nameAfter;
};

// ---------------------------------------------------------------- the diagram itself

// Nodes that came into the diagram: an object placed, an arrow drawn, the
// image of a functor. Undone, they are taken out of the scene but NOT
// destroyed - this memento holds them, ready to put back, and destroys them
// only when the history itself lets go.
class NodesCreated : public Memento
{
public:
	NodesCreated(const QString& description, const QList<Node*>& nodes);
	~NodesCreated() override;

	void undo() override;   // take them out
	void redo() override;   // put them back

	quint16 typeTag() const override { return MementoTag::NodesCreated; }
	// what was made, and where: kind, label and the path down from the ambient
	// category, so the step still reads after the file is closed
	QByteArray payload() const override;

	bool isEmpty() const { return m_held.isEmpty(); }

protected:
	struct Held
	{
		QPointer<Node> node;
		// where it was: a category, hence a QGraphicsObject, hence watchable.
		// The ambient category can be swapped out from under a memento, and
		// a raw pointer here would put a node back into a freed parent.
		QPointer<QGraphicsObject> parent;
		QGraphicsScene* scene = nullptr;
		QPointF pos;
		bool owned = false;   // out of the scene: ours to destroy
	};

	// take them out of the scene, keeping them alive and remembering where
	// they were; arrows first, so no arrow is ever left pointing at a node
	// that is no longer in the scene
	void detachAll();
	void attachAll();

	QList<Held> m_held;
};

// Nodes that went out of the diagram: the delete button. The same holding,
// the other way round.
class NodesRemoved : public NodesCreated
{
public:
	NodesRemoved(const QString& description, const QList<Node*>& nodes)
		: NodesCreated(description, nodes) {}

	void undo() override { attachAll(); }
	void redo() override { detachAll(); }
	quint16 typeTag() const override { return MementoTag::NodesRemoved; }
};

// Whether a part of the diagram is given or claimed to exist. Not graphical:
// it is what the diagram SAYS.
class ExistsSuchChanged : public Memento
{
public:
	ExistsSuchChanged(const QString& description, Node* node, bool before, bool after)
		: Memento(description), m_node(node), m_before(before), m_after(after) {}

	void undo() override;
	void redo() override;
	quint16 typeTag() const override { return MementoTag::ExistsSuch; }
	QByteArray payload() const override;

private:
	QPointer<Node> m_node;
	bool m_before;
	bool m_after;
};

// Whether an arrow is asserted cancellable on the left. Not graphical: it is
// a claim about the mathematics, drawn with a hooked tail as a side effect.
class MonicChanged : public Memento
{
public:
	MonicChanged(const QString& description, Arrow* arrow, bool before, bool after)
		: Memento(description), m_arrow(arrow), m_before(before), m_after(after) {}

	void undo() override;
	void redo() override;
	quint16 typeTag() const override { return MementoTag::Monic; }
	QByteArray payload() const override;

private:
	QPointer<Arrow> m_arrow;
	bool m_before;
	bool m_after;
};

// The same, cancellable on the right - drawn with a doubled head.
class EpicChanged : public Memento
{
public:
	EpicChanged(const QString& description, Arrow* arrow, bool before, bool after)
		: Memento(description), m_arrow(arrow), m_before(before), m_after(after) {}

	void undo() override;
	void redo() override;
	quint16 typeTag() const override { return MementoTag::Epic; }
	QByteArray payload() const override;

private:
	QPointer<Arrow> m_arrow;
	bool m_before;
	bool m_after;
};

// What something is called. Not a look: the label IS the object as far as the
// diagram is concerned.
class Renamed : public Memento
{
public:
	Renamed(const QString& description, Node* node, const QString& before, const QString& after)
		: Memento(description), m_node(node), m_before(before), m_after(after) {}

	void undo() override;
	void redo() override;
	quint16 typeTag() const override { return MementoTag::Renamed; }
	QByteArray payload() const override;

private:
	QPointer<Node> m_node;
	QString m_before;
	QString m_after;
};

// A step of reasoning. Nothing on the canvas changes - "since g is onto,
// choose b with g(b) = x" moves the argument, not the picture - but it is
// part of how the thing was arrived at, and a proof is unreadable without it.
class Note : public Memento
{
public:
	explicit Note(const QString& description) : Memento(description) {}

	void undo() override {}
	void redo() override {}
	bool canRestore() const override { return false; }
	quint16 typeTag() const override { return MementoTag::Note; }
};

// A change read back from a file: it says what was done, and that is all it
// can do. Kept in the history so a diagram opened tomorrow still knows what
// was built in it today.
class SealedStep : public Memento
{
public:
	SealedStep(const QString& description, quint16 tag, const QByteArray& payload, bool pureGraphical)
		: Memento(description), m_tag(tag), m_payload(payload), m_pure(pureGraphical) {}

	void undo() override {}
	void redo() override {}
	bool canRestore() const override { return false; }
	bool isPureGraphical() const override { return m_pure; }
	quint16 typeTag() const override { return m_tag; }
	QByteArray payload() const override { return m_payload; }

private:
	quint16 m_tag;
	QByteArray m_payload;
	bool m_pure;
};

// every arrow, anywhere in the scene, that ends on one of these nodes or on
// anything inside them - they have to travel with it
QList<Node*> withAttachedArrows(const QList<Node*>& nodes);
