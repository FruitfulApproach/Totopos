#include "core/english/Translation.h"

#include <QSet>
#include <QStringList>
#include <QTextDocumentFragment>

#include "art/DiagramScene.h"
#include "art/Category.h"
#include "art/Arrow.h"
#include "art/Node.h"
#include "core/Notation.h"

namespace
{
	const QChar kTo(0x2192);        // ->
	const QChar kDash(0x2014);      // em dash

	QString plural(const QString& word)
	{
		if (word.isEmpty())
			return word;
		if (word.endsWith("s") || word.endsWith("x") || word.endsWith("z")
		 || word.endsWith("ch") || word.endsWith("sh"))
			return word + "es";
		// category -> categories, but day -> days: only a consonant before
		// the y takes the ies
		if (word.endsWith("y") && word.size() > 1 && !QStringLiteral("aeiou").contains(word.at(word.size() - 2)))
			return word.left(word.size() - 1) + "ies";
		return word + "s";
	}

	QString aOrAn(const QString& word)
	{
		if (word.isEmpty())
			return QStringLiteral("a");
		const QChar first = word.at(0).toLower();
		// "an R-module", because the letter is read "ar"; "a set"; "an object"
		static const QString vowels = QStringLiteral("aeiou");
		static const QString saidWithAVowel = QStringLiteral("aefhilmnorsx");
		const bool acronym = word.size() > 1 && word.at(0).isUpper() && !word.at(1).isLower();
		return (acronym ? saidWithAVowel : vowels).contains(first)
			? QStringLiteral("an") : QStringLiteral("a");
	}

	// what to write for a label: 1_X becomes 1 with a subscript. This dock is
	// rich text, so it gets the same <sub>/<sup> the diagram's labels get, and
	// toHtml escapes the rest on the way through.
	QString labelOf(const QString& raw)
	{
		return Notation::toHtml(raw);
	}

	QString nameOf(const Node* node)
	{
		if (node == nullptr)
			return QStringLiteral("?");
		if (auto* arrow = dynamic_cast<const Arrow*>(node))
		{
			const QString effective = arrow->effectiveId();
			if (!effective.isEmpty())
				return labelOf(effective);
		}
		else if (!node->id().isEmpty())
		{
			return labelOf(node->id());
		}
		return QStringLiteral("an unnamed one");
	}

	QString maths(const QString& text)
	{
		return QString("<b>%1</b>").arg(text);
	}

	// join: "A", "A and B", "A, B and C"
	QString andList(const QStringList& parts)
	{
		if (parts.isEmpty())
			return QString();
		if (parts.size() == 1)
			return parts.first();
		QStringList head = parts;
		const QString last = head.takeLast();
		return QString("%1 and %2").arg(head.join(", "), last);
	}

	// One thing the diagram talks about, and everything needed to say it.
	struct Item
	{
		Node* node = nullptr;
		bool isArrow = false;
		QString name;
		QString kindWord;    // "R-module", "R-linear map"
		QString home;        // the category it is drawn in, when not the ambient one
		QString domain, codomain;
		bool exists = false;
		bool struck = false;
	};

	QString kindWordFor(Node* node, bool isArrow)
	{
		Category* home = node->surroundingCategory();
		if (home == nullptr)
			return isArrow ? QStringLiteral("arrow") : QStringLiteral("object");
		return isArrow ? home->morphismName() : home->objectName();
	}

	void collect(QGraphicsItem* parent, const Category* ambient, QList<Item>& out)
	{
		for (QGraphicsItem* child : parent->childItems())
		{
			auto* node = dynamic_cast<Node*>(child);
			if (node == nullptr)
				continue;   // a label
			auto* arrow = dynamic_cast<Arrow*>(node);
			const bool named = arrow != nullptr ? !arrow->effectiveId().isEmpty() : !node->id().isEmpty();
			if (named)
			{
				Item item;
				item.node = node;
				item.isArrow = arrow != nullptr;
				item.name = nameOf(node);
				item.kindWord = kindWordFor(node, item.isArrow);
				if (auto* home = dynamic_cast<Category*>(node->parentItem()); home != nullptr && home != ambient)
					item.home = labelOf(home->id());
				if (arrow != nullptr)
				{
					item.domain = nameOf(arrow->domain());
					item.codomain = nameOf(arrow->codomain());
				}
				item.exists = node->existsSuch();
				item.struck = node->markedForDeletion();
				out << item;
			}
			collect(node, ambient, out);
		}
	}

	// How the kind word is read. A universal names a CLASS - "For all
	// categories C and objects X in C" - so it is always plural and never
	// takes an article, however many names follow it; "For all a category C"
	// is not English. An existential names particular things - "there exists
	// an arrow f : A -> B" - and agrees with how many there are.
	enum class Number { Universal, Existential };

	// "R-modules A, B and C", "R-linear maps f : A -> B and g : B -> C",
	// grouping things that are read together
	QString phraseFor(const QList<Item>& items, Number number)
	{
		QStringList parts;
		int at = 0;
		while (at < items.size())
		{
			const bool isArrow = items.at(at).isArrow;
			const QString kind = items.at(at).kindWord;
			const QString home = items.at(at).home;
			QStringList names;
			while (at < items.size() && items.at(at).isArrow == isArrow
			       && items.at(at).kindWord == kind && items.at(at).home == home)
			{
				const Item& item = items.at(at++);
				names << (item.isArrow
					? maths(QString("%1 : %2 %3 %4").arg(item.name, item.domain, QString(kTo), item.codomain))
					: maths(item.name));
			}

			QString part = number == Number::Universal || names.size() > 1
				? QString("%1 %2").arg(plural(kind), andList(names))
				: QString("%1 %2 %3").arg(aOrAn(kind), kind, names.first());
			if (!home.isEmpty())
				part += QString(" in %1").arg(maths(home));
			parts << part;
		}
		return andList(parts);
	}

	QString headingFor(const DiagramScene* scene)
	{
		if (scene->statementKind() == DiagramScene::Unstated)
			return QString();
		const QString kind = DiagramScene::kindName(scene->statementKind());
		QString heading = scene->statementName().isEmpty()
			? kind
			: QString("%1 %2 %3").arg(kind, QString(kDash), scene->statementName().toHtmlEscaped());
		return QString("<p><b>%1.</b></p>").arg(heading);
	}

	// what this KIND of statement carries besides the picture
	QString structureFor(const DiagramScene* scene)
	{
		QStringList lines;
		switch (scene->statementKind())
		{
		case DiagramScene::Proof:
			lines << (scene->proves().isEmpty()
				? QStringLiteral("This is a proof, but it does not yet say what it proves.")
				: QString("A proof of %1.").arg(maths(scene->proves().toHtmlEscaped())));
			if (const QList<DiagramScene::ProofStep> steps = scene->proofSteps(); !steps.isEmpty())
			{
				QStringList said;
				for (const DiagramScene::ProofStep& step : steps)
				{
					if (step.ruleName.isEmpty())
					{
						said << step.description.toHtmlEscaped();
						continue;
					}
					QStringList bound;
					for (int i = 0; i < step.variables.size() && i < step.values.size(); ++i)
						if (step.variables.at(i) != step.values.at(i))
							bound << maths(QString("%1 = %2").arg(labelOf(step.variables.at(i)),
							                                      labelOf(step.values.at(i))));
					said << (bound.isEmpty()
						? QString("by %1").arg(maths(step.ruleName.toHtmlEscaped()))
						: QString("by %1, with %2").arg(maths(step.ruleName.toHtmlEscaped()), andList(bound)));
				}
				lines << QString("It goes in %1 step%2: %3.")
					.arg(steps.size()).arg(steps.size() == 1 ? "" : "s", said.join("; "));
			}
			break;
		case DiagramScene::Definition:
			if (!scene->defines().isEmpty())
				lines << QString("This defines %1.").arg(maths(scene->defines().toHtmlEscaped()));
			break;
		case DiagramScene::Theorem:
		case DiagramScene::Conjecture:
			if (!scene->provedBy().isEmpty())
			{
				QStringList proofs;
				for (const QString& path : scene->provedBy())
					proofs << maths(path.toHtmlEscaped());
				lines << QString("Proved in %1.").arg(andList(proofs));
			}
			else if (scene->statementKind() == DiagramScene::Conjecture)
			{
				lines << QStringLiteral("Put forward as a conjecture: nothing here is granted.");
			}
			break;
		case DiagramScene::Axiom:
			lines << QStringLiteral("Put forward as an axiom: it is granted, and owes no proof.");
			break;
		default:
			break;
		}
		return lines.isEmpty() ? QString() : QString("<p>%1</p>").arg(lines.join(" "));
	}
}

QString Translation::describe(const DiagramScene* scene)
{
	return describe(scene, QList<Node*>());
}

QString Translation::describe(const DiagramScene* scene, const QList<Node*>& only)
{
	if (scene == nullptr || scene->ambientCategory() == nullptr)
		return QStringLiteral("<p>There is no diagram to read.</p>");

	const Category* ambient = scene->ambientCategory();
	QList<Item> all;
	collect(scene->ambientCategory(), ambient, all);

	// A selection is read on its own, but an arrow in it still has to name its
	// two ends, so those come along whether they were picked or not.
	const bool whole = only.isEmpty();
	QList<Item> items;
	if (whole)
	{
		items = all;
	}
	else
	{
		QSet<Node*> wanted;
		for (Node* node : only)
		{
			if (node == nullptr)
				continue;
			wanted.insert(node);
			if (auto* arrow = dynamic_cast<Arrow*>(node))
			{
				if (arrow->domain() != nullptr) wanted.insert(arrow->domain());
				if (arrow->codomain() != nullptr) wanted.insert(arrow->codomain());
			}
		}
		for (const Item& item : all)
			if (wanted.contains(item.node))
				items << item;
	}

	if (items.isEmpty())
		return whole
			? QString("<p>Nothing is drawn in %1 yet. Double-click the canvas to place an object.</p>")
				.arg(maths(labelOf(ambient->id())))
			: QStringLiteral("<p>Nothing is selected. Pick something in the diagram, or turn the switch "
			                 "off to read the whole thing.</p>");

	// the three parts a diagram is read in
	QList<Item> given, claimed, gone;
	for (const Item& item : items)
	{
		if (item.exists)       claimed << item;
		else if (item.struck)  gone << item;
		else                   given << item;
	}
	// what is taken away still has to be there to be taken away, so it is
	// quantified over along with everything else that is drawn solid
	QList<Item> universal = given;
	universal += gone;

	// A dashed item whose name is already quantified over is that same thing
	// drawn again - the X an identity arrow points back at is the X the rule
	// was given, not a second one - and "for all objects X, there exists an
	// object X" says nothing. Only what the name is NEW for is claimed.
	QSet<QString> alreadyNamed;
	for (const Item& item : universal)
		alreadyNamed << item.name;
	for (int i = claimed.size() - 1; i >= 0; --i)
		if (alreadyNamed.contains(claimed.at(i).name))
			claimed.removeAt(i);

	QString html;
	if (whole)
	{
		html += headingFor(scene);
		html += structureFor(scene);
	}

	// ---- the setting
	QStringList sentences;
	sentences << QString("Everything here is drawn in %1%2.")
		.arg(maths(labelOf(ambient->id())),
		     ambient->objectName() == QLatin1String("object")
		         ? QString()
		         : QString(", whose objects are %1 and whose arrows are %2")
		             .arg(plural(ambient->objectName()), plural(ambient->morphismName())));

	// ---- what it quantifies over
	QString sentence;
	if (!universal.isEmpty())
		sentence = QString("For all %1").arg(phraseFor(universal, Number::Universal));

	// ---- the conditions on them
	QStringList conditions;
	// A diagram with no two paths between the same pair of objects commutes
	// for want of anything to say: there is no equation behind the word. One
	// arrow, or a tree of them, is such a diagram - so saying "such that the
	// diagram commutes" there rules nothing out and only lengthens the
	// sentence. The equations are what decide it, not the flag.
	const QStringList equations = scene->commutes() ? scene->commutingEquations(true) : QStringList();
	const bool saysCommutes = !equations.isEmpty();
	if (saysCommutes)
	{
		QString commuting = QStringLiteral("the diagram commutes");
		if (whole)
		{
			QStringList written;
			for (const QString& equation : equations)
				written << maths(labelOf(equation));
			commuting += QString(" %1 that is, %2").arg(QString(kDash), andList(written));
		}
		conditions << commuting;
	}
	if (whole)
	{
		QStringList exactRows, exactColumns;
		for (const DiagramScene::Component& piece : scene->components())
		{
			if (piece.rowsExact) exactRows << maths(labelOf(piece.title));
			if (piece.columnsExact) exactColumns << maths(labelOf(piece.title));
		}
		if (!exactRows.isEmpty())
			conditions << QString("the rows of %1 are exact").arg(andList(exactRows));
		if (!exactColumns.isEmpty())
			conditions << QString("the columns of %1 are exact").arg(andList(exactColumns));
	}
	if (!conditions.isEmpty() && !sentence.isEmpty())
		sentence += QString(" such that %1").arg(andList(conditions));

	// ---- and what the diagram then says there is
	QStringList results;
	if (!claimed.isEmpty())
		results << QString("there %1 %2")
			.arg(claimed.size() == 1 ? "exists" : "exist", phraseFor(claimed, Number::Existential));
	if (!gone.isEmpty())
	{
		QStringList names;
		for (const Item& item : gone)
			names << maths(item.name);
		results << QString("%1 %2 taken out %3 %4 marks %5")
			.arg(andList(names), gone.size() == 1 ? "is" : "are", QString(kDash),
			     gone.size() == 1 ? "the red cross" : "the red crosses",
			     gone.size() == 1 ? "it" : "them");
	}

	if (!results.isEmpty())
	{
		sentence += sentence.isEmpty()
			? QString("In this diagram %1").arg(andList(results))
			: QString(", %1").arg(andList(results));
		if (saysCommutes)
			sentence += QStringLiteral(", and the diagram commutes still");
	}
	else if (!conditions.isEmpty() && universal.isEmpty())
	{
		sentence = QString("Here %1").arg(andList(conditions));
	}

	if (!sentence.isEmpty())
		sentences << sentence + ".";

	if (whole && scene->isChasing())
		sentences << QString("A chase is under way: %1 hypothes%2 %3 been added to the setup it started from.")
			.arg(scene->hypotheses().size())
			.arg(scene->hypotheses().size() == 1 ? "is" : "es")
			.arg(scene->hypotheses().size() == 1 ? "has" : "have");

	if (whole && !scene->cycleError().isEmpty())
		sentences << QString("As drawn this cannot be meant: %1").arg(scene->cycleError().toHtmlEscaped());

	html += QString("<p>%1</p>").arg(sentences.join(" "));

	if (!whole)
		html += QStringLiteral("<p><i>This reads the selection only. Turn the switch off for the whole "
		                       "diagram.</i></p>");
	return html;
}

QString Translation::asPlainText(const QString& richText)
{
	return QTextDocumentFragment::fromHtml(richText).toPlainText();
}
