#include "core/AppSettings.h"
#include "art/Node.h"
#include "tutor/Tutor.h"

#include <QSettings>

const char* AppSettings::SnapEnabled = "diagram/snapEnabled";
const char* AppSettings::SnapUnit = "diagram/snapUnit";
const char* AppSettings::ShowGrid = "diagram/showGrid";
const char* AppSettings::TutorEnabled = "tutor/enabled";
const char* AppSettings::DefaultCategory = "diagram/defaultCategory";
const char* AppSettings::FunctorNotation = "notation/functorApplication";
const char* AppSettings::CycleCheck = "diagram/cycleCheck";
const char* AppSettings::NameCheck = "diagram/nameCheck";
const char* AppSettings::FrameEmptyNodes = "diagram/frameEmptyNodes";
const char* AppSettings::Collision = "diagram/collision";
const char* AppSettings::CollisionEpsilon = "diagram/collisionEpsilon";
const char* AppSettings::ArrowLineWidth = "arrow/lineWidth";
const char* AppSettings::ArrowHeadLength = "arrow/headLength";
const char* AppSettings::ArrowHeadWidth = "arrow/headWidth";
const char* AppSettings::ArrowHeadLineWidth = "arrow/headLineWidth";
const char* AppSettings::ArrowHitWidth = "arrow/hitWidth";
const char* AppSettings::LabelPointSize = "label/pointSize";
const char* AppSettings::ComposeWithRing = "notation/composeWithRing";
const char* AppSettings::EnglishSelectionOnly = "english/selectionOnly";

namespace
{
	QSettings store()
	{
		return QSettings("DiagramDetective", "DiagramDetective");
	}
}

AppSettings::AppSettings()
{
}

AppSettings& AppSettings::instance()
{
	static AppSettings s;
	return s;
}

QVariant AppSettings::defaultValue(const char* key)
{
	const QString k(key);
	if (k == SnapEnabled) return true;
	if (k == SnapUnit) return 25.0;
	if (k == ShowGrid) return false;
	if (k == TutorEnabled) return true;
	if (k == DefaultCategory) return QStringLiteral("BigCat");
	if (k == FunctorNotation) return QStringLiteral("F(h)");
	if (k == CycleCheck) return true;
	if (k == NameCheck) return true;
	if (k == FrameEmptyNodes) return false;
	if (k == Collision) return true;
	if (k == CollisionEpsilon) return 1.0;
	if (k == ArrowLineWidth) return 2.0;
	if (k == ArrowHeadLength) return 15.0;
	if (k == ArrowHeadWidth) return 7.0;
	if (k == ArrowHeadLineWidth) return 2.0;
	if (k == ArrowHitWidth) return 20.0;
	if (k == LabelPointSize) return 11.0;
	if (k == ComposeWithRing) return true;
	if (k == EnglishSelectionOnly) return false;
	return QVariant();
}

QVariant AppSettings::value(const char* key) const
{
	return store().value(key, defaultValue(key));
}

void AppSettings::setValue(const char* key, const QVariant& v)
{
	store().setValue(key, v);
}

void AppSettings::apply()
{
	Node::setSnapEnabled(snapEnabled());
	Node::setSnapUnit(snapUnit());
	Node::setCollisionEnabled(collision());
	Node::setCollisionEpsilon(collisionEpsilon());
	Tutor::setEnabled(tutorEnabled());   // the same key: Tutor reads it directly too
	emit changed();
}
