#pragma once

#include <QList>
#include <QStringList>
#include <QPointF>
#include <QPainterPath>
#include "art/Node.h"

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
	void setLabelOffset(const QPointF& offset) override;

	// beside the middle of the line, plus wherever it has been dragged to -
	// NOT centred on the arrow's origin, which is the category's own corner
	void placeLabel() override;
	bool labelIsMovable() const override { return true; }
	// an arrow's label rides its line, not the edge of a frame
	bool labelFollowsBox() const override { return false; }
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

	// WHILE IT IS BEING PLACED an arrow has no codomain yet: it runs to the
	// cursor, and its head follows the mouse until the user says what it
	// points at. Nothing stands in for it - this is the arrow itself, drawn
	// the way it will be drawn, with one end not yet tied down.
	//
	// The point is in SCENE coordinates, because that is what the mouse is
	// given in and the arrow may be parented anywhere (or nowhere).
	// Take this arrow out of the mouse's way entirely, label and all: the one
	// being placed lies under the cursor for as long as it exists.
	void setInert();
	void setLooseEnd(const QPointF& scenePos);
	void clearLooseEnd();
	bool hasLooseEnd() const { return m_hasLooseEnd; }
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

	// How this arrow is drawn, and so what it is CLAIMED to be. The marks are
	// the usual ones and are read the usual way: a hooked tail for an
	// inclusion, a barbed tail for a monomorphism, a second head for an
	// epimorphism, a tilde over the line for an isomorphism.
	//
	// Not to be confused with Node::hasChosenStyle(), which is about colour.
	enum class Style
	{
		Plain,       // a plain arrow: nothing claimed beyond its being an arrow
		Inclusion,   // hooked tail, as in a subobject taken into the whole
		Mono,        // barbed tail: monic, left-cancellable
		Epi,         // two heads: epic, right-cancellable
		Iso,         // a tilde over the line: invertible
	};

	Style style() const { return m_style; }
	void setStyle(Style style);
	// the same, and put it in the scene's history: what an arrow is claimed to
	// be is a step, not a look
	void setStyleRecorded(Style style);

	// "Monomorphism", and the sentence under it in the menu
	static QString styleName(Style style);
	static QString styleDescription(Style style);
	// every style, in the order they are offered
	static QList<Style> allStyles();

	// the line, the head and the bend handles - not the label beside it (see Node::boxRect)
	QRectF boxRect() const override;
	QRectF boundingRect() const override;
	QPainterPath shape() const override;
	void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

signals:
	void styleChanged(Arrow* arrow);
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
	QPointF m_looseEnd;            // scene coordinates; only read when m_hasLooseEnd
	bool m_hasLooseEnd = false;
	QList<ArrowProp*> m_props;
	Style m_style = Style::Plain;

	QList<QPointF> m_bends;
	QList<QPointF> m_bendsAtPress;
	QPointF m_pressPos;
	int m_dragBend = -1;
	bool m_pressed = false;
	bool m_hovered = false;
};
