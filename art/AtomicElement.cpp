#include "art/AtomicElement.h"
#include "art/Category.h"
#include "art/DiagramScene.h"
#include "core/props/CategoryProps.h"

#include <QMenu>

AtomicElement::AtomicElement(const QString& id, QGraphicsItem* parent)
	: Object(id, parent)
{
}

QString AtomicElement::contextTitle() const
{
	// what it is an element OF is the node it is drawn in, not the category
	// that node belongs to: x is an element of M, and M is the R-module
	auto* home = dynamic_cast<Node*>(parentItem());
	const QString name = id().isEmpty() ? QStringLiteral("an element") : id();
	if (home == nullptr || home->id().isEmpty())
		return QString("Element %1").arg(name);
	return QString("Element %1 of %2").arg(name, home->id());
}

void AtomicElement::populateActions(QMenu& menu)
{
	auto* diagram = dynamic_cast<DiagramScene*>(scene());
	Category* home = surroundingCategory();
	const QString name = id().isEmpty() ? QStringLiteral("this") : id();

	// Each of these takes a SECOND element, pointed at after the menu closes
	// (ElementOpTutor), so the entry is the start of the gesture rather than
	// the whole of it - hence the "..." on each.
	const auto entry = [&](const QString& text, const QString& why, DiagramScene::ElementOp op) {
		QAction* action = menu.addAction(text);
		action->setToolTip(why);
		if (diagram == nullptr)
		{
			action->setEnabled(false);
			return;
		}
		QObject::connect(action, &QAction::triggered, diagram, [diagram, this, op] {
			// queued: the menu is still closing, and this puts a tutor in
			// charge of the very scene the menu belongs to
			QMetaObject::invokeMethod(diagram, [diagram, this, op] {
				diagram->beginElementOp(const_cast<AtomicElement*>(this), op);
			}, Qt::QueuedConnection);
		});
	};

	// Adding and subtracting need somewhere for the answer to live: they make
	// a new element, and that only means something where the module really is
	// additive. Shown greyed rather than left out, so the reason is on the
	// face of it.
	const bool additive = home != nullptr && home->has(IsAdditive::Key());
	if (additive)
	{
		entry(QStringLiteral("+  %1 + …").arg(name),
		      QString("Add another element to %1: click the second one, and %1 + it is drawn "
		              "beside them. Picking %1 again gives %1 + %1.").arg(name),
		      DiagramScene::ElementOp::Plus);
		entry(QStringLiteral("−  %1 − …").arg(name),
		      QString("Take another element from %1: click the second one, and %1 - it is drawn "
		              "beside them.").arg(name),
		      DiagramScene::ElementOp::Minus);
	}
	else
	{
		QAction* why = menu.addAction(QStringLiteral("+ and −"));
		why->setEnabled(false);
		why->setToolTip(QString("Elements can only be added and subtracted where they live in "
		                        "something ADDITIVE, and %1 is not said to be.")
			.arg(home != nullptr ? home->id() : QStringLiteral("this category")));
	}

	// Equality needs nothing of the category: two elements are the same
	// element or they are not, whatever structure is about them.
	entry(QStringLiteral("=  %1 = …").arg(name),
	      QString("Say %1 is equal to another element: click it, and the two are joined by a "
	              "double line with no head - an equals, not a map.").arg(name),
	      DiagramScene::ElementOp::Equals);

	menu.addSeparator();

	// and what an element offers anyway: another one beside it
	Object::populateActions(menu);
}
