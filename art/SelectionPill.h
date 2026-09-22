#pragma once

#include <QGraphicsObject>
#include <QList>
#include <QRectF>
#include <QString>

// THE LITTLE BAR THAT APPEARS OVER WHAT HAS BEEN PICKED OUT.
//
// Having picked several things out, what anybody wants is to take them
// somewhere - so that is offered where the answer is, over the selection,
// rather than in a panel across the window or a menu that has to be known
// about.
//
//   the two sheets   copy them and carry them (see Carry)
//
// The book that stood beside it is gone with the tags it opened into: a file
// is ONE statement, said once at the top of the window, and not something a
// handful of picked-out items is asked about (see Node).
//
// It is drawn at a fixed size whatever the zoom
// (ItemIgnoresTransformations): these are controls, not part of the diagram,
// and a control that shrinks with the drawing is one nobody can press.
class SelectionPill : public QGraphicsObject
{
	Q_OBJECT

public:
	explicit SelectionPill(QGraphicsItem* parent = nullptr);

	QRectF boundingRect() const override;
	void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

	// put it above the middle of that rectangle, in scene coordinates
	void hoverOver(const QRectF& sceneBounds);
	// show or hide the add-subnode button (only for object selections)
	void setShowAddNode(bool show);

signals:
	// the sheets: take what is picked out and carry it
	void copyAsked();
	// the plus: add a new object inside the selected node
	void addNodeAsked();

protected:
	void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
	void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;
	void hoverMoveEvent(QGraphicsSceneHoverEvent* event) override;
	void hoverLeaveEvent(QGraphicsSceneHoverEvent* event) override;

private:
	// What the bar is made of, worked out once and shared by the drawing and
	// the pressing, so what is seen and what answers are the same shapes.
	struct Button
	{
		QRectF where;
		bool isCopy = false;
	};
	QList<Button> buttons() const;
	int buttonAt(const QPointF& where) const;

	int m_hot = -1;        // which button the mouse is on
	int m_pressed = -1;
	bool m_showAddNode = false;
};
