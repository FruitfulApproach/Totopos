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
	// the same, and put it in the scene's history - what the menu entry and
	// the Properties button both want, neither of them being able to reach
	// recordBends from outside
	void straightenRecorded();
	// the bend under a point in this arrow's coordinates, or -1
	int bendAt(const QPointF& pos, qreal radius = 9.0) const;

	// the curve as drawn, in this arrow's coordinates
	QPainterPath curve() const;

	// IS THIS PRESS THE ARROW'S, OR DOES IT BELONG TO WHAT IS UNDER IT?
	//
	// True on an existing bend point, and anywhere in the middle stretch of
	// the line. False near either end, where the line lies across the object
	// it runs into and the press is almost certainly aimed at that object
	// (see BendFreeEnds). The scene asks this too, so that what it writes
	// down as the thing being dragged is the same thing that gets the press.
	bool takesPressAt(const QPointF& itemPos) const;
	// the clipped start, the bends, and the clipped end
	QList<QPointF> throughPoints() const;
	// the frame an end is joined to; an arrow is joined at the middle of its line
	QRectF endFrame(Node* end) const;

	// Where the label sits if it has not been moved: beside the middle of the
	// line. What the user drags is an offset from THAT, so the label keeps its
	// place relative to the arrow as the arrow moves and bends.
	QPointF labelAnchor() const;
	void setLabelOffset(const QPointF& offset) override;

	// A LABEL THAT HAS BEEN DRAGGED TRAVELS WITH THE LINE, IN PROPORTION.
	//
	// The offset alone is a number of pixels, and pixels stop meaning the same
	// thing the moment the arrow does: a label put a comfortable distance from
	// the middle of a long arrow is stranded halfway across the diagram once a
	// tidy-up makes that arrow short, which is how "m" came to be sitting in
	// the empty top half of its category with its arrow down in the corner.
	//
	// So what is REMEMBERED is not the offset but where it puts the label in
	// the arrow's own frame: how far along the line, and how far off to the
	// side, each as a fraction of the line's length. The offset is worked out
	// again from those whenever the line moves, turns or changes length, so a
	// label keeps the place it was given however the arrow is redrawn.
	void rememberLabelPlacement();
	// the line as a frame to measure in: its middle, the way it points, and
	// how long it is. False when there is no line yet to measure.
	bool labelFrame(QPointF& along, QPointF& across, qreal& length) const;

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
	void removeProperty(const QString& key);
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

		// TWO LINES AND NO HEAD: these two are the same thing.
		//
		// Not a morphism at all, which is why it has no head to point with:
		// an equals joins two ELEMENTS that have turned out to be equal - x
		// and y, or x + 0 and x - and says only that. Drawn the way equality
		// is written, as a double line, and read in either direction.
		Equals,
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

	// Cancellable on the left / on the right (props/ArrowProps.h): whether this
	// is asserted, drawn with a split vee tail / a doubled head. Checked often
	// enough (paint, the context menu) to be worth their own bool, like
	// existsSuch() is on Node - the underlying fact still lives as a Prop, so
	// it saves, loads and lists itself the same way every other one does.
	// Whether this arrow carries a property of that kind, by TYPE rather than
	// by key: an Inclusion is a Monomorphism, so propOfType<Monomorphism>()
	// finds it. Asking by key would not - the keys differ - and every place
	// that wanted "is this monic?" would have to list the subclasses.
	template <typename P> P* propOfType() const
	{
		for (ArrowProp* p : m_props)
			if (auto* found = dynamic_cast<P*>(p))
				return found;
		return nullptr;
	}

	bool isMonic() const;
	bool isEpic() const;
	// the narrower claim: monic AND drawn as a hook
	bool isInclusion() const;
	void setInclusion(bool inclusion);
	void setInclusionRecorded(bool inclusion);
	// the plain setters: undo/redo call these, so putting a change back never
	// records a second one
	void setMonic(bool monic);
	void setEpic(bool epic);
	// set it AND put it in the scene's history, the way a user's click does
	void setMonicRecorded(bool monic);
	void setEpicRecorded(bool epic);

	// THE SHAFT AND THE HEAD ARE TWO SEPARATE QUESTIONS.
	//
	// An equals is a double line with no head, because it points nowhere; a
	// plain arrow is a single line with a head. Asking the two apart is what
	// lets the third combination exist: an implication (art/Implies.h) is a
	// double line WITH a head, which is how "implies" is written.
	virtual bool drawsDoubleLine() const { return m_style == Style::Equals; }
	virtual bool drawsHead() const { return m_style != Style::Equals; }
	// half the space between the two lines, in scene units before the arrow's
	// depth is taken off it
	virtual qreal doubleLineGap() const { return 1.8; }

	// WHERE THE MARKS GO, worked out once.
	//
	// The head, the second head of an epi, the vee or hook at the tail: all
	// of them are placed off the same handful of numbers, read off the curve
	// and scaled for how deep the arrow is drawn. Painting them and testing a
	// click against them must agree exactly, so both ask for them here rather
	// than each working them out again.
	struct Marks
	{
		qreal scale = 1.0;
		QPointF tip, dir, normal;       // the codomain end, and the frame there
		QPointF tail, tailDir;          // the domain end, pointing into the line
		qreal headLength = 0, headWidth = 0;
		qreal secondHeadBack = 0;       // how far back an epi's second head sits
	};
	Marks markGeometry(const QPainterPath& path) const;

	QRectF boundingRect() const override;
	QPainterPath shape() const override;

	// EVERYTHING THIS ARROW DRAWS, as one path: the line (both halves of a
	// doubled one), the head, and whatever mark its style or its properties
	// put at either end. Not the label, which is a child node of its own.
	//
	// What it is FOR is hit-testing. An arrow is its line and its marks, and
	// a click on the head or on the hook of an inclusion is a click on the
	// arrow - so shape() strokes this, not just the curve.
	QPainterPath figure() const;
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

	// How far along the line that point is, from 0 at the tail to 1 at the
	// head. Measured along the CURVE, so a bent arrow answers about the line
	// as drawn rather than the straight run between its ends.
	qreal fractionAlong(const QPointF& pos) const;

	// HOW MUCH OF EACH END IS NOT FOR BENDING.
	//
	// An arrow is drawn right up to the edge of the thing it leaves and the
	// thing it arrives at, and it is wide to the mouse (arrowHitWidth) so it
	// can be grabbed at all. Between them, the last stretch of line before an
	// object lies across the very place you reach for to pick that object up:
	// going for O, you catch the arrow a few pixels short of it, and the
	// press became a bend instead of a drag.
	//
	// So the outer third at each end belongs to whatever is under it. The
	// middle third bends - which is where a bend is wanted anyway, a line
	// being pulled out sideways from its middle - and an existing bend point
	// can still be grabbed wherever it happens to sit.
	static constexpr qreal BendFreeEnds = 1.0 / 3.0;
	// is that point near enough to the line the arrow would take without it?
	bool isRedundantBend(const QPointF& point, const QList<QPointF>& without) const;

	Node* m_domain = nullptr;
	Node* m_codomain = nullptr;
	QPointF m_looseEnd;            // scene coordinates; only read when m_hasLooseEnd
	bool m_hasLooseEnd = false;
	QList<ArrowProp*> m_props;
	Style m_style = Style::Plain;

	// where the label was put, in the arrow's own frame: along the line and
	// off to the side, as fractions of its length (see rememberLabelPlacement)
	qreal m_labelAlong = 0.0;
	qreal m_labelAcross = 0.0;
	bool m_labelPlacedByHand = false;

	QList<QPointF> m_bends;
	QList<QPointF> m_bendsAtPress;
	QPointF m_pressPos;
	int m_dragBend = -1;
	bool m_pressed = false;
	bool m_hovered = false;
};
