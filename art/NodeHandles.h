#pragma once

#include <QGraphicsObject>
#include <QPointer>
#include <QList>
#include "art/Node.h"   // QPointer<Node>::data() needs the complete type (moc compiles this header alone)

// The little bar beside a node, holding the one button left: the runner, on an
// arrow whose domain holds elements, which chases them across.
//
// Draw-an-arrow and delete used to live here too. An arrow is now begun by
// double-clicking a node's back surface, and deleting is on the right-click
// menu and the Delete key - so a bar that appeared under everything the mouse
// passed over was in the way of the diagram for no gain.
//
// A scene-level item, never a child of the node: a child would count towards
// the node's frame. Under an object; beside an arrow, on the side away from
// its label.
class NodeHandles : public QGraphicsObject
{
	Q_OBJECT

public:
	enum Button { Chase };

	explicit NodeHandles(QGraphicsItem* parent = nullptr);
	~NodeHandles() override;   // out of the scene first - see Node::~Node

	void attach(Node* node, const QPointF& itemPos);
	void detach();
	Node* node() const { return m_node.data(); }

	QRectF boundingRect() const override;
	QPainterPath shape() const override;   // only the buttons take the mouse
	void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

signals:
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
