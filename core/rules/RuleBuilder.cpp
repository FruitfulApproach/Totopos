#include "core/rules/RuleBuilder.h"

#include <QDir>
#include <QFileInfo>

#include "core/rules/Library.h"
#include "art/Category.h"
#include "art/Object.h"
#include "art/Arrow.h"
#include "core/io/SceneFile.h"

RuleBuilder::RuleBuilder(const QString& category, const QString& name, DiagramScene::StatementKind kind)
{
	m_scene.setAmbientCategory(category);
	m_home = m_scene.ambientCategory();
	// What it is put forward as is settled at the end, so that declaring it
	// does not come out ahead of the steps that drew it.
	m_name = name;
	m_kind = kind;
}

Object* RuleBuilder::obj(const QString& name, qreal x, qreal y, Category* inside)
{
	Category* into = inside != nullptr ? inside : m_home;
	if (into == nullptr)
		return nullptr;
	Object* object = into->createObject(name, into->mapToScene(QPointF(x, y)));
	if (object != nullptr)
		m_made << object;
	return object;
}

Arrow* RuleBuilder::arr(const QString& name, Node* from, Node* to, Category* inside)
{
	Category* into = inside != nullptr ? inside : m_home;
	if (into == nullptr || from == nullptr || to == nullptr)
		return nullptr;
	Arrow* arrow = into->createArrow(name, from, to);
	if (arrow != nullptr)
		m_made << arrow;
	return arrow;
}

Object* RuleBuilder::some(const QString& name, qreal x, qreal y, Category* inside)
{
	Object* object = obj(name, x, y, inside);
	if (object != nullptr)
		object->setExistsSuch(true);
	return object;
}

Arrow* RuleBuilder::someArr(const QString& name, Node* from, Node* to, Category* inside)
{
	Arrow* arrow = arr(name, from, to, inside);
	if (arrow != nullptr)
		arrow->setExistsSuch(true);
	return arrow;
}

void RuleBuilder::gone(Node* node)
{
	if (node != nullptr)
		node->setDeleteMark(true);
}

void RuleBuilder::derived(Node* node, const QString& pattern, const QList<Node*>& sources)
{
	if (node != nullptr && !sources.isEmpty())
		node->setDerivedLabel(pattern, sources);
}

void RuleBuilder::bend(Arrow* arrow, qreal x, qreal y)
{
	if (arrow != nullptr)
		arrow->addBend(QPointF(x, y));
}

void RuleBuilder::note(const QString& text)
{
	m_scene.recordNote(text);
}

void RuleBuilder::step(const QString& description, const QList<Node*>& made)
{
	m_scene.recordCreation(description, made);
	for (Node* node : made)
		m_made.removeAll(node);
}

void RuleBuilder::commutes(bool on)
{
	m_scene.setCommutes(on);
}

void RuleBuilder::exactRows(bool on)
{
	// asserted of every piece of the diagram, the way the panel asserts it
	for (const DiagramScene::Component& piece : m_scene.components())
		for (Node* node : piece.objects)
			node->setRowsExactInComponent(on);
	if (m_home != nullptr)
		m_home->setRowsExact(on);
}

void RuleBuilder::exactColumns(bool on)
{
	for (const DiagramScene::Component& piece : m_scene.components())
		for (Node* node : piece.objects)
			node->setColumnsExactInComponent(on);
	if (m_home != nullptr)
		m_home->setColumnsExact(on);
}

void RuleBuilder::defines(const QString& term)
{
	m_scene.setDefines(term);
}

void RuleBuilder::proves(const QString& libraryPath)
{
	m_scene.setProves(libraryPath);
}

Category* RuleBuilder::categoryOf(Object* object) const
{
	return dynamic_cast<Category*>(object);
}

bool RuleBuilder::write(const QString& relativePath, QString* error)
{
	const QString base = Library::ensureRoot();
	if (base.isEmpty())
	{
		if (error != nullptr)
			*error = QStringLiteral("There is nowhere to put the library.");
		return false;
	}

	// anything placed and not yet spoken for is one step, so a rule read as a
	// proof still says how its picture was built
	if (!m_made.isEmpty())
	{
		m_scene.recordCreation(QString("Draw %1").arg(m_name), m_made);
		m_made.clear();
	}

	// every piece of a diagram that says it commutes, says so
	if (m_scene.commutes())
		for (const DiagramScene::Component& piece : m_scene.components())
			for (Node* node : piece.objects)
				node->setCommutesInComponent(true);

	m_scene.setStatementName(m_name);
	m_scene.setStatementKind(m_kind);

	const QString path = base + "/" + relativePath;
	QDir().mkpath(QFileInfo(path).absolutePath());
	return SceneFile::save(&m_scene, path, error);
}
