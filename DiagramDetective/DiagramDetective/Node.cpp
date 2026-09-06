#include "Node.h"
#include "Category.h"

#include <QGraphicsSceneContextMenuEvent>
#include <QMenu>
#include <QWidgetAction>
#include <QToolButton>
#include <QGridLayout>
#include <QColorDialog>
#include <QStyleOptionGraphicsItem>

qreal Node::s_snapUnit = 25.0;
bool Node::s_snapEnabled = true;

QPointF Node::snapped(const QPointF& parentPos) const
{
	if (!s_snapEnabled || s_snapUnit <= 0)
		return parentPos;
	// snap in scene coordinates: the parent's transform (its position,
	// nesting depth) must not shift the grid
	const QGraphicsItem* parent = parentItem();
	const QPointF scenePt = parent != nullptr ? parent->mapToScene(parentPos) : parentPos;
	const QPointF onGrid(qRound(scenePt.x() / s_snapUnit) * s_snapUnit,
	                     qRound(scenePt.y() / s_snapUnit) * s_snapUnit);
	return parent != nullptr ? parent->mapFromScene(onGrid) : onGrid;
}

Node::Node(const QString& id, QGraphicsItem *parent)
	: QGraphicsObject(parent)
{
	setFlag(QGraphicsItem::ItemSendsGeometryChanges, true);   // itemChange sees moves
	setId(id);
}

Node::~Node()
{
	if (m_idText != nullptr)
	{
		delete m_idText;
		m_idText = nullptr;
	}

	emit deleted(this);
}

void Node::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
	painter->setRenderHint(QPainter::RenderHint::Antialiasing, true);
}

void Node::setId(const QString& id) {
	if (m_idText != nullptr)
	{
		if (m_idText->toPlainText() != id)
		{
			prepareGeometryChange();
			ancestorsPrepareGeometryChange();
			m_idText->setPlainText(id);
			centreLabel();
			ancestorsUpdate();
			emit idChanged(this, id);
		}
	}
	else {
		if (id.isEmpty())
			return;
		prepareGeometryChange();
		ancestorsPrepareGeometryChange();
		m_idText = new QGraphicsTextItem(id, this);
		centreLabel();
		ancestorsUpdate();
		emit idChanged(this, id);
	}
}

void Node::setFill(const QBrush& fill)
{
	if (m_fill == fill) return;
	m_fill = fill;
	update();
	emit styleChanged(this);
}

void Node::setBorder(const QPen& border)
{
	if (m_border == border) return;
	prepareGeometryChange();   // a wider pen paints outside the old rect
	ancestorsPrepareGeometryChange();
	m_border = border;
	update();
	ancestorsUpdate();
	emit styleChanged(this);
}

int Node::containedCount(const QGraphicsItem* except) const
{
	int n = 0;
	for (QGraphicsItem* c : childItems())
		if (c != except && dynamic_cast<Node*>(c) != nullptr)   // nodes only: never our own label
			++n;
	return n;
}

void Node::refreshLabelWeight(int contained)
{
	QFont f = labelFont();
	const bool bold = contained > 0;
	if (f.bold() == bold)
		return;
	f.setBold(bold);
	setLabelFont(f);
}

QFont Node::labelFont() const
{
	return m_idText != nullptr ? m_idText->font() : QFont();
}

void Node::setLabelFont(const QFont& font)
{
	if (m_idText == nullptr || m_idText->font() == font)
		return;
	prepareGeometryChange();
	ancestorsPrepareGeometryChange();
	m_idText->setFont(font);
	centreLabel();
	update();
	ancestorsUpdate();
}

void Node::centreLabel()
{
	if (m_idText == nullptr)
		return;
	const QRectF r = m_idText->boundingRect();
	m_idText->setPos(-r.width() / 2.0, -r.height() / 2.0);
}

QVariant Node::itemChange(GraphicsItemChange change, const QVariant& value)
{
	switch (change)
	{
	case ItemPositionChange:
		// about to move: every ancestor's frame is about to change shape
		ancestorsPrepareGeometryChange();
		// the position Qt will apply is the one we return: put it on the grid
		return QGraphicsObject::itemChange(change, snapped(value.toPointF()));
	case ItemPositionHasChanged:
	{
		ancestorsUpdate();
		const QPointF p = value.toPointF();
		emit moved(this, p - m_lastPos);
		m_lastPos = p;
		break;
	}
	case ItemChildAddedChange:
	case ItemChildRemovedChange:
	{
		// our own frame follows our children
		prepareGeometryChange();
		ancestorsPrepareGeometryChange();
		// whether the child is already in / still in childItems() here depends on
		// Qt's ordering, so count without it and add it back by hand
		auto* child = qvariant_cast<QGraphicsItem*>(value);
		const bool counts = dynamic_cast<Node*>(child) != nullptr;
		refreshLabelWeight(containedCount(child) + (change == ItemChildAddedChange && counts ? 1 : 0));
		break;
	}
	default:
		break;
	}
	return QGraphicsObject::itemChange(change, value);
}

void Node::ancestorsPrepareGeometryChange()
{
	for (QGraphicsItem* p = parentItem(); p != nullptr; p = p->parentItem())
		if (auto* node = dynamic_cast<Node*>(p))
			node->prepareGeometryChange();
}

void Node::ancestorsUpdate()
{
	for (QGraphicsItem* p = parentItem(); p != nullptr; p = p->parentItem())
		p->update();
}

namespace
{
	// a colour chip: a small square button in that colour
	QToolButton* chip(const QColor& c, const QString& tip, QWidget* parent)
	{
		auto* b = new QToolButton(parent);
		b->setFixedSize(18, 18);
		b->setToolTip(tip);
		b->setCursor(Qt::PointingHandCursor);
		b->setStyleSheet(QString("QToolButton { background: %1; border: 1px solid rgba(0,0,0,90); border-radius: 3px; }"
		                         "QToolButton:hover { border: 2px solid white; }").arg(c.isValid() ? c.name(QColor::HexArgb) : "transparent"));
		return b;
	}

	const char* const kPalette[] = {
		"#ffffff", "#e5e7eb", "#9ca3af", "#4b5563", "#1f2937", "#000000",
		"#fee2e2", "#fecaca", "#ef4444", "#b91c1c", "#fff7ed", "#f97316",
		"#fef9c3", "#facc15", "#dcfce7", "#22c55e", "#15803d", "#ccfbf1",
		"#14b8a6", "#dbeafe", "#3b82f6", "#1d4ed8", "#ede9fe", "#8b5cf6",
	};
}

QMenu* Node::colourMenu(const QString& title, const QColor& current, QMenu* parent, std::function<void(const QColor&)> apply)
{
	auto* menu = parent->addMenu(title);
	auto* grid = new QWidget(menu);
	auto* layout = new QGridLayout(grid);
	layout->setContentsMargins(8, 6, 8, 6);
	layout->setSpacing(4);
	const int cols = 6;
	int i = 0;
	for (const char* hex : kPalette)
	{
		QColor c(hex);
		auto* b = chip(c, c.name(), grid);
		QObject::connect(b, &QToolButton::clicked, menu, [menu, apply, c] { apply(c); menu->close(); });
		layout->addWidget(b, i / cols, i % cols);
		++i;
	}
	auto* action = new QWidgetAction(menu);
	action->setDefaultWidget(grid);
	menu->addAction(action);
	menu->addSeparator();
	menu->addAction("None (transparent)", [apply] { apply(QColor()); });
	menu->addAction("Custom...", [apply, current] {
		const QColor c = QColorDialog::getColor(current.isValid() ? current : Qt::white, nullptr, "Pick a colour", QColorDialog::ShowAlphaChannel);
		if (c.isValid()) apply(c);
	});
	return menu;
}

Category* Node::category() const
{
	Category* C = nullptr;	
	C = dynamic_cast<Category*>(parentItem());

	if (C != nullptr)
	{
		return C;
	}

	if (parentItem() != nullptr)
	{
		Node* node = dynamic_cast<Node*>(parentItem());

		if (node != nullptr)
			return node->category();
	}
	return nullptr;
}

void Node::contextMenuEvent(QGraphicsSceneContextMenuEvent* event)
{
	popupContextMenu(event->screenPos());
	event->accept();
}

void Node::popupContextMenu(const QPoint& screenPos)
{
	QMenu menu;
	populateContextMenu(menu);
	menu.exec(screenPos);
}

void Node::populateContextMenu(QMenu& menu)
{
	menu.addAction(id().isEmpty() ? "node" : id())->setEnabled(false);
	menu.addSeparator();

	colourMenu("Fill", m_fill.style() == Qt::NoBrush ? QColor() : m_fill.color(), &menu, [this](const QColor& c) {
		setFill(c.isValid() ? QBrush(c) : QBrush(Qt::NoBrush));
	});
	colourMenu("Border", m_border.style() == Qt::NoPen ? QColor() : m_border.color(), &menu, [this](const QColor& c) {
		if (!c.isValid()) { setBorder(Qt::NoPen); return; }
		QPen p = m_border.style() == Qt::NoPen ? QPen(c, 1.5) : m_border;
		p.setColor(c);
		setBorder(p);
	});
}


Category* Node::surroundingCategory() const
{
	for (QGraphicsItem* p = parentItem(); p != nullptr; p = p->parentItem())
		if (auto* category = dynamic_cast<Category*>(p))
			return category;
	return nullptr;
}
