#pragma once

#include <QPointer>
#include "core/props/ArrowProp.h"

class Category;
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

	// Keep the image in step from now on. Turning this on brings D up to date
	// at once and then follows every change in C.
	bool isLive() const { return m_live; }
	void setLive(bool live);

	// Moving an object moves its image BY THE SAME AMOUNT: domain -> codomain.
	// Deltas, not places - so an object and its image can each be arranged
	// where you want them, and the image still travels with the object.
	// This is what a drawn functor usually wants, so it is on.
	bool imaginesPosition() const { return m_imaginesPosition; }
	void setImaginesPosition(bool imagines);

	// And the other way: dragging an image moves what it is the image of,
	// codomain -> domain. Off, because it is the surprising direction.
	//
	// Both at once is a ring - each move would answer the other - so every
	// position written by this property is written with m_syncing held, and
	// nothing is written that is already where it should be.
	bool reflectsPosition() const { return m_reflectsPosition; }
	void setReflectsPosition(bool reflects);

	// The same for the shape of an arrow: how many points its line is pulled
	// through, and where they are. An image of a bent arrow can be bent the
	// same way - or left straight while its source curves.
	// Contravariant: the image arrows run the OTHER WAY. Hom(-,D) is like
	// this - a map f : X -> Y gives Hom(f,D) : Hom(Y,D) -> Hom(X,D) - while
	// Hom(A,-) is covariant and runs the same way. Which one a formula is
	// cannot be read off its name, so it is said here.
	bool isContravariant() const { return m_contravariant; }
	void setContravariant(bool contravariant);

	bool imaginesBends() const { return m_imaginesBends; }
	void setImaginesBends(bool imagines);
	bool reflectsBends() const { return m_reflectsBends; }
	void setReflectsBends(bool reflects);

	// The two ends. Reading what is drawn in the domain needs nothing of it -
	// any node holds its children - so the domain is a plain Node. Drawing the
	// image needs the codomain to be able to MAKE things, which is what a
	// Category does, so that one is asked for by kind.
	Node* domain() const;
	Category* codomain() const;

	// This mapping's identity - what its images are stamped with. Set when a
	// diagram is read back, because a Functor makes its mapping inside its own
	// constructor, before anything can stamp the arrow.
	QString mappingId() const { return m_mappingId; }
	void setMappingId(const QString& id);

	// Whether what this mapping drew is on show. Put away, the image nodes
	// are HIDDEN, not destroyed - whatever is drawn inside them goes with them
	// and comes back untouched. Double-clicking the arrow toggles it.
	bool imagesVisible() const { return m_imagesVisible; }
	void setImagesVisible(bool visible);

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
	Node* imageOf(Category* codomain, const QString& functor, const Node* source) const;
	void stamp(Node* image, const QString& functor, const Node* source) const;
	// is that node one of ours?
	bool isOurImage(const Node* node) const;

	bool m_live = true;
	bool m_imaginesPosition = true;
	bool m_reflectsPosition = false;
	bool m_imaginesBends = true;
	bool m_reflectsBends = false;
	bool m_contravariant = false;
	bool m_imagesVisible = true;
	bool m_syncing = false;
	QString m_name;        // what the functor was called when its images were drawn
	QString m_mappingId;   // this mapping's identity, kept with the arrow and in the file
};
