#pragma once

#include <QGraphicsObject>
#include <QPointer>
#include <QList>
#include "Node.h"   // QPointer<Node>::data() needs the complete type (moc compiles this header alone)

// The little bar of buttons beside a node. Which buttons depends on what the
// node is: an object shows only delete (dragging it draws an arrow, holding
// it carries it); an arrow shows draw-an-arrow and delete, and - when its
// domain holds any elements - the runner, which chases them across.
//
// A scene-level item, never a child of the node: a child would count towards
// the node's frame. Under an object; beside an arrow, on the side away from
// its label.
class NodeHandles : public QGraphicsObject
{
	Q_OBJECT

public:
	enum Button { DrawArrow, DeleteNode, Chase };

	explicit NodeHandles(QGraphicsItem* parent = nullptr);

	void attach(Node* node, const QPointF& itemPos);
	void detach();
	Node* node() const { return m_node.data(); }

	QRectF boundingRect() const override;
	QPainterPath shape() const override;   // only the buttons take the mouse
	void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

signals:
	void arrowRequested(Node* from);
	void deleteRequested(Node* node);
	// carry the elements of the arrow's domain across, and keep them carried
	void chaseRequested(Node* arrow);

protected:
	void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
	void mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event) override;
	void hoverMoveEvent(QGraphicsSceneHoverEvent* event) override;
	void hoverLeaveEvent(QGraphicsSceneHoverEvent* event) override;

private:
	void place();
	QList<Button> buttons() const;
	QPointF centreOf(int index) const;
	int indexAt(const QPointF& itemPos) const;   // -1 for none

	QPointer<Node> m_node;
	QPointF m_at;                  // where the node was clicked, in its coordinates
	QPointF m_out = QPointF(0, 1); // the way out from the node
	int m_hover = -1;              // index of the button under the mouse
};
