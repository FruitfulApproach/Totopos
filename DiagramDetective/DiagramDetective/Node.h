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

signals:
	void idChanged(Node* thisNode, const QString& id);
	void deleted(Node* thisNode);
	void moved(Node* thisNode, const QPointF& delta);
	void styleChanged(Node* thisNode);

protected:
	void contextMenuEvent(QGraphicsSceneContextMenuEvent* event) override;
	QVariant itemChange(GraphicsItemChange change, const QVariant& value) override;

	// a submenu of colour chips: the built-in palette, None, and Custom...
	QMenu* colourMenu(const QString& title, const QColor& current, QMenu* parent, std::function<void(const QColor&)> apply);


private:
	// keep the label centred on the item's origin, so setPos() positions the node's CENTRE
	void centreLabel();

	QGraphicsTextItem* m_idText = nullptr;
	QBrush m_fill = Qt::NoBrush;
	QPen m_border = Qt::NoPen;
	QPointF m_lastPos;

	Category* m_category = nullptr;
};
