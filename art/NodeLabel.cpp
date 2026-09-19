#include "art/NodeLabel.h"
#include "art/Node.h"

#include <QKeyEvent>
#include <QTextDocument>
#include <QTextCursor>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsScene>
#include "art/DiagramScene.h"
#include "core/Notation.h"
#include "core/Emoji.h"
#include <QVariantAnimation>
#include <QPainter>
#include <QDebug>

NodeLabel::NodeLabel(const QString& text, Node* node)
	: QGraphicsTextItem(node)
	, m_node(node)
	, m_source(text)
{
	// A TEXT ITEM'S OWN MARGIN IS NOT THIS DIAGRAM'S PADDING.
	//
	// QGraphicsTextItem leaves 4 units of air on every side by default, for a
	// document laid out in a page. Here a label is a NAME, and for a node
	// that holds nothing the label IS the node - so that margin came out as a
	// wide empty border round a lone M, and squaring the box for a single
	// letter (Object::boxRect) multiplied it. One unit is enough to keep the
	// glyph off its own frame.
	document()->setDocumentMargin(1.0);
	renderSource();
	// ItemIsMovable is set by the node that wants it (an arrow does), once its
	// own constructor has run - labelIsMovable() cannot be asked from in here,
	// where the object is still only a Node.
	setFlag(QGraphicsItem::ItemSendsGeometryChanges, true);
}

NodeLabel::~NodeLabel()
{
	// The same window Node::~Node closes: a text item in a scene is painted
	// until ~QGraphicsItem removes it, and by then it is no longer a
	// QGraphicsTextItem. Out first, unmade afterwards.
	qDebug() << "NodeLabel::~NodeLabel this=" << this << "scene=" << static_cast<void*>(scene());
	// (Qt takes it out of the scene itself, in ~QGraphicsItem, with its own
	// inDestructor flag set - doing it by hand from here reparents and calls
	// back into a half-destroyed object instead.)
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

QRectF NodeLabel::boundingRect() const
{
	const QRectF base = QGraphicsTextItem::boundingRect();
	// room for the padlock, and only while it is out: a permanent margin here
	// would widen every node's frame for nothing
	return m_hinting ? base.adjusted(-2, -15, 15, 2) : base;
}

void NodeLabel::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
	QGraphicsTextItem::paint(painter, option, widget);
	if (m_hintLevel <= 0.01)
		return;

	painter->setRenderHint(QPainter::Antialiasing, true);
	const int alpha = int(230 * m_hintLevel);
	const QRectF text = QGraphicsTextItem::boundingRect();
	painter->setBrush(Qt::NoBrush);
	painter->setPen(QPen(QColor(180, 83, 9, alpha), 1.6));
	painter->drawRoundedRect(text.adjusted(-1.5, -1.5, 1.5, 1.5), 4, 4);

	painter->setPen(QColor(180, 83, 9, alpha));
	painter->setFont(Emoji::font(11));
	painter->drawText(QRectF(text.right() - 3, text.top() - 15, 17, 17),
	                  Qt::AlignCenter, QStringLiteral("\U0001F512"));
}

void NodeLabel::showLockedHint()
{
	if (m_hint == nullptr)
	{
		m_hint = new QVariantAnimation(this);
		m_hint->setDuration(700);
		m_hint->setLoopCount(2);
		m_hint->setKeyValueAt(0.0, 0.0);
		m_hint->setKeyValueAt(0.5, 1.0);
		m_hint->setKeyValueAt(1.0, 0.0);
		connect(m_hint, &QVariantAnimation::valueChanged, this, [this](const QVariant& value) {
			m_hintLevel = value.toReal();
			update();
		});
		connect(m_hint, &QVariantAnimation::finished, this, [this] {
			prepareGeometryChange();
			m_hinting = false;
			m_hintLevel = 0.0;
			update();
		});
	}
	if (!m_hinting)
	{
		prepareGeometryChange();
		m_hinting = true;
	}
	m_hint->stop();
	m_hint->start();
}

void NodeLabel::beginEdit()
{
	if (m_editing)
		return;
	if (m_node != nullptr && m_node->labelIsLocked())
	{
		// not ours to change: say so and leave it alone
		showLockedHint();
		return;
	}
	// NOTHING ELSE IS SELECTED WHILE A NAME IS BEING TYPED.
	//
	// Editing a label is about ONE node, and a selection left over from
	// before says otherwise: the dashed selection frames of everything else
	// stay drawn round the editor (and any keystroke meant for the text that
	// the scene takes as a shortcut would land on all of them). So the
	// selection is narrowed to the node whose name this is, which is also
	// what a click on the label would have done had the editor not opened.
	if (QGraphicsScene* board = scene(); board != nullptr)
	{
		board->clearSelection();
		if (m_node != nullptr)
			m_node->setSelected(true);
	}

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
	// the moment the editor closes is the moment \circ becomes the character
	// it stands for - not while typing, where it would rewrite the text under
	// the cursor mid-word
	m_source = Notation::autoCorrect(toPlainText());
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

void NodeLabel::abandonEdit()
{
	m_editing = false;
	disconnect(m_live);
	m_node = nullptr;   // whatever happens from here reports to nobody
	setTextInteractionFlags(Qt::NoTextInteraction);
	if (hasFocus())
		clearFocus();   // give the keyboard up NOW, while this is still whole
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
