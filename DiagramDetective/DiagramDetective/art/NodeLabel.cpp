#include "art/NodeLabel.h"
#include "art/Node.h"

#include <QKeyEvent>
#include <QTextDocument>
#include <QTextCursor>
#include <QGraphicsSceneMouseEvent>
#include "art/DiagramScene.h"
#include "core/Notation.h"

NodeLabel::NodeLabel(const QString& text, Node* node)
	: QGraphicsTextItem(node)
	, m_node(node)
	, m_source(text)
{
	renderSource();
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
	// An OBJECT'S label is the handle its object is moved by: a press here,
	// outside the editor, hands the object to the scene as a move. An arrow's
	// label is movable on its own, and keeps its own drag.
	if (event->button() == Qt::LeftButton && !m_editing && m_node != nullptr && !m_node->labelIsMovable())
	{
		if (auto* diagram = dynamic_cast<DiagramScene*>(scene()))
		{
			diagram->beginPress(m_node, event->scenePos(), DiagramScene::Gesture::Move);
			if (!m_node->isSelected())
			{
				scene()->clearSelection();
				m_node->setSelected(true);
			}
			event->accept();
			return;
		}
	}
	m_dragFrom = pos();
	m_dragging = (flags() & ItemIsMovable) != 0 && event->button() == Qt::LeftButton;
	// the label is its node's name: picking it up is picking that node out,
	// even though the drag moves only the name
	if (m_dragging && m_node != nullptr && !m_node->isSelected() && scene() != nullptr)
	{
		scene()->clearSelection();
		m_node->setSelected(true);
	}
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

void NodeLabel::setSource(const QString& text)
{
	if (m_source == text)
		return;
	m_source = text;
	if (!m_editing)
		renderSource();   // mid-edit the document belongs to the person typing
}

void NodeLabel::renderSource()
{
	// Plain text unless there is something to raise or lower. setHtml on an
	// ordinary name would put it through the rich-text parser for nothing,
	// and & or < in a name would have to be escaped on the way in.
	if (Notation::hasScripts(m_source))
		setHtml(Notation::toHtml(m_source));
	else
		setPlainText(m_source);
}

void NodeLabel::beginEdit()
{
	if (m_editing)
		return;
	m_editing = true;
	m_before = m_source;
	// the editor shows the SOURCE: v_{x}, not v with a subscript. The markers
	// are what is being edited, so they have to be there to be typed.
	setPlainText(m_source);

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
		// the source follows the keyboard, so id() is live while typing and
		// whatever is built out of this label keeps up
		m_source = toPlainText();
		if (m_node != nullptr)
			m_node->labelBeingEdited(m_source);
	});
}

void NodeLabel::commitEdit()
{
	if (!m_editing)
		return;
	m_editing = false;
	disconnect(m_live);
	m_source = toPlainText();
	dropSelection();
	setTextInteractionFlags(Qt::NoTextInteraction);
	setFlag(ItemIsMovable, m_wasMovable);
	clearFocus();
	renderSource();   // out of the editor, so back to the drawn form
	if (m_node != nullptr)
		m_node->finishLabelEdit(m_before, m_source);
}

void NodeLabel::cancelEdit()
{
	if (!m_editing)
		return;
	m_editing = false;
	disconnect(m_live);
	m_source = m_before;      // as it was before the editor opened
	dropSelection();
	setTextInteractionFlags(Qt::NoTextInteraction);
	setFlag(ItemIsMovable, m_wasMovable);
	clearFocus();
	renderSource();
	if (m_node != nullptr)
		m_node->finishLabelEdit(m_before, m_before);
}

void NodeLabel::dropSelection()
{
	// The whole label was selected when the editor opened, so that typing
	// replaced it. A QGraphicsTextItem goes on painting a selection after the
	// editor is closed, so it has to be let go of by hand.
	QTextCursor cursor = textCursor();
	cursor.clearSelection();
	cursor.movePosition(QTextCursor::End);
	setTextCursor(cursor);
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
