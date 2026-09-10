#pragma once

#include "../Category.h"
#include "../Functor.h"

// The category of (possibly large) categories: an object of BigCat is a category.
class BigCat : public Category
{
	Q_OBJECT
public:
	explicit BigCat(QGraphicsItem* parent = nullptr);

	QString builtInName() const override { return QStringLiteral("BigCat"); }

public:
	QString objectName() const override { return QStringLiteral("category"); }
	QString morphismName() const override { return QStringLiteral("functor"); }

protected:
	Object* makeObject(const QString& name) override;
	QChar firstLetter() const override { return QChar('C'); }
	// an arrow between categories is a functor
	Arrow* createArrow(const QString& name, Node* from, Node* to) override;
	QChar firstArrowLetter() const override { return QChar('F'); }
};
