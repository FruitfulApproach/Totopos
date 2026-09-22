#pragma once

#include <QGraphicsTextItem>

class QVariantAnimation;

class Node;

// The text drawn on a node. Double-click it and it becomes an editor with the
// keyboard in it: what you type is in the diagram as you type it, so anything
// built out of this label (a product, a functor's image) follows along.
// Ctrl+Enter, Shift+Enter or clicking away is Done; Escape reverts.
class NodeLabel : public QGraphicsTextItem
{
public:
	NodeLabel(const QString& text, Node* node);
	// out of the scene before the vtable decays - see Node::~Node
	~NodeLabel() override;

	// A label holds TWO strings. m_source is what was typed - v_{x}, F^{op} -
	// and it is the node's id: what gets saved, matched and edited. What the
	// document holds is the DRAWING of that, v with a subscript x, made by
	// Notation::toHtml. The two are never confused: the rendered form is
	// written into the document and never read back out as the name.
	//
	// The one moment they are the same is inside the editor, which shows the
	// source so that the markers can be typed and corrected. beginEdit puts
	// the source in; commitEdit takes it back out and renders it again.
	QString source() const { return m_source; }
	void setSource(const QString& text);

	// Say no, without saying it in words: the label flashes and wears a
	// padlock for a moment. What beginEdit does instead of opening the editor
	// when the name is not this node's to change (see Node::labelIsLocked).
	void showLockedHint();

	QRectF boundingRect() const override;
	// What the letters actually cover, with the line's ascent and descent
	// trimmed off. boundingRect() is a LINE of text - as tall as the font can
	// ever need - and a node that is nothing but its name was getting that
	// whole line as its box, which is a wide empty border above and below a
	// single D. Falls back to boundingRect() for anything laid out in more
	// than a plain single line.
	QRectF inkRect() const;
	void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

	void beginEdit();
	void commitEdit();   // keep what was typed
	void cancelEdit();   // put back what was there before
	bool isEditing() const { return m_editing; }

	// THE NODE IS GOING. STOP, AND SAY NOTHING BACK TO IT.
	//
	// A label is destroyed by its node, from inside ~Node - and a
	// QGraphicsTextItem that has the keyboard gives the focus up as it is
	// destroyed, which sends it a focusOutEvent, which commits the edit,
	// which calls back into the node. By then the node is half destroyed:
	// everything it IS beyond a bare Node has already been unmade, and the
	// call lands on a vtable that no longer describes it (placeLabel,
	// refreshFrame, and through them the scene's own index). Node's
	// destructor calls this first, so the editor is shut down quietly and
	// the label has nobody to report to.
	void abandonEdit();

protected:
	void keyPressEvent(QKeyEvent* event) override;
	void focusOutEvent(QFocusEvent* event) override;
	// dragging it about (when the node allows it), and telling the node where
	// it ended up
	QVariant itemChange(GraphicsItemChange change, const QVariant& value) override;
	void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
	void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;
	void hoverEnterEvent(QGraphicsSceneHoverEvent* event) override;
	void hoverLeaveEvent(QGraphicsSceneHoverEvent* event) override;

private:
	// let go of the selection the editor made, so it is not painted for ever
	void dropSelection();
	// put the drawn form of m_source into the document
	void renderSource();

	QVariantAnimation* m_hint = nullptr;
	qreal m_hintLevel = 0.0;   // 0 nothing, 1 full pulse
	bool m_hinting = false;    // the rect is wider while the padlock is out

	Node* m_node = nullptr;
	QString m_source;    // as typed: v_{x}
	QString m_before;    // the source as it was when the editor opened
	QPointF m_dragFrom;
	bool m_editing = false;
	bool m_wasMovable = false;
	bool m_dragging = false;
	QMetaObject::Connection m_live;
};
