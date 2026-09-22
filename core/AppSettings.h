#pragma once

#include <QObject>
#include <QString>
#include <QColor>
#include <QVariant>

// The application settings: one store (QSettings) behind Tools > Settings.
// apply() pushes the stored values into the objects that act on them (the
// snap grid, the tutor); changed() lets live UI follow.
class AppSettings : public QObject
{
	Q_OBJECT

public:
	static AppSettings& instance();

	// Carry the settings over from the name the program used to go by, once,
	// and only when nothing has been saved under the new one. Called for you
	// when the singleton is built; main() calls it first thing as well, so
	// that anything reading QSettings directly - the tutor does - finds them
	// already moved.
	static void migrateFromOldName();

	// keys
	static const char* SnapEnabled;      // bool, true
	static const char* SnapUnit;         // double, 25.0
	static const char* ShowGrid;         // bool, false
	static const char* TutorEnabled;     // bool, true
	static const char* DefaultCategory;  // string, "BigCat"
	static const char* FunctorNotation;  // string, "F(h)" or "Fh"
	static const char* CycleCheck;       // bool, true
	static const char* NameCheck;        // bool, true
	static const char* FrameEmptyNodes;  // bool, false
	static const char* WarnOnLibraryRemove; // bool, true
	static const char* Collision;        // bool, true
	static const char* CollisionEpsilon; // double, 1.0
	static const char* ArrowLineWidth;   // double, 2.0
	static const char* ArrowHeadLength;  // double, 15.0
	static const char* ArrowHeadWidth;   // double, 7.0 (how far the head spreads either side)
	static const char* ArrowHeadLineWidth; // double, 2.0
	static const char* ArrowHitWidth;    // double, 20.0
	static const char* LabelPointSize;   // double, 11.0 (0 or less: follow the application font)
	static const char* ComposeWithRing;  // bool, true: g o f rather than gf
	static const char* EnglishSelectionOnly; // bool, false: the English panel reads the whole diagram
	static const char* NodeCornerRadius; // double, 13.0: how round a fresh node's corners are
	static const char* BendLingerMs;     // int, 2000: how long the bend points stay up after a hover

	// WHAT THE NEXT ONE PLACED IS DRAWN IN.
	//
	// Set from the Properties page, by the "Set default" beside the colour
	// chips: colour one node the way you want them, press it, and everything
	// placed from then on starts that way. An INVALID colour means nothing
	// has been chosen and the built-in look stands - which is not the same as
	// a chosen "none", so both can be said.
	static const char* NodeFill;         // colour, none chosen
	static const char* NodeBorder;       // colour, none chosen
	static const char* ArrowFill;        // colour, none chosen
	static const char* ArrowBorder;      // colour, none chosen
	static const char* NodeText;         // colour the NAME is written in, none chosen
	static const char* ArrowText;        // the same for an arrow's name
	static const char* Background;       // the paper a NEW diagram starts on

	// THE BADGE TAGS: the little stamps a node wears saying what it is put
	// forward as - AXIOM, DEFINITION, THEOREM, CONJECTURE. All four are one
	// colour: which KIND it is is written on the badge in words, and four
	// colours saying the same thing again only made the page noisier. Both
	// the ground and the lettering are set here.
	static const char* BadgeFill;        // colour, dodger blue
	static const char* BadgeText;        // colour, white

	// WHERE THE WINDOW WAS AND HOW IT WAS ARRANGED.
	//
	// Not a preference anybody sets in a dialog, but a setting all the same:
	// it is how the program looked when it was last used, and opening it
	// back in the middle of the screen with every dock reset is the program
	// forgetting something the user arranged on purpose. Qt writes both of
	// these itself (QWidget::saveGeometry, QMainWindow::saveState) and reads
	// them back the same way; they are opaque here.
	static const char* WindowGeometry;   // QByteArray
	static const char* WindowState;      // QByteArray: the docks and toolbars
	// the folder the last diagram was opened from or saved to, so the next
	// file dialog starts where the work is
	static const char* LastFolder;       // QString

	QVariant value(const char* key) const;
	// "category/R-Mod/fill" and its like: one key per built-in, made here so
	// the spelling lives in one place
	static QString categoryKey(const QString& builtIn, const char* what);
	void setValue(const char* key, const QVariant& value);
	static QVariant defaultValue(const char* key);

	// convenience
	bool snapEnabled() const { return value(SnapEnabled).toBool(); }
	double snapUnit() const { return value(SnapUnit).toDouble(); }
	bool showGrid() const { return value(ShowGrid).toBool(); }
	// draw a frame around a node even when it holds nothing
	bool frameEmptyNodes() const { return value(FrameEmptyNodes).toBool(); }
	// ask before taking a file out of the library. Turned off by the asking
	// itself, from the box that asks.
	bool warnOnLibraryRemove() const { return value(WarnOnLibraryRemove).toBool(); }
	bool collision() const { return value(Collision).toBool(); }
	double collisionEpsilon() const { return value(CollisionEpsilon).toDouble(); }
	bool tutorEnabled() const { return value(TutorEnabled).toBool(); }
	QString defaultCategory() const { return value(DefaultCategory).toString(); }
	// F(h) rather than Fh
	bool functorParentheses() const { return value(FunctorNotation).toString().contains(QLatin1Char('(')); }
	// look for cycles in a diagram that is asserted to commute
	bool cycleCheck() const { return value(CycleCheck).toBool(); }
	bool nameCheck() const { return value(NameCheck).toBool(); }

	// how an arrow is drawn, and how easy it is to hit
	double arrowLineWidth() const { return value(ArrowLineWidth).toDouble(); }
	double arrowHeadLength() const { return value(ArrowHeadLength).toDouble(); }
	double arrowHeadWidth() const { return value(ArrowHeadWidth).toDouble(); }
	double arrowHeadLineWidth() const { return value(ArrowHeadLineWidth).toDouble(); }
	double arrowHitWidth() const { return value(ArrowHitWidth).toDouble(); }
	// the size labels are drawn at, before the nesting takes its share
	double labelPointSize() const { return value(LabelPointSize).toDouble(); }
	// write a composite as g o f; off, as gf
	bool composeWithRing() const { return value(ComposeWithRing).toBool(); }
	// the English panel reads only what is selected
	bool englishSelectionOnly() const { return value(EnglishSelectionOnly).toBool(); }
	// how round the corners of a freshly placed node are
	double nodeCornerRadius() const { return value(NodeCornerRadius).toDouble(); }
	// how long the points a curve is pulled through stay up once the mouse
	// has brought them out; 0 takes them away the moment it leaves
	int bendLingerMs() const { return value(BendLingerMs).toInt(); }

	// The colours a freshly placed node or arrow starts in. An invalid colour
	// is "nothing chosen": the caller keeps its own built-in look.
	QColor defaultFill(bool arrow) const
	{ return value(arrow ? ArrowFill : NodeFill).value<QColor>(); }
	QColor defaultBorder(bool arrow) const
	{ return value(arrow ? ArrowBorder : NodeBorder).value<QColor>(); }
	// The colour a freshly placed one writes its NAME in. Invalid: nothing
	// chosen, so the label is written in the ink names are written in.
	QColor defaultText(bool arrow) const
	{ return value(arrow ? ArrowText : NodeText).value<QColor>(); }
	// The paper a new diagram is drawn on. Unlike the six above this one has
	// a real colour behind it rather than "nothing chosen": there is no such
	// thing as a sheet with no colour, so a diagram always has one.
	QColor defaultBackground() const { return value(Background).value<QColor>(); }

	// what a badge tag is stamped in, and what it is lettered in
	QColor badgeFill() const { return value(BadgeFill).value<QColor>(); }
	QColor badgeText() const { return value(BadgeText).value<QColor>(); }

	// the window as it was left, and where files were last kept
	QByteArray windowGeometry() const { return value(WindowGeometry).toByteArray(); }
	QByteArray windowState() const { return value(WindowState).toByteArray(); }
	void rememberWindow(const QByteArray& geometry, const QByteArray& state)
	{
		setValue(WindowGeometry, geometry);
		setValue(WindowState, state);
	}
	QString lastFolder() const { return value(LastFolder).toString(); }
	void setLastFolder(const QString& folder) { setValue(LastFolder, folder); }
	void setDefaultBackground(const QColor& paper) { setValue(Background, paper); }
	// A BUILT-IN CATEGORY'S COLOUR IS THE BUILT-IN'S, NOT ONE NODE'S.
	//
	// Every R-Mod drawn anywhere is the same category, so they are all drawn
	// the same: the colour belongs to the NAME, and is kept here under it
	// rather than on any one node. Change it on one and every R-Mod in every
	// open diagram follows, which is why the colour chips on a built-in are
	// not a per-node choice at all.
	//
	// Invalid means the built-in's own look stands.
	QColor categoryFill(const QString& builtIn) const
	{ return categoryColour(builtIn, "fill"); }
	QColor categoryBorder(const QString& builtIn) const
	{ return categoryColour(builtIn, "border"); }
	void setCategoryLook(const QString& builtIn, const QColor& fill, const QColor& border);

	// Remember these as what the next one placed should look like. An invalid
	// colour is stored as such and means "none": a node with no fill at all.
	void setDefaultLook(bool arrow, const QColor& fill, const QColor& border)
	{
		setValue(arrow ? ArrowFill : NodeFill, fill);
		setValue(arrow ? ArrowBorder : NodeBorder, border);
	}
	void setDefaultText(bool arrow, const QColor& text)
	{
		setValue(arrow ? ArrowText : NodeText, text);
	}

private:
	QColor categoryColour(const QString& builtIn, const char* what) const;

public:

	// push every stored value into the objects that act on them, then announce
	void apply();

signals:
	void changed();

private:
	AppSettings();
};
