#include "NodeLabel.h"
#include "Node.h"

#include <QKeyEvent>
#include <QTextDocument>
#include <QTextCursor>
#include <QGraphicsSceneMouseEvent>

NodeLabel::NodeLabel(const QString& text, Node* node)
	: QGraphicsTextItem(text, node)
	, m_node(node)
{
	// ItemIsMovable is set by the node that wants it (an arrow does), once its
	// own constructor has run - labelIsMovable() cannot be asked from in here,
	// where the object is still only a Node.
	setFlag(QGraphicsItem::ItemSendsGeometryChanges, true);
}

QVariant NodeLabel::itemChange(GraphicsItemChange change, const QVariant& value)
{
	// ONLY while the user is dragging it. Every other move of this label is
	// the node placing it where it belongs, and reporting that back would have
	// the node record the placement as a displacement from itself.
	if (change == ItemPositionHasChanged && m_dragging && m_node != nullptr)
		m_node->labelMoved(value.toPointF());
	return QGraphicsTextItem::itemChange(change, value);
}

void NodeLabel::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
	m_dragFrom = pos();
	m_dragging = (flags() & ItemIsMovable) != 0 && event->button() == Qt::LeftButton;
	QGraphicsTextItem::mousePressEvent(event);
}

void NodeLabel::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
	QGraphicsTextItem::mouseReleaseEvent(event);
	const bool wasDragging = m_dragging;
	m_dragging = false;
	if (wasDragging && m_node != nullptr && pos() != m_dragFrom)
		m_node->labelDragFinished(m_dragFrom);
}

void NodeLabel::beginEdit()
{
	if (m_editing)
		return;
	m_editing = true;
	m_before = toPlainText();

	setTextInteractionFlags(Qt::TextEditorInteraction);   // this also makes it focusable
	m_wasMovable = (flags() & ItemIsMovable) != 0;
	setFlag(ItemIsMovable, false);   // in the editor the mouse selects text
	setFocus(Qt::MouseFocusReason);

	QTextCursor cursor = textCursor();
	cursor.select(QTextCursor::Document);   // typing replaces the old label
	setTextCursor(cursor);

	// every keystroke is a change to the diagram, not just to this box: the
	// node reframes, and whatever is built out of this label follows
	m_live = connect(document(), &QTextDocument::contentsChanged, this, [this] {
		if (m_node != nullptr)
			m_node->labelBeingEdited(toPlainText());
	});
}

void NodeLabel::commitEdit()
{
	if (!m_editing)
		return;
	m_editing = false;
	disconnect(m_live);
	setTextInteractionFlags(Qt::NoTextInteraction);
	setFlag(ItemIsMovable, m_wasMovable);
	clearFocus();
	if (m_node != nullptr)
		m_node->finishLabelEdit(m_before, toPlainText());
}

void NodeLabel::cancelEdit()
{
	if (!m_editing)
		return;
	m_editing = false;
	disconnect(m_live);
	setPlainText(m_before);   // as it was before the editor opened
	setTextInteractionFlags(Qt::NoTextInteraction);
	setFlag(ItemIsMovable, m_wasMovable);
	clearFocus();
	if (m_node != nullptr)
		m_node->finishLabelEdit(m_before, m_before);
}

void NodeLabel::keyPressEvent(QKeyEvent* event)
{
	if (event->key() == Qt::Key_Escape)
	{
		cancelEdit();
		event->accept();
		return;
	}
	if ((event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)
	 && (event->modifiers() & (Qt::ControlModifier | Qt::ShiftModifier)))
	{
		commitEdit();
		event->accept();
		return;
	}
	QGraphicsTextItem::keyPressEvent(event);
}

void NodeLabel::focusOutEvent(QFocusEvent* event)
{
	QGraphicsTextItem::focusOutEvent(event);
	commitEdit();   // clicking away keeps what was typed
}
