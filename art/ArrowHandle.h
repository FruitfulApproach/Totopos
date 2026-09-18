#pragma once

#include <QGraphicsObject>
#include <QPointer>
#include <QList>
#include "art/Node.h"   // QPointer<Node>::data() needs the complete type (moc compiles this header alone)

// The little row of round buttons that appears UNDER THE CURSOR when the mouse
// comes to the border of a node. What it offers depends on what the node is:
//
//   an object or a category   draw an arrow out of it
//
// An ELEMENT is offered nothing here: what can be done with one (x + y, x - y,
// x = y) needs a word of explanation each, and lives on its right-click menu.
//
// Press one - a single press, no double-click - and the thing happens.
//
// It takes no mouse of its own. Because it sits on the cursor it would
// otherwise be what every click landed on, and nothing underneath could ever
// be reached; instead the scene watches for a press while it is up (see
// DiagramScene::mousePressEvent), asks which button that was, and hit-testing
// skips the handle entirely.
//
// A scene-level item at screen size, never a child of the node: a child would
// count towards the node's frame.
class ArrowHandle : public QGraphicsObject
{
	Q_OBJECT

public:
	enum class Button
	{
		DrawArrow,   // a triangle: the arrow comes out of here
	};

	explicit ArrowHandle(QGraphicsItem* parent = nullptr);

	// Put the row at `scenePos`, offering these, on behalf of `from`.
	// A row that staysPut() is placed once and then left alone, however
	// often this is called again with the same node and the same buttons.
	void showFor(Node* from, const QList<Button>& buttons, const QPointF& scenePos);
	void hideHandle();
	Node* node() const { return m_node.data(); }
	const QList<Button>& buttons() const { return m_buttons; }

	// Does this row hold still once it is up? It does.
	//
	// It used to ride the cursor, which was possible because it appeared the
	// instant the mouse came near a border - always already under the finger,
	// nothing to aim at. It is now asked for by RESTING near a border, and
	// what is asked for has to stay where it was put long enough to be
	// reached: one that slid away from under the hand on the way to it could
	// never be pressed.
	bool staysPut() const { return true; }

	// Is that scene point on one of the circles, give or take `slack`? Used
	// to keep a placed row up while the cursor is on it.
	bool coversScenePos(const QPointF& scenePos, qreal slack = 0.0) const;

	// Which button is at that scene point, or nothing. The row is centred on
	// the cursor, so with one button this is always that button; with two it
	// is whichever half the press landed in.
	bool buttonAt(const QPointF& scenePos, Button& which) const;

	// how near the border counts as near, in scene units
	static qreal reach() { return 3.5; }

	QRectF boundingRect() const override;
	QPainterPath shape() const override;
	void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

private:
	// the middle of button `index`, in this item's coordinates
	QPointF centreOf(int index) const;

	QPointer<Node> m_node;
	QList<Button> m_buttons;
};
