#pragma once

#include <QList>
#include <QStringList>
#include <QPointF>
#include <QPainterPath>
#include "Node.h"

class ArrowProp;
class QMenu;

// An arrow between two nodes, drawn from the edge of its domain to the edge
// of its codomain and following them as they move. Its label rides the
// midpoint. Its own pos() is irrelevant; the ends decide its geometry.
class Arrow  : public Node
{
	Q_OBJECT

public:
	Arrow(const QString& id, Node* domain, Node* codomain, QGraphicsItem *parent=nullptr);

	// The points the line is pulled through. Drag the line and one appears
	// where it was grabbed; the curve through them is a Catmull-Rom spline, so
	// it passes through every point rather than merely being pulled towards
	// it. Purely graphical: where an arrow is drawn says nothing about it.
	const QList<QPointF>& bends() const { return m_bends; }
	void setBends(const QList<QPointF>& bends);
	void addBend(const QPointF& at);
	void removeBend(int index);
	void straighten();
	// the bend under a point in this arrow's coordinates, or -1
	int bendAt(const QPointF& pos, qreal radius = 9.0) const;

	// the curve as drawn, in this arrow's coordinates
	QPainterPath curve() const;
	// the clipped start, the bends, and the clipped end
	QList<QPointF> throughPoints() const;
	// the frame an end is joined to; an arrow is joined at the middle of its line
	QRectF endFrame(Node* end) const;

	// Where the label sits if it has not been moved: beside the middle of the
	// line. What the user drags is an offset from THAT, so the label keeps its
	// place relative to the arrow as the arrow moves and bends.
	QPointF labelAnchor() const;
	QPointF labelOffset() const { return m_labelOffset; }
	void setLabelOffset(const QPointF& offset);

	// beside the middle of the line, plus wherever it has been dragged to -
	// NOT centred on the arrow's origin, which is the category's own corner
	void placeLabel() override;
	bool labelIsMovable() const override { return true; }
	void labelMoved(const QPointF& pos) override;
	void labelDragFinished(const QPointF& fromPos) override;
	// where the line meets an end, given where it is coming from: the nearest
	// spot on that frame, on the grid
	QPointF attachTo(const QRectF& frame, const QPointF& from) const;
	// one coordinate of a point of this arrow, put on the scene grid
	QPointF onGrid(const QPointF& point, bool snapX, bool snapY) const;

	// "R-linear map f", "Functor F"
	QString contextTitle() const override;

	// The name to READ this arrow by: its label, or - when the label is blank
	// - whatever the category says an unlabelled arrow is. In R-Mod that is
	// 0, so an unnamed arrow into the zero module reads as the zero map in
	// every equation without the label saying so.
	QString effectiveId() const;

	Node* domain() const { return m_domain; }
	Node* codomain() const { return m_codomain; }
	virtual void setDomain(Node* domain);
	virtual void setCodomain(Node* codomain);

	~Arrow();

	// What this arrow can do (props/ArrowProp.h): a functor maps the elements
	// of its domain into its codomain. Each adds its own context-menu section.
	const QList<ArrowProp*>& props() const { return m_props; }
	QStringList properties() const;
	void setProperties(const QStringList& keys);
	void addProperty(const QString& key);
	bool has(const QString& key) const;
	ArrowProp* prop(const QString& key) const;

	QRectF boundingRect() const override;
	QPainterPath shape() const override;
	void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

signals:
	void domainChanged(Node* domain);
	void codomainChanged(Node* codomain);
	// the points it is pulled through changed: an image of this arrow can
	// take the same shape
	void bendsChanged(Arrow* arrow);

private slots:
	// The end is going: forget it AT ONCE. deleteLater alone is not enough -
	// during teardown the posted event is never delivered, and ~Arrow would
	// disconnect from a freed object while segment() would ask it for its
	// bounding rect.
	void onObjectDeleted(Node* object);
	void onObjectMoved(Node*, const QPointF&) { refreshGeometry(); }

protected:
	// what this arrow does to what is drawn in its domain, and the shape of
	// its own line
	void populateActions(QMenu& menu) override;

	// dragging the line bends it; dragging a bend moves it
	void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
	void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
	void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;
	void hoverEnterEvent(QGraphicsSceneHoverEvent* event) override;
	void hoverLeaveEvent(QGraphicsSceneHoverEvent* event) override;

	void connectToObject(Node* object);
	void disconnectFromObject(Node* object);

	// the segment in our coordinates, from the domain's frame to the codomain's
	bool segment(QPointF& from, QPointF& to) const;
	// the ends moved or changed: re-index, re-place the label, repaint
	void refreshGeometry();

private:
	// put the change in the scene's history, as a purely graphical one
	void recordBends(const QString& what, const QList<QPointF>& before);
	// the index a new bend at this point should take among the others
	int bendIndexFor(const QPointF& pos) const;
	// is that point near enough to the line the arrow would take without it?
	bool isRedundantBend(const QPointF& point, const QList<QPointF>& without) const;

	Node* m_domain = nullptr;
	Node* m_codomain = nullptr;
	QList<ArrowProp*> m_props;

	QList<QPointF> m_bends;
	QPointF m_labelOffset;   // where the user put the label, from labelAnchor()
	QList<QPointF> m_bendsAtPress;
	QPointF m_pressPos;
	int m_dragBend = -1;
	bool m_pressed = false;
	bool m_hovered = false;
};
