#pragma once

#include "../Category.h"

// Sets and functions.
class Set : public Category
{
	Q_OBJECT
public:
	explicit Set(QGraphicsItem* parent = nullptr);

protected:
	QChar firstLetter() const override { return QChar('X'); }
};
