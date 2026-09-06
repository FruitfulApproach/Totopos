#pragma once

#include "../Category.h"

// The category of (possibly large) categories: an object of BigCat is a category.
class BigCat : public Category
{
	Q_OBJECT
public:
	explicit BigCat(QGraphicsItem* parent = nullptr);

protected:
	Object* makeObject(const QString& name) override;
	QChar firstLetter() const override { return QChar('C'); }
};
