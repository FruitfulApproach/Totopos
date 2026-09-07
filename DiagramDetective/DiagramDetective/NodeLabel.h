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
	Node* m_node = nullptr;
	QString m_before;
	QPointF m_dragFrom;
	bool m_editing = false;
	bool m_wasMovable = false;
	bool m_dragging = false;
	QMetaObject::Connection m_live;
};
