#pragma once

#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsSceneContextMenuEvent>
#include <QPointer>
#include <QTimer>
#include "tutor/TutorSession.h"   // QPointer needs the complete type
#include <memory>
#include "art/Object.h"
#include "art/Category.h"
#include "art/Arrow.h"   // noteArrowStyle names Arrow::Style, so the type must be complete
#include "core/rules/Rule.h"

class ClassicalView;
class NodeHandles;
class SceneHistory;
class QGraphicsLineItem;
class QKeyEvent;

// The diagram's scene. A QGraphicsScene is a QObject, not a widget: it has no
// designer form; its window is whichever view shows it. Everything drawn lives
// inside the AMBIENT category, an item sitting at the centre of the scene.
class DiagramScene : public QGraphicsScene
{
	Q_OBJECT

public:
	explicit DiagramScene(QObject* parent = nullptr);
	~DiagramScene() override;

	Category* ambientCategory() const { return m_ambientCategory; }

	// Everything that has been done to this scene, in order. The purely
	// graphical changes are marked as such, so the history can also be read
	// as the steps that mean something.
	SceneHistory* history() const { return m_history; }
	// note that these nodes were just made (an object placed, an arrow drawn,
	// a construction built); the change must already have happened
	void recordCreation(const QString& description, const QList<Node*>& nodes);

	// The chase. Before it starts, whatever is drawn is the setup. Once it is
	// running, anything ADDED is forced into the hypotheses of the statement -
	// deleting things is not an assumption and adds nothing.
	// Before the chase the diagram is a LET: this is what we are given. The
	// chase begins when the user says so, and can be left again.
	bool isChasing() const { return m_chasing; }
	void startChase();
	void endChase() { setChasing(false); }
	void toggleChase();
	void setChasing(bool chasing);
	const QList<QPointer<Node>>& hypotheses() const { return m_hypotheses; }

	// ---------------------------------------------------------------- how it is written
	//
	// HOW THIS DIAGRAM IS WRITTEN DOWN, which is not what it says.
	//
	// Succinct is the notation this program is built on and the only one it
	// stores: one picture, solid for what is given and dashed for what there
	// then is. Classical is the same statement set out the way it is set out
	// on paper - the givens in one box, the conclusion in another, and a
	// double arrow between them carrying the rule's name.
	//
	// Switching is a way of LOOKING, never a way of editing: the diagram is
	// the same diagram either way, and what is saved, matched and reasoned
	// about is always the succinct one. See core/view/ClassicalView.h.
	enum class Notation
	{
		Succinct,    // one picture: dashed means "there exists"
		Classical,   // two pictures: givens ==> conclusion
	};
	Notation notation() const { return m_notation; }
	bool isClassical() const { return m_notation == Notation::Classical; }

	// THE COLOUR OF THE PAPER.
	//
	// The ambient category is not drawn as a box any more (see
	// Category::becameAmbient), so the wash of colour that used to stand for
	// "this is the canvas" has nowhere to go - and it belongs to the canvas
	// anyway rather than to a node standing on it. This is that colour, asked
	// for and set exactly the way a node's fill is, on the same page.
	//
	// An invalid colour means the default: whatever the view's palette says,
	// which is what a scene paints when nobody has chosen.
	QColor background() const { return m_background; }
	void setBackground(const QColor& colour);
	void setNotation(Notation notation);
	void toggleNotation();

	// WHERE THINGS WERE PUT IN THE CLASSICAL VIEW.
	//
	// Two boxes want a different arrangement from one picture, so the two
	// notations keep their layouts apart: the succinct positions are the
	// nodes' own, and these are the classical ones. Dragging things about in
	// one notation never disturbs the other, and switching back and forth
	// finds each arrangement as it was left. Keyed by side and by the identity
	// of the node a thing is a copy of (see ClassicalView), so they survive a
	// relabelling and a reopening of the file, and are saved with it.
	const QHash<QString, QPointF>& classicalPositions() const { return m_classicalPositions; }
	void setClassicalPositions(const QHash<QString, QPointF>& positions) { m_classicalPositions = positions; }
	// Read the positions back off the classical view, if it is up. Switching
	// notations does this by itself; saving has to ask, because the file is
	// written while the view is still standing.
	void syncClassicalPositions();

	// Whether this diagram is asserted to commute. With it on, the statement
	// reads "... such that the diagram commutes".
	bool commutes() const { return m_commutes; }
	void setCommutes(bool commutes);

	// What this diagram is put forward AS. The picture says the same thing
	// either way; what changes is what saying it amounts to. An axiom is
	// granted, a conjecture is not, and a theorem owes a proof.
	enum StatementKind
	{
		Unstated = 0,   // a drawing, not yet a claim
		Axiom,
		Definition,
		Theorem,
		Conjecture,
		Remark,
		Proof,
	};
	static QString kindName(StatementKind kind);
	static QStringList kindNames();

	StatementKind statementKind() const { return m_kind; }
	void setStatementKind(StatementKind kind);
	// what it is called, so it can be referred to: "Additive identity exists"
	QString statementName() const { return m_statementName; }
	void setStatementName(const QString& name);

	// What a kind of statement needs BESIDES the picture. A theorem is the
	// picture and nothing else; a proof has to say what it proves and how it
	// got there; a definition has to say what word it is defining.
	//
	// The file a proof proves, as a path relative to the library root. Only a
	// Proof has one.
	QString proves() const { return m_proves; }
	void setProves(const QString& path);
	// the proofs OF this statement, for a theorem or a conjecture
	QStringList provedBy() const { return m_provedBy; }
	void setProvedBy(const QStringList& paths);
	// the word a definition defines ("kernel", "short exact sequence")
	QString defines() const { return m_defines; }
	void setDefines(const QString& term);

	// One step of a proof: a rule from the library applied here, what its
	// variables stood for, and what that amounted to. A step with an empty
	// rulePath is a step of plain reasoning - a note - which is still a step.
	struct ProofStep
	{
		QString rulePath;
		QString ruleName;
		QString description;
		QStringList variables;
		QStringList values;
	};
	// the steps this diagram was arrived at by, read off the history: the rules
	// applied and the notes made, in order, leaving out anything undone
	QList<ProofStep> proofSteps() const;
	// a rule was applied here: the step, and the nodes it drew in
	void recordRuleApplication(const QString& description, const QList<Node*>& made,
	                           const QString& rulePath, const QString& ruleName,
	                           const QStringList& variables, const QStringList& values);

	// One connected piece of the diagram drawn in the ambient category: the
	// objects, the arrows between them, and where it sits. Two objects are in
	// the same piece when an arrow joins them, whichever way it points.
	struct Component
	{
		QList<Node*> objects;
		QList<Arrow*> arrows;
		QRectF bounds;      // in scene coordinates
		bool commutes = true;
		bool rowsExact = false;
		bool columnsExact = false;
		QString title;      // "M, N, 0"
	};
	QList<Component> components() const;

	// A directed cycle in a diagram that is asserted to commute: the whole ring
	// is drawn red and the reason is put in the status bar. Nothing is looked
	// for when the diagram does not claim to commute, or when the check is
	// switched off in the settings - a diagram that may or may not commute is
	// free to go round in circles.
	// A ring in a diagram asserted to commute, and a name doing the work of
	// two. Whatever is wrong is drawn red and said in the status bar.
	void checkDiagram();
	QString cycleError() const { return m_cycleError; }
	// a label used for two things that share a world; marks them and says so
	QString nameClash();
	// everything drawn here that carries a label, except one
	QList<Node*> labelledNodes(const Node* except = nullptr) const;

	// Every pair of paths with the same two ends, as the equations the diagram
	// asserts when it commutes. Written right to left, so A -f-> B -g-> C is
	// "g o f" (or "gf" when composeWithRing is off). There are no cycles in a
	// commuting diagram, so the paths are finite.
	QStringList commutingEquations(bool composeWithRing) const;

	// The diagram read as a sentence: for all the solid objects and arrows
	// (such that it commutes), there exist the dotted ones (such that it still
	// commutes).
	QString statementText() const;

	// a node's Exists such was switched: remember it and say so
	void noteExistsSuch(Node* node, bool before, bool after);
	// an arrow was told what kind of arrow it is
	void noteArrowStyle(Arrow* arrow, Arrow::Style before, Arrow::Style after);
	// the same for the red X that strikes a node off when the rule is applied
	void noteDeleteMark(Node* node, bool before, bool after);

	// ---------------------------------------------------------------- the clipboard
	//
	// A piece of a diagram can be carried off and put down again: in this
	// diagram, or in another one open in another tab, or in another copy of the
	// program. What travels is the fragment (io/SceneFile.h) and, beside it,
	// the English reading of it, so a selection pasted into a document arrives
	// as a sentence.

	// every node picked out, the ambient category never among them
	QList<Node*> selectedNodes() const;
	// take the selection off to the clipboard; false when there was nothing
	bool copySelection();
	bool cutSelection();
	// put the clipboard down here, or - with no point given - where the mouse
	// last was
	QList<Node*> paste(const QPointF& scenePos);
	QList<Node*> paste();
	// a copy of the selection, a little to one side of it
	QList<Node*> duplicateSelection();
	// a fragment carried in by a drag, put down at that point
	QList<Node*> dropFragment(const QByteArray& payload, const QPointF& scenePos);
	// is there anything on the clipboard for us?
	static bool clipboardHasFragment();

	// pick out exactly these
	void selectOnly(const QList<Node*>& nodes);
	// everything drawn in the diagram
	void selectEverything();
	// the category a thing dropped at this point belongs in
	Category* categoryForDrop(const QPointF& scenePos) const;
	// where the mouse last was, in scene coordinates
	QPointF lastScenePos() const { return m_lastScenePos; }

	// Carry the selected objects by that much, as one change. This is how a
	// node that holds things is moved: its label is its own to drag, so the
	// label is no longer the handle the node is carried by.
	bool nudgeSelection(const QPointF& delta);

	// take a node out of the diagram, with the arrows that end on it
	void deleteNode(Node* node);
	// the same for several at once, as ONE change in the history
	void deleteNodes(const QList<Node*>& nodes);

	// A step that says something without changing anything: the reasoning
	// between one construction and the next.
	void recordNote(const QString& text);

	// TIDY THE DIAGRAM UP, on a thread of its own.
	//
	// `kindId` names one of GraphLayouts::all(). The diagram is copied, the
	// thread works on the copy, and what comes back is applied here as one
	// step of the history - so a tidy-up can be undone like anything else.
	// Nothing is blocked while it runs; an answer about a diagram that has
	// changed underneath is dropped.
	// `fresh` is what has JUST been drawn in - the rest of the diagram is
	// taken as arranged, and is left where it is when it is already tidy.
	void layOut(const QString& kindId, const QList<Node*>& fresh = QList<Node*>());
	// tidy up once a rule has finished drawing itself in
	void layOutAfterRule(const QList<Node*>& made);

	// A rule from the library laid over the diagram. Every place its premise
	// is found - as a subdiagram, the rule's variables standing for whatever
	// is there - is lit up with an Apply button, and the rest is dimmed.
	// Applying draws the rule's conclusion in at that place.
	bool beginRule(const QString& path);
	void endRule();
	bool ruleActive() const { return m_rule != nullptr; }
	QString ruleName() const;
	int matchCount() const { return m_matches.size(); }
	void applyMatch(int index);

	// Light one place up and dim the rest, without laying a rule over the
	// diagram at all: what the Applicable Rules panel does when a row is
	// picked out. clearMatch() puts everything back.
	void showMatch(const QList<Node*>& matched);
	void clearMatch();
	void applyAllMatches();

	// A node was pressed: the drag that follows carries it, wherever on the
	// node the press landed. Escape or the right button abandons a move and
	// puts the node back.
	// What a press turned into. A press on a node is always a move now -
	// arrows are begun by double-clicking - so there is nothing else a drag
	// can become, save carrying a copy off (which the move branch handles).
	enum class Gesture { None, Move };
	// A scene point in the coordinates a node's pos() is measured in: its
	// parent's, or - for a node hanging off the scene itself - the scene's.
	static QPointF inParentOf(const Node* node, const QPointF& scenePos);
	void beginPress(Node* node, const QPointF& scenePos, Gesture gesture);
	bool isMoving() const { return m_gesture == Gesture::Move && !m_pressed.isNull(); }
	void cancelMove();   // put it back where it was

	// a node shoved out of the way by another: it moved too, so it belongs in
	// the same entry of the history as the drag that shoved it
	void notePushed(Node* node, const QPointF& before);
	// empty the diagram and forget its history (for opening a file)
	void clearDiagram();

	// The topmost item a click is really ABOUT. Our own overlays - the arrow
	// handle, the arrow preview line, the tutor's pointer and badges - float
	// above the diagram and must never be mistaken for what was clicked.
	QGraphicsItem* hitItem(const QPointF& scenePos) const;
	// the node at a scene point (a label counts for its node), or nullptr
	Node* nodeAt(const QPointF& scenePos) const;
	// the node a PRESS there is about: the same as nodeAt, except that an
	// arrow pressed near one of its ends stands aside for what is under it
	// (Arrow::takesPressAt)
	Node* nodeForPress(const QPointF& scenePos) const;
	// is the keyboard in a label right now?
	bool isEditingLabel() const;

	// begin adding or subtracting, from the element `from`
	// What can be done with an ELEMENT, once a second one has been pointed at:
	// x + y and x - y build a new element, x = y joins the two that are there.
	// Started from the element's own right-click menu (AtomicElement).
	enum class ElementOp { Plus, Minus, Equals };
	void beginElementOp(Node* from, ElementOp operation);
	// the node whose border is nearest this point, within `within`, or nullptr

	void beginArrow(Node* from);
	void cancelArrow();
	bool arrowPending() const { return !m_arrowFrom.isNull(); }
	// the dashed line that follows the cursor while an arrow is being drawn
	void setArrowPreviewSource(Node* from);
	void hideArrowPreview();
	// build the arrow from the pending source to this node; nullptr if refused
	Arrow* finishArrow(Node* to);
	// hide the little bar of buttons on the last node clicked
	void hideHandles();

	// one guided interaction at a time: a new one cancels the running one
	void beginSession(TutorSession* session);
	TutorSession* session() const { return m_session; }

public slots:
	// switch the ambient category by name (BigCat, Ab, R-Mod, ...); the
	// objects already placed move over to the new one
	void setAmbientCategory(const QString& name);

signals:
	void ambientCategoryChanged(Category* category);
	// something worth a line in the status bar
	void message(const QString& text);
	void chasingChanged(bool chasing);
	// the diagram is now being shown the other way round (see Notation)
	void notationChanged(bool classical);
	// the paper is a different colour now
	void backgroundChanged(const QColor& colour);
	void commutesChanged(bool commutes);
	void statementKindChanged(int kind, const QString& name);
	// something the diagram cannot mean; empty when it is put right
	void error(const QString& text);
	// what was just drawn / just taken out: a live functor listens to these
	void nodesAdded(const QList<Node*>& nodes);
	// a rule was laid over the diagram, or taken off (empty name), and how
	// many places it fits
	void ruleChanged(const QString& name, int matches);
	void nodesRemoved(const QList<Node*>& nodes);
	// the sentence the diagram now makes
	void statementChanged(const QString& statement);
	// Ctrl was held and the selection was dragged: carry it. A scene is not a
	// widget and a drag has to start from one, so the view does the carrying.
	void fragmentDragRequested(const QByteArray& payload);

protected:
	void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
	void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;
	void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
	void keyPressEvent(QKeyEvent* event) override;
	void mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event) override;
	// a right-click on the background is a right-click on the ambient category
	void contextMenuEvent(QGraphicsSceneContextMenuEvent* event) override;
	// the snap grid, when Settings asks for it
	void drawBackground(QPainter* painter, const QRectF& rect) override;

private:
	// the category a double-click at this item places into: the category
	// hit (its frame or label), the ambient one for the background, none
	// when some other node was hit
	Category* categoryAt(QGraphicsItem* item) const;

	// the arrow affordance: the triangle on the last node clicked, and the
	// line that follows the cursor while an arrow is being drawn
	void showHandles(Node* node, const QPointF& itemPos);

	QColor m_background;   // invalid: the view's own palette
	Category* m_ambientCategory = nullptr;
	QPointer<TutorSession> m_session;
	NodeHandles* m_handle = nullptr;
	QPointer<Node> m_arrowFrom;
	// The arrow being placed: a real Arrow with no codomain, running to the
	// cursor. QPointer because it hangs off its domain and goes with it.
	QPointer<Arrow> m_pending;
	SceneHistory* m_history = nullptr;
	// the classical view, while it is up; null the rest of the time
	std::unique_ptr<ClassicalView> m_classical;
	Notation m_notation = Notation::Succinct;
	QHash<QString, QPointF> m_classicalPositions;
	bool m_chasing = false;
	bool m_commutes = false;
	QString m_cycleError;
	StatementKind m_kind = Unstated;
	QString m_statementName;
	QString m_proves;        // a proof: the statement it proves
	QStringList m_provedBy;  // a theorem or conjecture: its proofs
	QString m_defines;       // a definition: the word being defined
	QList<QPointer<Node>> m_hypotheses;

	// the rule being applied, where it fits, and the buttons at those places
	void refreshRuleOverlay();
	void clearRuleOverlay();
	std::unique_ptr<Rule> m_rule;
	QList<RuleMatch> m_matches;

	QPointer<Node> m_pressed;      // the object under the button
	QPointF m_pressScenePos;
	Gesture m_gesture = Gesture::None;
	bool m_moved = false;          // has the move actually gone anywhere yet
	QPointF m_moveFrom;            // where it stood, for Escape and for the history
	QPointF m_moveGrab;            // where in the object the mouse took hold
	// where the items being dragged were when the drag started
	QList<QPair<QPointer<Node>, QPointF>> m_dragFrom;
	QPointF m_lastScenePos;        // where the mouse was last seen: where a paste lands
	bool m_dragCarriedOff = false; // this press turned into carrying a copy away
};
