#pragma once

#include <QPointer>
#include "core/props/ArrowProp.h"

class Category;
class Object;
class Node;

// A functor F : C -> D maps what is drawn in C into D: an object X becomes
// F(X), an arrow h becomes F(h). Left LIVE, it keeps D in step with C by
// itself - draw something in C and its image appears, delete it and the image
// goes - so D always shows the image of C as it stands. The notation
// (F(h) or Fh) is a setting; so are the three below.
class MapsElements : public ArrowProp
{
	Q_OBJECT

public:
	// What an image node remembers about where it came from. The mapping's own
	// id is what ties them together: a NAME cannot, because renaming the
	// functor would orphan everything it had already drawn - the old images
	// would answer to a name nothing uses any more, so nothing would ever
	// rename them or clear them up, and the next sync would draw a second set
	// beside them.
	enum ItemDataKey
	{
		ImageFunctorKey = 1,   // the name it was drawn under (for reading, not for matching)
		ImageSourceKey  = 2,   // the KEY (Node::key) of the node it is the image of - never its label
		MappingIdKey    = 3,   // on an ARROW: the identity of its own mapping
		ImageMappingKey = 4,   // on an IMAGE: the mapping that drew it
		// ON THE WAY OUT. An image is taken away with deleteLater rather than
		// destroyed where it stands (see deleteAll), so for one turn of the
		// event loop it is still a child of the codomain. This says not to
		// look at it: without it the very next sync would find it, take it for
		// an image that is still wanted, and hand back a node that is about to
		// cease to exist.
		DoomedKey = 5,
	};

	explicit MapsElements(Arrow* arrow = nullptr);

	static QString Key() { return QStringLiteral("mapsElements"); }
	QString key() const override { return Key(); }
	QString label() const override { return QStringLiteral("Maps elements"); }
	QString description() const override { return QStringLiteral("Applying the functor to a label: X becomes F(X), h becomes F(h)."); }

	void arrowContextMenu(QMenu& menu, Arrow* arrow) override;

	// The functor applied to a label. A name with a HOLE in it is a formula
	// rather than a prefix: H(.,D) applied to A is H(A,D), not H(.,D)(A). The
	// dot is the point of application, and only the first is filled, so a
	// two-holed name like H(.,.) applied to A leaves H(A,.) - a bifunctor with
	// one side still open.
	static QString applied(const QString& functor, const QString& element);
	static QChar hole() { return QLatin1Char('.'); }

	// The formula a name stands for. EVERY name is a formula with a hole in
	// it: one written without a hole has it at the end, the way the notation
	// setting asks - F means F(.), or F. juxtaposed. So H(.,D) and F are the
	// same kind of thing, and applying either fills its hole.
	static QString formula(const QString& functor);

	// draw the image of the domain's diagram inside the codomain, once
	int mapDiagram();

	// ONE SWITCH FOR THE WHOLE MIRROR.
	//
	// A drawn mapping either keeps its two sides in step or it does not, and
	// in practice nobody wants half of that: an image that follows what it is
	// the image of but not the other way round, or that carries a bend across
	// but not the move that went with it, reads as a bug rather than as a
	// setting. So the eight things this used to ask separately - show the
	// image, stay live, carry object moves each way, carry bend points each
	// way, carry label placements each way - are one answer:
	//
	//   * the image is on show, and kept in step with the domain;
	//   * moving an object moves its image BY THE SAME AMOUNT, and dragging
	//     an image moves what it is the image of, by the same amount;
	//   * bending an arrow bends its image, either way round;
	//   * dragging a LABEL clear of its node moves the corresponding label on
	//     the other side by the same amount, either way round.
	//
	// Deltas throughout, never places, so each side keeps the arrangement it
	// was given and simply travels with the other. Both directions at once
	// would be a ring - each move answering the other - so every position
	// this property writes is written with m_syncing held, and nothing is
	// written that is already where it should be.
	//
	// On for a freshly placed mapping: this is what a drawn functor wants.
	// Contravariance is NOT part of it - which way the image arrows run is
	// what the mapping means, not how it is kept in step.
	bool mirrorsGeometry() const { return m_mirror; }
	void setMirrorsGeometry(bool mirror);

	// Contravariant: the image arrows run the OTHER WAY. Hom(-,D) is like
	// this - a map f : X -> Y gives Hom(f,D) : Hom(Y,D) -> Hom(X,D) - while
	// Hom(A,-) is covariant and runs the same way. Which one a formula is
	// cannot be read off its name, so it is said here.
	bool isContravariant() const { return m_contravariant; }
	void setContravariant(bool contravariant);

	// The two ends. Reading what is drawn in the domain needs nothing of it -
	// any node holds its children - so the domain is a plain Node. Drawing the
	// image needs the codomain to be able to MAKE things, which is what a
	// Category does, so that one is asked for by kind.
	Node* domain() const;
	// What the images are drawn into. An OBJECT, not a category: since an
	// object of a concrete category holds elements, a plain R-module is a
	// perfectly good place for the image of one to go.
	Object* codomain() const;

	// This mapping's identity - what its images are stamped with. Set when a
	// diagram is read back, because a Functor makes its mapping inside its own
	// constructor, before anything can stamp the arrow.
	QString mappingId() const { return m_mappingId; }
	void setMappingId(const QString& id);

	// Whether what this mapping drew is on show. Put away, the image nodes
	// are HIDDEN, not destroyed - whatever is drawn inside them goes with
	// them and comes back untouched. This is the mirror seen from the other
	// side: putting the image away IS switching the mirror off, which is what
	// double-clicking the arrow does.
	bool imagesVisible() const { return m_mirror; }

	// make the image match the domain, exactly, right now
	void sync();
	// everything this mapping drew, taken away again
	void removeImages();

signals:
	void settingsChanged();

private slots:
	// the functor itself was relabelled: its images are still ITS images, so
	// they take the new name with it rather than being left behind under the
	// old one
	void onFunctorRenamed();
	void onSourceBends(Arrow* source);
	void onImageBends(Arrow* image);
	void onSourceMoved(Node* source, const QPointF& delta);
	void onImageMoved(Node* image, const QPointF& delta);
	// a label dragged clear of its node, on one side or the other: the
	// corresponding label on the far side is dragged the same way
	void onSourceLabelMoved(Node* source, const QPointF& delta);
	void onImageLabelMoved(Node* image, const QPointF& delta);
	// what this node was the image of has gone, so the image goes too - and as
	// it goes it says so, which is what carries the deletion along a chain of
	// functors C -> D -> E without any of them knowing about the others
	void onSourceDeleted(Node* source);

private:
	void listen(bool on);
	// the image of that source in the codomain, by what it remembers, else by
	// its name (which is how an image read back from a file is found again)
	// The image of THAT NODE - not of anything that happens to share its name.
	// Keyed by Node::key(), because a diagram may draw two objects both called
	// X and a functor must carry both of them over, to two images.
	// the node in the domain that this key belongs to, or nullptr
	Node* sourceWithKey(const QString& key) const;
	Node* imageOf(Object* codomain, const QString& functor, const Node* source) const;
	void stamp(Node* image, const QString& functor, const Node* source) const;
	// is that node one of ours?
	bool isOurImage(const Node* node) const;

	bool m_mirror = true;   // the whole mirror: on show, live, and both ways
	bool m_contravariant = false;
	bool m_syncing = false;
	QString m_name;        // what the functor was called when its images were drawn
	QString m_mappingId;   // this mapping's identity, kept with the arrow and in the file
};
