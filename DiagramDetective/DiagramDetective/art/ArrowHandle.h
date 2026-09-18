#pragma once

#include <QGraphicsObject>
#include <QPointer>
#include "art/Node.h"   // QPointer<Node>::data() needs the complete type (moc compiles this header alone)

// The little round button with a triangle in it that appears UNDER THE CURSOR
// when the mouse comes near the border of a node. Press it - one press, no
// double-click - and an arrow is drawn out of that node.
//
// It takes no mouse of its own. Because it sits exactly on the cursor it would
// otherwise be what every click landed on, and nothing underneath could ever
// be reached; instead the scene watches for a press while it is up (see
// DiagramScene::mousePressEvent) and hit-testing skips it entirely.
//
// A scene-level item at screen size, never a child of the node: a child would
// count towards the node's frame.
class ArrowHandle : public QGraphicsObject
{
	Q_OBJECT

public:
	explicit ArrowHandle(QGraphicsItem* parent = nullptr);

	// put it at `scenePos` as the way out of `from`
	void showFor(Node* from, const QPointF& scenePos);
	void hideHandle();
	Node* node() const { return m_node.data(); }

	// How near the border counts as near, in scene units. Deliberately small:
	// while the button is up it takes the press, so every pixel of reach is a
	// pixel the node cannot be picked up by. It should feel like aiming at the
	// edge, not like the edge reaching out.
	static qreal reach() { return 3.5; }

	QRectF boundingRect() const override;
	QPainterPath shape() const override;
	void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

private:
	QPointer<Node> m_node;
};
