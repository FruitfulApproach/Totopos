#pragma once

#include <QGraphicsTextItem>

class Node;

// The text drawn on a node. Double-click it and it becomes an editor with the
// keyboard in it: what you type is in the diagram as you type it, so anything
// built out of this label (a product, a functor's image) follows along.
// Ctrl+Enter, Shift+Enter or clicking away is Done; Escape reverts.
class NodeLabel : public QGraphicsTextItem
{
public:
	NodeLabel(const QString& text, Node* node);

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

	void beginEdit();
	void commitEdit();   // keep what was typed
	void cancelEdit();   // put back what was there before
	bool isEditing() const { return m_editing; }

protected:
	void keyPressEvent(QKeyEvent* event) override;
	void focusOutEvent(QFocusEvent* event) override;
	// dragging it about (when the node allows it), and telling the node where
	// it ended up
	QVariant itemChange(GraphicsItemChange change, const QVariant& value) override;
	void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
	void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;

private:
	// let go of the selection the editor made, so it is not painted for ever
	void dropSelection();
	// put the drawn form of m_source into the document
	void renderSource();

	Node* m_node = nullptr;
	QString m_source;    // as typed: v_{x}
	QString m_before;    // the source as it was when the editor opened
	QPointF m_dragFrom;
	bool m_editing = false;
	bool m_wasMovable = false;
	bool m_dragging = false;
	QMetaObject::Connection m_live;
};
