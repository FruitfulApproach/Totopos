#pragma once

#include <QString>
#include <QList>
#include <QPointF>

#include "art/DiagramScene.h"

class Category;
class Object;
class Arrow;
class Node;

// Building one library file.
//
// The format is binary and compressed, so a rule is written by the same code
// that reads it back rather than by hand (see library/Library.h). This is that
// code, said in as few words as possible: a rule should read like the picture
// it draws.
//
//     RuleBuilder rule("R-Mod", "Kernel", DiagramScene::Definition);
//     Object* A = rule.obj("A", -120, 0);
//     Object* B = rule.obj("B",  120, 0);
//     rule.arr("f", A, B);
//     Object* K = rule.some("Ker f", -120, -140);
//     rule.derived(K, "Ker %1", { rule.arrowNamed("f") });
//     rule.someArr("k", K, A);
//     rule.write("R-Mod/kernel.definition.totopos", &error);
//
// Solid is the premise - a pattern, every label a variable. Dotted (`some`,
// `someArr`) is what the rule says there then is. Crossed out (`gone`) is what
// it takes away. A rule with neither dotted nor crossed-out parts is a
// RECOGNISER: it says this scene is there, and citing it is a step.
class RuleBuilder
{
public:
	RuleBuilder(const QString& category, const QString& name, DiagramScene::StatementKind kind);

	DiagramScene* scene() { return &m_scene; }
	Category* home() const { return m_home; }

	// solid: quantified over
	Object* obj(const QString& name, qreal x, qreal y, Category* inside = nullptr);
	Arrow* arr(const QString& name, Node* from, Node* to, Category* inside = nullptr);
	// dotted: claimed to exist
	Object* some(const QString& name, qreal x, qreal y, Category* inside = nullptr);
	Arrow* someArr(const QString& name, Node* from, Node* to, Category* inside = nullptr);

	// crossed out in red: found, then taken away
	void gone(Node* node);

	// a label built out of other labels: "Ker %1" over the arrow it is the
	// kernel of, so it follows whatever that arrow turns out to be
	void derived(Node* node, const QString& pattern, const QList<Node*>& sources);

	// pull an arrow's line through a point, so two arrows between the same
	// ends are told apart
	void bend(Arrow* arrow, qreal x, qreal y);

	// a step of reasoning, and a step that made something
	void note(const QString& text);
	void step(const QString& description, const QList<Node*>& made);

	// what the picture claims
	void commutes(bool on = true);
	void exactRows(bool on = true);
	void exactColumns(bool on = true);
	void defines(const QString& term);
	void proves(const QString& libraryPath);

	// an object of this category drawn inside another object of it
	Category* categoryOf(Object* object) const;

	// write it at that path below the library root; the folders are made
	bool write(const QString& relativePath, QString* error = nullptr);

private:
	DiagramScene m_scene;
	QString m_name;
	DiagramScene::StatementKind m_kind = DiagramScene::Unstated;
	Category* m_home = nullptr;
	QList<Node*> m_made;   // everything placed since the last step
};
