#pragma once

#include "../Category.h"

// Vector spaces over a field.
class Vect : public Category
{
	Q_OBJECT
public:
	explicit Vect(QGraphicsItem* parent = nullptr);

protected:
	QChar firstLetter() const override { return QChar('V'); }
};
