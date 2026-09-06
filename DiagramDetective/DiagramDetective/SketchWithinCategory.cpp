#include "SketchWithinCategory.h"

SketchWithinCategory::SketchWithinCategory(const QString& name, Category* category)
	: Object(name)
	, m_category(category)
{
}

SketchWithinCategory::~SketchWithinCategory()
{
}

const QList<Arrow*> SketchWithinCategory::equalPaths(Node* /*domain*/, Node* /*codomain*/) const
{
	// TODO: the parallel paths between two objects that the sketch asserts equal
	return {};
}

QList<Object*> SketchWithinCategory::objects(const QString& id) const
{
	QList<Object*> result;
	for (Node* node : m_nodes.value(id))
		if (auto* object = dynamic_cast<Object*>(node))
			result.append(object);
	return result;
}

QList<Arrow*> SketchWithinCategory::arrows(const QString& id) const
{
	QList<Arrow*> result;
	for (Node* node : m_nodes.value(id))
		if (auto* arrow = dynamic_cast<Arrow*>(node))
			result.append(arrow);
	return result;
}

void SketchWithinCategory::add(Node* item)
{
	if (item == nullptr)
		return;
	
	file(item);

	item->setParentItem(this);
	// follow the node: an id change re-files it, its deletion unfiles it
	connect(item, &Node::idChanged, this, &SketchWithinCategory::onNodeIdChanged);
	connect(item, &Node::deleted, this, &SketchWithinCategory::onNodeDeleted);
}

void SketchWithinCategory::remove(Node* item)
{
	if (item == nullptr)
		return;
	if (unfile(item)) {
		disconnect(item, nullptr, this, nullptr);
		item->setParentItem(nullptr);
	}	
}

void SketchWithinCategory::onNodeIdChanged(Node* node, const QString& /*newId*/)
{
	unfile(node);   // filed under the old id
	file(node);     // re-filed under the new one (node->id() is already the new id)
}

void SketchWithinCategory::onNodeDeleted(Node* node)
{
	// emitted from ~Node: only the address is used, the object is never touched
	unfile(node);
}

bool SketchWithinCategory::file(Node* node)
{
	if (!m_nodes.contains(node->id()))
	{
		m_nodes[node->id()] = QList<Node*>{ node };
		return true;
	}
	else
	{
		auto& bucket = m_nodes[node->id()];

		if (!bucket.contains(node))
		{
			bucket.append(node);
			return true;
		}
		else
			return false;
	}
}

bool SketchWithinCategory::unfile(Node* node)
{
	for (auto it = m_nodes.begin(); it != m_nodes.end(); ++it)
	{
		if (it.value().removeOne(node))
		{
			if (it.value().isEmpty())
				m_nodes.erase(it);
			return true;
		}
	}
	return false;
}
