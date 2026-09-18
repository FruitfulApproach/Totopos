#pragma once

#include "art/Category.h"

// Sets and functions.
class Set : public Category
{
	Q_OBJECT
public:
	explicit Set(QGraphicsItem* parent = nullptr);

public:
	QString objectName() const override { return QStringLiteral("set"); }
	QString morphismName() const override { return QStringLiteral("function"); }

protected:
	QChar firstLetter() const override { return QChar('X'); }
};
