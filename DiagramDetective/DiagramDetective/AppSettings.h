#pragma once

#include <QObject>
#include <QString>
#include <QVariant>

// The application settings: one store (QSettings) behind Tools > Settings.
// apply() pushes the stored values into the objects that act on them (the
// snap grid, the tutor); changed() lets live UI follow.
class AppSettings : public QObject
{
	Q_OBJECT

public:
	static AppSettings& instance();

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
	static const char* Collision;        // bool, true
	static const char* CollisionEpsilon; // double, 1.0
	static const char* ArrowLineWidth;   // double, 2.0
	static const char* ArrowHeadLength;  // double, 15.0
	static const char* ArrowHeadWidth;   // double, 7.0 (how far the head spreads either side)
	static const char* ArrowHeadLineWidth; // double, 2.0
	static const char* ArrowHitWidth;    // double, 20.0
	static const char* LabelPointSize;   // double, 11.0 (0 or less: follow the application font)
	static const char* ComposeWithRing;  // bool, true: g o f rather than gf

	QVariant value(const char* key) const;
	void setValue(const char* key, const QVariant& value);
	static QVariant defaultValue(const char* key);

	// convenience
	bool snapEnabled() const { return value(SnapEnabled).toBool(); }
	double snapUnit() const { return value(SnapUnit).toDouble(); }
	bool showGrid() const { return value(ShowGrid).toBool(); }
	// draw a frame around a node even when it holds nothing
	bool frameEmptyNodes() const { return value(FrameEmptyNodes).toBool(); }
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

	// push every stored value into the objects that act on them, then announce
	void apply();

signals:
	void changed();

private:
	AppSettings();
};
