#pragma once

#include "art/Category.h"

// Vector spaces over a field.
class Vect : public Category
{
	Q_OBJECT
public:
	explicit Vect(QGraphicsItem* parent = nullptr);

public:
	QString objectName() const override { return QStringLiteral("vector space"); }
	QString implicitArrowName() const override { return QStringLiteral("0"); }   // the zero map
	QString morphismName() const override { return QStringLiteral("linear map"); }

protected:
	QChar firstLetter() const override { return QChar('V'); }
};
