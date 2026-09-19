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
const char* AppSettings::WarnOnLibraryRemove = "library/warnOnRemove";
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
const char* AppSettings::NodeCornerRadius = "node/cornerRadius";
const char* AppSettings::NodeFill = "node/fill";
const char* AppSettings::NodeBorder = "node/border";
const char* AppSettings::ArrowFill = "arrow/fill";
const char* AppSettings::ArrowBorder = "arrow/border";

namespace
{
	// WHERE THE SETTINGS LIVE, and where they USED to live.
	//
	// The program was called Diagram Detective and its settings were filed
	// under that name. Renaming the program does not move them: QSettings
	// keys off the two names below, so a rename alone would have left every
	// stored preference behind and started everyone back at the defaults with
	// no explanation. See migrateFromOldName.
	const char* kOrganisation = "Totopos";
	const char* kApplication = "Totopos";
	const char* kFormerOrganisation = "DiagramDetective";
	const char* kFormerApplication = "DiagramDetective";

	QSettings store()
	{
		return QSettings(kOrganisation, kApplication);
	}
}

void AppSettings::migrateFromOldName()
{
	// Once, and only into an empty store. Anything already saved under the
	// new name is what the user has said more recently and is never written
	// over; the old store is left exactly as it is, so an older build of the
	// program still opens with its settings intact.
	QSettings fresh(kOrganisation, kApplication);
	if (!fresh.allKeys().isEmpty())
		return;
	QSettings former(kFormerOrganisation, kFormerApplication);
	const QStringList keys = former.allKeys();
	if (keys.isEmpty())
		return;
	for (const QString& key : keys)
		fresh.setValue(key, former.value(key));
	fresh.sync();
}

AppSettings::AppSettings()
{
	// before anything is read: the singleton is built on the first ask, and
	// every ask goes through it
	migrateFromOldName();
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
	if (k == WarnOnLibraryRemove) return true;
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
	if (k == NodeCornerRadius) return 13.0;
	// A default QColor is INVALID, and that is the answer wanted: nothing has
	// been chosen, so whatever placed the node keeps its own look.
	if (k == NodeFill || k == NodeBorder || k == ArrowFill || k == ArrowBorder)
		return QColor();
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
