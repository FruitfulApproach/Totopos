#include "art/RModule.h"

#include "art/AtomicElement.h"
#include "art/Category.h"
#include "art/DiagramScene.h"
#include "core/Emoji.h"

#include <QInputDialog>
#include <QMenu>

RModule::RModule(const QString& id, QGraphicsItem* parent)
	: Object(id, parent)
{
}

void RModule::setRing(const QString& ring)
{
	const QString named = ring.trimmed();
	if (named.isEmpty() || named == m_ring)
		return;
	m_ring = named;
	// Nothing is drawn differently, but everything written about this module
	// now says a different ring: the title, the tooltips, and the scalars
	// offered on its elements.
	update();
}

bool RModule::scalarOnLeft() const
{
	// Mod-R is right modules and writes x.r; everything else that makes one
	// of these is left modules and writes r.x.
	Category* home = surroundingCategory();
	return home == nullptr || home->builtInName() != QStringLiteral("Mod-R");
}

QString RModule::contextTitle() const
{
	const QString name = id().isEmpty() ? QStringLiteral("a module") : id();
	// "R-module M" says everything when the ring is the R the category is
	// named for, which it is unless somebody has said otherwise.
	return QString("%1-module %2").arg(m_ring, name);
}

AtomicElement* RModule::existingZero() const
{
	for (QGraphicsItem* child : childItems())
		if (auto* element = dynamic_cast<AtomicElement*>(child); element != nullptr
		    && element->id() == QStringLiteral("0"))
			return element;
	return nullptr;
}

AtomicElement* RModule::zeroElement(const QPointF& scenePos)
{
	// ONE zero, however many times it is asked for. A module has exactly one,
	// and a second 0 drawn beside the first would be two names for it with
	// nothing saying they are the same - which is a diagram that lies.
	if (AtomicElement* already = existingZero())
		return already;
	return createElement(QStringLiteral("0"), scenePos);
}

AtomicElement* RModule::createScalarMultiple(const QString& scalar, AtomicElement* of)
{
	if (of == nullptr || of->parentItem() != this)
		return nullptr;
	const QString named = scalar.trimmed();
	if (named.isEmpty())
		return nullptr;

	// beside the element it is made from, and a little below, so it lands on
	// neither it nor whatever is under it
	const QPointF beside = of->pos() + QPointF(0, 40);
	AtomicElement* made = createElement(mapToScene(beside));
	if (made == nullptr)
		return nullptr;

	// Built out of the element rather than copied from it: rename x and r.x
	// follows, the same way x + y does.
	const QString dot = QStringLiteral(" · ");   // middle dot: the scalar action
	made->setDerivedLabel(scalarOnLeft() ? named + dot + QStringLiteral("%1")
	                                     : QStringLiteral("%1") + dot + named,
	                      { of });
	return made;
}

void RModule::populateActions(QMenu& menu)
{
	auto* diagram = diagramOf(this);
	const QString name = id().isEmpty() ? QStringLiteral("this module") : id();

	// THE ZERO, which is there whether or not it is drawn.
	//
	// Offered rather than always drawn: a module has a zero, but a diagram
	// that is not about the zero should not have one sitting in it. Greyed
	// once it is there, so asking twice does not make a second.
	if (diagram != nullptr)
	{
		QAction* zero = menu.addAction(QStringLiteral("0  Add the zero of %1").arg(name));
		if (existingZero() != nullptr)
		{
			zero->setEnabled(false);
			zero->setToolTip(QString("%1 already has its zero drawn. A module has exactly one.")
				.arg(name));
		}
		else
		{
			zero->setToolTip(QString("Draw 0, the zero of %1: the element with x + 0 = x for every "
			                         "x in %1. Every module has one whether or not it is drawn; this "
			                         "puts it where it can be pointed at.").arg(name));
			RModule* self = this;
			const QPointF at = mapToScene(contextPos());
			QObject::connect(zero, &QAction::triggered, diagram, [diagram, self, at] {
				// queued: the menu is still closing, and this puts a node in
				// the scene the menu belongs to
				QMetaObject::invokeMethod(diagram, [diagram, self, at] {
					if (AtomicElement* made = self->zeroElement(at))
						diagram->recordCreation(
							QString("Put the zero in %1").arg(self->id()), { made });
				}, Qt::QueuedConnection);
			});
		}
	}

	// WHAT RING, when it is not the R the category is named for. A Z-module
	// and an R-module are drawn in the same picture often enough that this is
	// worth asking, and the answer is only ever a name.
	if (diagram != nullptr)
	{
		QAction* over = menu.addAction(QString("Over the ring %1...").arg(m_ring));
		over->setToolTip(QString("Which ring %1 is a module over. R unless you say otherwise - the "
		                         "same R the category is named for.").arg(name));
		RModule* self = this;
		QObject::connect(over, &QAction::triggered, diagram, [diagram, self] {
			QMetaObject::invokeMethod(diagram, [diagram, self] {
				bool said = false;
				const QString ring = QInputDialog::getText(
					nullptr, QStringLiteral("Over which ring?"),
					QString("%1 is a module over:").arg(self->id()),
					QLineEdit::Normal, self->ring(), &said);
				if (said)
					self->setRing(ring);
			}, Qt::QueuedConnection);
		});
	}

	menu.addSeparator();

	// and everything an object of a concrete category offers: its elements
	Object::populateActions(menu);
}
