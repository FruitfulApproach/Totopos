#pragma once

#include "../Category.h"

// Rings.
class Ring : public Category
{
	Q_OBJECT
public:
	explicit Ring(QGraphicsItem* parent = nullptr);

protected:
	QChar firstLetter() const override { return QChar('R'); }
};
