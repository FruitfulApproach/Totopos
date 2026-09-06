#pragma once

#include <QGraphicsObject>
#include <QPainter>
#include <QBrush>
#include <QPen>
class Category;   // forward: Category derives (via Object) from Node, so it cannot be included here

class QGraphicsSceneContextMenuEvent;
class QMenu;

class Node  : public QGraphicsObject
{
	Q_OBJECT

public:
	Node(const QString& id, QGraphicsItem *parent);

	virtual QString id() const { if (m_idText) return m_idText->toPlainText(); return QString(); }
	virtual void setId(const QString& id);

	// the font of the id label; changing it re-centres the label and reframes the node
	QFont labelFont() const;
	void setLabelFont(const QFont& font);

	// appearance: transparent and borderless by default; both settable from the
	// right-click colour chips
	const QBrush& fill() const { return m_fill; }
	const QPen& border() const { return m_border; }
	void setFill(const QBrush& fill);
	void setBorder(const QPen& border);

	// the category this node was filed in, if any (see setCategory)
	Category* category() const;
	void setCategory(Category* category) { m_category = category; }
	// the nearest Category among this item's ancestors — the category it is drawn inside
	Category* surroundingCategory() const;

	virtual ~Node();

	void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

	// the right-click menu, built by populateContextMenu, shown at a screen point
	void popupContextMenu(const QPoint& screenPos);

	// The hidden snap grid: node positions land on multiples of the unit,
	// measured in SCENE coordinates, so nesting makes no difference — an
	// object inside a category snaps to the same grid as the category itself.
	static qreal snapUnit() { return s_snapUnit; }
	static void setSnapUnit(qreal unit) { s_snapUnit = unit; }
	static bool snapEnabled() { return s_snapEnabled; }
	static void setSnapEnabled(bool on) { s_snapEnabled = on; }
	// a position (in this item's parent coordinates) moved onto the grid
	QPointF snapped(const QPointF& parentPos) const;

signals:
	void idChanged(Node* thisNode, const QString& id);
	void deleted(Node* thisNode);
	void moved(Node* thisNode, const QPointF& delta);
	void styleChanged(Node* thisNode);

protected:
	void contextMenuEvent(QGraphicsSceneContextMenuEvent* event) override;
	QVariant itemChange(GraphicsItemChange change, const QVariant& value) override;

	// what the right-click menu holds: the base adds the title and the colour
	// chips; subclasses add their own sections after calling it
	virtual void populateContextMenu(QMenu& menu);

	// the id label item (for subclasses that place it themselves, e.g. an arrow)
	QGraphicsTextItem* label() const { return m_idText; }

	// a submenu of colour chips: the built-in palette, None, and Custom...
	QMenu* colourMenu(const QString& title, const QColor& current, QMenu* parent, std::function<void(const QColor&)> apply);


	// A node's frame is the union of its children (see Object::boundingRect),
	// so a child moving or resizing changes every ancestor's rect: they must be
	// told BEFORE (to re-index) and AFTER (to repaint). Call these around any
	// change to this item's own geometry.
	void ancestorsPrepareGeometryChange();
	void ancestorsUpdate();

	// the nodes this node contains (its child items other than its own label)
	int containedCount(const QGraphicsItem* except = nullptr) const;
	// a node that contains others shows its id in bold
	void refreshLabelWeight(int contained);

private:
	// keep the label centred on the item's origin, so setPos() positions the node's CENTRE
	void centreLabel();

	QGraphicsTextItem* m_idText = nullptr;
	QBrush m_fill = Qt::NoBrush;
	QPen m_border = Qt::NoPen;
	QPointF m_lastPos;

	Category* m_category = nullptr;

	static qreal s_snapUnit;
	static bool s_snapEnabled;
};
