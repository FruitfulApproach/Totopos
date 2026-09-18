#include "core/history/Mementos.h"

#include <QGraphicsScene>
#include <QDataStream>
#include <QIODevice>
<<<<<<< HEAD:DiagramDetective/DiagramDetective/core/history/Mementos.cpp
#include "art/Arrow.h"
#include "art/Category.h"
#include "art/DiagramScene.h"
#include "art/Functor.h"
=======
#include "../Arrow.h"
#include "../Category.h"
#include "../DiagramScene.h"
#include "../Functor.h"
#include "../NodeKind.h"
>>>>>>> 3e9da9ce39d6dc74c5a0385266cfd9f7c2eeaba9:DiagramDetective/DiagramDetective/history/Mementos.cpp

// ---------------------------------------------------------------- purely graphical

void ItemsMoved::undo()
{
	for (const Move& m : m_moves)
		if (!m.node.isNull())
			m.node->setPos(m.before);
}

void ItemsMoved::redo()
{
	for (const Move& m : m_moves)
		if (!m.node.isNull())
			m.node->setPos(m.after);
}

void StyleChanged::undo()
{
	if (m_node.isNull())
		return;
	m_node->setFill(m_fillBefore);
	m_node->setBorder(m_borderBefore);
	if (m_radiusBefore >= 0)
		m_node->setCornerRadius(m_radiusBefore);
}

void StyleChanged::redo()
{
	if (m_node.isNull())
		return;
	m_node->setFill(m_fillAfter);
	m_node->setBorder(m_borderAfter);
	if (m_radiusAfter >= 0)
		m_node->setCornerRadius(m_radiusAfter);
}

void ArrowBent::undo()
{
	if (auto* arrow = dynamic_cast<Arrow*>(m_arrow.data()))
		arrow->setBends(m_before);
}

void ArrowBent::redo()
{
	if (auto* arrow = dynamic_cast<Arrow*>(m_arrow.data()))
		arrow->setBends(m_after);
}

// ---------------------------------------------------------------- the diagram itself

namespace
{
	bool isInside(const QGraphicsItem* item, const QGraphicsItem* ancestor)
	{
		for (const QGraphicsItem* p = item; p != nullptr; p = p->parentItem())
			if (p == ancestor)
				return true;
		return false;
	}
}

QList<Node*> withAttachedArrows(const QList<Node*>& nodes)
{
	QList<Node*> all;
	QList<Node*> arrows;
	for (Node* n : nodes)
		if (n != nullptr && !all.contains(n))
			(dynamic_cast<Arrow*>(n) != nullptr ? arrows : all).append(n);

	// an arrow whose end is one of these nodes, or sits inside one, cannot
	// stay behind
	QGraphicsScene* scene = nodes.isEmpty() || nodes.first() == nullptr ? nullptr : nodes.first()->scene();
	if (scene != nullptr)
	{
		for (QGraphicsItem* item : scene->items())
		{
			auto* arrow = dynamic_cast<Arrow*>(item);
			if (arrow == nullptr || arrows.contains(arrow) || all.contains(arrow))
				continue;
			for (Node* n : all)
			{
				if ((arrow->domain() != nullptr && isInside(arrow->domain(), n))
				 || (arrow->codomain() != nullptr && isInside(arrow->codomain(), n)))
				{
					arrows.append(arrow);
					break;
				}
			}
		}
	}
	return all + arrows;   // objects first, arrows last
}

NodesCreated::NodesCreated(const QString& description, const QList<Node*>& nodes)
	: Memento(description)
{
	for (Node* node : withAttachedArrows(nodes))
	{
		if (node == nullptr)
			continue;
		Held held;
		held.node = node;
		held.parent = dynamic_cast<QGraphicsObject*>(node->parentItem());
		held.scene = node->scene();
		held.pos = node->pos();
		m_held.append(held);
	}
}

NodesCreated::~NodesCreated()
{
	// if we still hold them when the history lets go of us, they are ours
	for (Held& held : m_held)
		if (held.owned && !held.node.isNull())
			delete held.node.data();
}

void NodesCreated::detachAll()
{
	// arrows first: they are last in the list
	for (int i = m_held.size() - 1; i >= 0; --i)
	{
		Held& held = m_held[i];
		if (held.node.isNull() || held.owned)
			continue;
		held.parent = dynamic_cast<QGraphicsObject*>(held.node->parentItem());
		held.scene = held.node->scene();
		held.pos = held.node->pos();
		held.node->setParentItem(nullptr);
		if (held.node->scene() != nullptr)
			held.node->scene()->removeItem(held.node.data());
		held.owned = true;
	}
}

void NodesCreated::attachAll()
{
	// objects first, so no arrow is put back before its ends are there
	for (Held& held : m_held)
	{
		if (held.node.isNull() || !held.owned)
			continue;
		if (!held.parent.isNull())
			held.node->setParentItem(held.parent.data());   // this puts it back in the scene too
		else if (auto* diagram = dynamic_cast<DiagramScene*>(held.scene); diagram != nullptr && diagram->ambientCategory() != nullptr)
			held.node->setParentItem(diagram->ambientCategory());   // its old home is gone: the canvas will do
		else if (held.scene != nullptr)
			held.scene->addItem(held.node.data());
		held.node->setPos(held.pos);
		held.node->refreshFrame();
		held.owned = false;
	}
}

void NodesCreated::undo()
{
	detachAll();
}

void NodesCreated::redo()
{
	attachAll();
}

QByteArray NodesCreated::payload() const
{
	// pointers mean nothing in a file: say what each item WAS - its kind, its
	// label, and where in the nesting it sat
	QByteArray bytes;
	QDataStream out(&bytes, QIODevice::WriteOnly);
	out.setVersion(QDataStream::Qt_6_0);
	out << qint32(m_held.size());
	for (const Held& held : m_held)
	{
		QString kind = "Object";
		QString id;
		QList<int> path;
		if (!held.node.isNull())
		{
			Node* node = held.node.data();
			if (dynamic_cast<Functor*>(node) != nullptr)       kind = "Functor";
			else if (dynamic_cast<Arrow*>(node) != nullptr)    kind = "Arrow";
			else if (dynamic_cast<Category*>(node) != nullptr) kind = "Category";
			id = node->id();
			path = node->pathFromRoot();
		}
		out << kind << id << path;
	}
	return bytes;
}

void ExistsSuchChanged::undo()
{
	if (!m_node.isNull())
		m_node->setExistsSuch(m_before);
}

void ExistsSuchChanged::redo()
{
	if (!m_node.isNull())
		m_node->setExistsSuch(m_after);
}

QByteArray ExistsSuchChanged::payload() const
{
	QByteArray bytes;
	QDataStream out(&bytes, QIODevice::WriteOnly);
	out.setVersion(QDataStream::Qt_6_0);
	out << (m_node.isNull() ? QString() : m_node->id())
	    << (m_node.isNull() ? QList<int>() : m_node->pathFromRoot())
	    << m_after;
	return bytes;
}

<<<<<<< HEAD:DiagramDetective/DiagramDetective/core/history/Mementos.cpp
void ArrowStyleChanged::undo()
{
	if (!m_arrow.isNull())
		m_arrow->setStyle(static_cast<Arrow::Style>(m_before));
}

void ArrowStyleChanged::redo()
{
	if (!m_arrow.isNull())
		m_arrow->setStyle(static_cast<Arrow::Style>(m_after));
}

QByteArray ArrowStyleChanged::payload() const
=======
void MonicChanged::undo()
{
	if (!m_arrow.isNull())
		m_arrow->setMonic(m_before);
}

void MonicChanged::redo()
{
	if (!m_arrow.isNull())
		m_arrow->setMonic(m_after);
}

QByteArray MonicChanged::payload() const
>>>>>>> 3e9da9ce39d6dc74c5a0385266cfd9f7c2eeaba9:DiagramDetective/DiagramDetective/history/Mementos.cpp
{
	QByteArray bytes;
	QDataStream out(&bytes, QIODevice::WriteOnly);
	out.setVersion(QDataStream::Qt_6_0);
	out << (m_arrow.isNull() ? QString() : m_arrow->id())
	    << (m_arrow.isNull() ? QList<int>() : m_arrow->pathFromRoot())
<<<<<<< HEAD:DiagramDetective/DiagramDetective/core/history/Mementos.cpp
	    << qint32(m_after);
	return bytes;
}

void DeleteMarkChanged::undo()
{
	if (!m_node.isNull())
		m_node->setDeleteMark(m_before);
}

void DeleteMarkChanged::redo()
{
	if (!m_node.isNull())
		m_node->setDeleteMark(m_after);
}

QByteArray DeleteMarkChanged::payload() const
{
	QByteArray bytes;
	QDataStream out(&bytes, QIODevice::WriteOnly);
	out.setVersion(QDataStream::Qt_6_0);
	out << (m_node.isNull() ? QString() : m_node->id())
	    << (m_node.isNull() ? QList<int>() : m_node->pathFromRoot())
=======
>>>>>>> 3e9da9ce39d6dc74c5a0385266cfd9f7c2eeaba9:DiagramDetective/DiagramDetective/history/Mementos.cpp
	    << m_after;
	return bytes;
}

<<<<<<< HEAD:DiagramDetective/DiagramDetective/core/history/Mementos.cpp
RuleApplied::RuleApplied(const QString& description, const QList<Node*>& made,
                         const QString& rulePath, const QString& ruleName,
                         const QStringList& variables, const QStringList& values)
	: NodesCreated(description, made)
	, m_rulePath(rulePath)
	, m_ruleName(ruleName)
	, m_variables(variables)
	, m_values(values)
{
}

QByteArray RuleApplied::payload() const
{
	// what was applied, and what its variables stood for, ahead of what it
	// drew in: a step of a proof reads as the rule first
	QByteArray bytes;
	QDataStream out(&bytes, QIODevice::WriteOnly);
	out.setVersion(QDataStream::Qt_6_0);
	out << m_rulePath << m_ruleName << m_variables << m_values;
	out << NodesCreated::payload();
=======
void EpicChanged::undo()
{
	if (!m_arrow.isNull())
		m_arrow->setEpic(m_before);
}

void EpicChanged::redo()
{
	if (!m_arrow.isNull())
		m_arrow->setEpic(m_after);
}

QByteArray EpicChanged::payload() const
{
	QByteArray bytes;
	QDataStream out(&bytes, QIODevice::WriteOnly);
	out.setVersion(QDataStream::Qt_6_0);
	out << (m_arrow.isNull() ? QString() : m_arrow->id())
	    << (m_arrow.isNull() ? QList<int>() : m_arrow->pathFromRoot())
	    << m_after;
>>>>>>> 3e9da9ce39d6dc74c5a0385266cfd9f7c2eeaba9:DiagramDetective/DiagramDetective/history/Mementos.cpp
	return bytes;
}

void Renamed::undo()
{
	if (!m_node.isNull())
		m_node->setId(m_before);
}

void Renamed::redo()
{
	if (!m_node.isNull())
		m_node->setId(m_after);
}

QByteArray Renamed::payload() const
{
	QByteArray bytes;
	QDataStream out(&bytes, QIODevice::WriteOnly);
	out.setVersion(QDataStream::Qt_6_0);
	out << m_before << m_after << (m_node.isNull() ? QList<int>() : m_node->pathFromRoot());
	return bytes;
}

LabelMoved::LabelMoved(const QString& description, Node* node, const QPointF& before, const QPointF& after)
	: PureGraphical(description)
	, m_node(node)
	, m_before(before)
	, m_after(after)
{
}

void LabelMoved::undo()
{
	if (!m_node.isNull())
		m_node->setLabelOffset(m_before);
}

void LabelMoved::redo()
{
	if (!m_node.isNull())
		m_node->setLabelOffset(m_after);
}

ExactnessChanged::ExactnessChanged(const QString& description, Category* category, bool rows, bool before, bool after)
	: Memento(description)
	, m_category(category)
	, m_rows(rows)
	, m_before(before)
	, m_after(after)
{
}

void ExactnessChanged::undo()
{
	if (m_category.isNull())
		return;
	if (m_rows) m_category->setRowsExact(m_before);
	else        m_category->setColumnsExact(m_before);
}

void ExactnessChanged::redo()
{
	if (m_category.isNull())
		return;
	if (m_rows) m_category->setRowsExact(m_after);
	else        m_category->setColumnsExact(m_after);
}

QByteArray ExactnessChanged::payload() const
{
	QByteArray bytes;
	QDataStream out(&bytes, QIODevice::WriteOnly);
	out.setVersion(QDataStream::Qt_6_0);
	out << (m_category.isNull() ? QString() : m_category->id()) << m_rows << m_after;
	return bytes;
}

StatementDeclared::StatementDeclared(const QString& description, DiagramScene* scene,
                                     int kindBefore, int kindAfter,
                                     const QString& nameBefore, const QString& nameAfter)
	: Memento(description)
	, m_scene(scene)
	, m_kindBefore(kindBefore)
	, m_kindAfter(kindAfter)
	, m_nameBefore(nameBefore)
	, m_nameAfter(nameAfter)
{
}

void StatementDeclared::undo()
{
	if (m_scene.isNull())
		return;
	m_scene->setStatementName(m_nameBefore);
	m_scene->setStatementKind(DiagramScene::StatementKind(m_kindBefore));
}

void StatementDeclared::redo()
{
	if (m_scene.isNull())
		return;
	m_scene->setStatementName(m_nameAfter);
	m_scene->setStatementKind(DiagramScene::StatementKind(m_kindAfter));
}

QByteArray StatementDeclared::payload() const
{
	QByteArray bytes;
	QDataStream out(&bytes, QIODevice::WriteOnly);
	out.setVersion(QDataStream::Qt_6_0);
	out << qint32(m_kindAfter) << m_nameAfter;
	return bytes;
}

// ---------------------------------------------------------------- what a node is

NodeRetyped::NodeRetyped(const QString& description, Node* before, Node* after)
	: Memento(description), m_before(before), m_after(after)
{
	// the change has already happened: `after` is the one in the scene
}

NodeRetyped::~NodeRetyped()
{
	// whichever shell is standing outside the scene is ours to destroy; the
	// one inside it belongs to the diagram
	Node* spare = m_isAfter ? m_before.data() : m_after.data();
	if (spare != nullptr && spare->scene() == nullptr && spare->parentItem() == nullptr)
		delete spare;
}

void NodeRetyped::swap(Node* from, Node* to)
{
	if (from == nullptr || to == nullptr)
		return;
	NodeKind::transplant(from, to);
	to->setSelected(false);
	if (auto* diagram = dynamic_cast<DiagramScene*>(to->scene()))
	{
		emit diagram->statementChanged(diagram->statementText());
		diagram->checkDiagram();
	}
}

void NodeRetyped::undo()
{
	if (!m_isAfter || m_before.isNull() || m_after.isNull())
		return;
	swap(m_after.data(), m_before.data());
	m_isAfter = false;
}

void NodeRetyped::redo()
{
	if (m_isAfter || m_before.isNull() || m_after.isNull())
		return;
	swap(m_before.data(), m_after.data());
	m_isAfter = true;
}

QByteArray NodeRetyped::payload() const
{
	// pointers mean nothing in a file: what it was, what it became, and what
	// it is called
	QByteArray bytes;
	QDataStream out(&bytes, QIODevice::WriteOnly);
	out.setVersion(QDataStream::Qt_6_0);
	out << (m_before.isNull() ? QString() : NodeKind::of(m_before.data()))
	    << (m_after.isNull() ? QString() : NodeKind::of(m_after.data()))
	    << (m_after.isNull() ? QString() : m_after->id());
	return bytes;
}
