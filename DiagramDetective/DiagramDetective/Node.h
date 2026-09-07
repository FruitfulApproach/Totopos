#pragma once

#include <QGraphicsObject>
#include <QList>
#include <QSet>
#include <QPointer>
#include <QPainter>
#include "NodeLabel.h"
#include <QBrush>
#include <QPen>
class Category;   // forward: Category derives (via Object) from Node, so it cannot be included here
class DiagramScene;

class QGraphicsSceneContextMenuEvent;
class QMenu;

class Node  : public QGraphicsObject
{
	Q_OBJECT

public:
	Node(const QString& id, QGraphicsItem *parent);

	virtual QString id() const { if (m_idText) return m_idText->toPlainText(); return QString(); }
	virtual void setId(const QString& id);

	// put the keyboard in this node's label
	void editLabel();
	bool isEditingLabel() const { return m_idText != nullptr && m_idText->isEditing(); }
	// called by the label as it is typed in, and when the editor closes
	void labelBeingEdited(const QString& text);
	void finishLabelEdit(const QString& before, const QString& after);

	// A label made out of other labels: "%1 x %2" over the nodes it is built
	// from. Whenever one of THEM is relabelled - by hand, or by anything else -
	// this one follows, so a product is never left naming factors that have
	// been renamed.
	void setDerivedLabel(const QString& pattern, const QList<Node*>& sources);
	void clearDerivedLabel();

	// Read a label the user typed and tie it to the nodes it MENTIONS. Type
	// Hom(X,B) where X and B are drawn, and the label becomes a formula over
	// them: rename B to C and it says Hom(X,C) of its own accord. Whole names
	// only - a node called "o" is not found inside "Hom".
	void bindLabelReferences();
	QString labelPattern() const { return m_labelPattern; }
	QList<Node*> labelSources() const;

	// the font of the id label; changing it re-centres the label and reframes the node
	QFont labelFont() const;
	void setLabelFont(const QFont& font);

	// appearance: transparent and borderless by default; both settable from the
	// right-click colour chips
	const QBrush& fill() const { return m_fill; }
	const QPen& border() const { return m_border; }
	void setFill(const QBrush& fill);
	void setBorder(const QPen& border);

	// Was the fill or the border chosen by hand (the colour chips)? A frame
	// that was asked for is always drawn; one that is merely the default is
	// only drawn when there is something inside to frame.
	bool hasChosenStyle() const { return m_styleChosen; }

	// does this node hold any other node? (its label does not count)
	bool holdsAnything() const;
	// where its label is drawn, in this node's own coordinates
	QRectF labelRect() const;

	// Lit up for a moment - pointing at it from somewhere else, a list of
	// components say. Green wins over every colour the node has been given,
	// because it is not about the node's own appearance at all.
	bool isHighlighted() const { return m_highlighted; }
	void setHighlight(bool on);

	// Whether the connected component this node belongs to is asserted to
	// commute. Kept on the members rather than on the component: components
	// are worked out afresh from the arrows every time, and would have nowhere
	// to keep it. Every component commutes until told otherwise.
	bool commutesInComponent() const { return m_componentCommutes; }
	void setCommutesInComponent(bool commutes);

	// The same for exactness. A connected component is a diagram; a lone
	// object is not, which is why these do not belong to an object as such.
	bool rowsExactInComponent() const { return m_componentRowsExact; }
	void setRowsExactInComponent(bool exact) { m_componentRowsExact = exact; }
	bool columnsExactInComponent() const { return m_componentColumnsExact; }
	void setColumnsExactInComponent(bool exact) { m_componentColumnsExact = exact; }

	// Part of something the diagram cannot mean - a ring in a diagram that is
	// asserted to commute, or a name doing the work of two. Drawn bright red
	// until it is put right.
	bool hasError() const { return m_inCycle; }
	void setError(bool error);

	// How round the corners of the frame are drawn, in scene units. 0 is a
	// plain rectangle.
	qreal cornerRadius() const { return m_cornerRadius; }
	void setCornerRadius(qreal radius);

	// set Exists such AND put it in the scene's history (it changes what the
	// diagram claims, so it is a step, not a look)
	void setExistsSuchRecorded(bool existsSuch);
	static DiagramScene* diagramOf(const Node* node);

	// "Exists such": this part of the diagram is not given, it is what is
	// CLAIMED TO EXIST. Drawn dotted - dots around the border of an object,
	// a dotted line for an arrow - and read as the existential part of the
	// statement the diagram makes.
	bool existsSuch() const { return m_existsSuch; }
	void setExistsSuch(bool existsSuch);

	// Added while chasing: part of the hypotheses of the statement, not of
	// the setup it started from.
	bool isHypothesis() const { return m_hypothesis; }
	void setHypothesis(bool hypothesis) { m_hypothesis = hypothesis; }

	// the category this node was filed in, if any (see setCategory)
	Category* category() const;
	void setCategory(Category* category) { m_category = category; }
	// the nearest Category among this item's ancestors — the category it is drawn inside
	Category* surroundingCategory() const;

	virtual ~Node();

	void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

	// A node's frame is the union of what it holds; Object and Arrow give it
	// their own shape. The base needs one even so: Qt asks a node for its
	// bounding rect WHILE IT IS STILL BEING BUILT (setId puts a label in and
	// tells the scene the geometry changed, and the scene then asks this item
	// and its ancestors for their rects). At that moment the subclass does not
	// exist yet, so a pure virtual here is a crash.
	QRectF boundingRect() const override { return childFrame(); }

	// Our frame is the union of what we hold, so it changes whenever a child
	// arrives, leaves, moves or resizes. Call this once the change is COMPLETE
	// (never from itemChange - see the note there): it re-indexes this node
	// and its ancestors, repaints them, and refreshes the bold labels.
	void refreshFrame();

	// how many nodes this one sits inside (0 at the top)
	int nesting() const;

	// Where this node sits, as the indexes to walk down from the topmost node
	// (the ambient category), counting only the objects - arrows are not
	// stepped through, so a path keeps its meaning whatever arrows are drawn.
	// The path of an arrow is the path of the category it is drawn in.
	QList<int> pathFromRoot() const;
	// the node at that path below `root`, or nullptr
	static Node* fromPath(Node* root, const QList<int>& path);
	// Looks that follow the nesting: the deeper a node, the smaller its label
	// (and the fainter a category's fill). Applies to this node and everything
	// inside it, so it is right again after a move between parents.
	void refreshDepthAppearance();

	// the right-click menu, built by populateContextMenu, shown at a screen point
	void popupContextMenu(const QPoint& screenPos);

	// What this is, in words, at the head of its menu: "R-module M",
	// "R-linear map f", "Category C". The kind comes from the category it is
	// drawn in, which is what knows.
	virtual QString contextTitle() const;
	// "r-linear map" -> "R-linear map"
	static QString sentenceCase(const QString& text);

	// The hidden snap grid: node positions land on multiples of the unit,
	// measured in SCENE coordinates, so nesting makes no difference — an
	// object inside a category snaps to the same grid as the category itself.
	static qreal snapUnit() { return s_snapUnit; }
	static void setSnapUnit(qreal unit) { s_snapUnit = unit; }
	static bool snapEnabled() { return s_snapEnabled; }
	static void setSnapEnabled(bool on) { s_snapEnabled = on; }
	// a position (in this item's parent coordinates) moved onto the grid
	QPointF snapped(const QPointF& parentPos) const;

	// Nodes with the same parent do not sit on top of one another. A node
	// moved into its neighbours pushes them ALONG THE WAY IT IS GOING, far
	// enough to be clear and a little further, and they push theirs in turn.
	static bool collisionEnabled() { return s_collisionEnabled; }
	static void setCollisionEnabled(bool on) { s_collisionEnabled = on; }
	// Only a node the user is dragging shoves its neighbours. Everything else
	// that moves a node - opening a file, undo, a functor carrying an image
	// along - must leave the diagram where it was put.
	static void setUserDragging(bool dragging) { s_userDragging = dragging; }
	static bool userDragging() { return s_userDragging; }
	static qreal collisionEpsilon() { return s_pushEpsilon; }
	static void setCollisionEpsilon(qreal epsilon) { s_pushEpsilon = epsilon; }

signals:
	void idChanged(Node* thisNode, const QString& id);
	void deleted(Node* thisNode);
	void moved(Node* thisNode, const QPointF& delta);
	void styleChanged(Node* thisNode);

protected:
	void contextMenuEvent(QGraphicsSceneContextMenuEvent* event) override;
	QVariant itemChange(GraphicsItemChange change, const QVariant& value) override;

	// The right-click menu, in this order: what this is, what it can DO
	// (populateActions, which each kind of node fills in), how it looks, what
	// it asserts, and finally deleting it.
	virtual void populateContextMenu(QMenu& menu);
	// what this kind of node offers - constructions, mappings, the shape of a
	// line. Submenus, not a flat list.
	virtual void populateActions(QMenu& menu) { Q_UNUSED(menu); }

	// where the last right-click landed, in this item's coordinates: a menu
	// entry may be about that spot rather than about the whole item
	QPointF contextPos() const { return m_contextPos; }

	// the id label item (for subclasses that place it themselves, e.g. an arrow)
	QGraphicsTextItem* label() const { return m_idText; }
	NodeLabel* labelItem() const { return m_idText; }

public:
	// A label that can be picked up and put somewhere else. An object's label
	// belongs in the middle of it; an arrow's has to get out of the way of
	// whatever it crosses, so an arrow says yes to all three of these. The
	// label itself calls them, so they are public.
	virtual bool labelIsMovable() const { return false; }
	virtual void labelMoved(const QPointF& pos) { Q_UNUSED(pos); }
	virtual void labelDragFinished(const QPointF& fromPos) { Q_UNUSED(fromPos); }

protected:

	// recompute a derived label from the labels it is built out of
	void refreshDerivedLabel();

	// note a colour change in the scene's history (purely graphical)
	void recordStyleChange(const QBrush& fillBefore, const QPen& borderBefore);

	// Where this node's label goes. An object centres it; an arrow puts it
	// beside the middle of its line, wherever it has been dragged to. It is
	// asked for on every keystroke while the label is being typed, so it must
	// not be the object's rule applied to everything.
	virtual void placeLabel();

	// a submenu of colour chips: the built-in palette, None, and Custom...
	QMenu* colourMenu(const QString& title, const QColor& current, QMenu* parent, std::function<void(const QColor&)> apply);


	// A node's frame is the union of its children (see Object::boundingRect),
	// so a child moving or resizing changes every ancestor's rect: they must be
	// told BEFORE (to re-index) and AFTER (to repaint). Call these around any
	// change to this item's own geometry.
	void ancestorsPrepareGeometryChange();
	void ancestorsUpdate();

	// The union of our children, in OUR coordinates - what every frame here is
	// built from. Use this and NEVER Qt's childrenBoundingRect(): see the note
	// on the implementation for why that one can abort the process.
	QRectF childFrame() const;

	// what this node looks like at that nesting depth; subclasses extend it
	virtual void applyDepthAppearance(int depth);

	// shove the neighbours this node has just moved into
	void pushSiblings(const QPointF& delta);

	// the nodes this node contains (its child items other than its own label)
	int containedCount(const QGraphicsItem* except = nullptr) const;
	// a node that contains others shows its id in bold
	void refreshLabelWeight(int contained);

private:
	// keep the label centred on the item's origin, so setPos() positions the node's CENTRE
	void centreLabel();
	// refreshFrame() on the next turn of the event loop, at most once
	void scheduleFrameRefresh();

	NodeLabel* m_idText = nullptr;
	QString m_labelPattern;
	QList<QPointer<Node>> m_labelSources;
	bool m_settingDerivedLabel = false;
	QBrush m_fill = Qt::NoBrush;
	QPen m_border = Qt::NoPen;
	bool m_existsSuch = false;
	bool m_hypothesis = false;
	bool m_inCycle = false;
	bool m_highlighted = false;
	bool m_componentCommutes = true;
	bool m_componentRowsExact = false;
	bool m_componentColumnsExact = false;
	bool m_styleChosen = false;
	qreal m_cornerRadius = 5.0;
	QPointF m_lastPos;
	QPointF m_contextPos;
	bool m_frameRefreshQueued = false;

	Category* m_category = nullptr;

	static qreal s_snapUnit;
	static bool s_snapEnabled;

	static bool s_collisionEnabled;
	static bool s_userDragging;
	static qreal s_pushEpsilon;
	static int s_pushDepth;          // a shove that starts another: bounded, so a ring ends
	static QSet<Node*> s_pushed;     // already moved this round, so nothing is shoved twice
};
