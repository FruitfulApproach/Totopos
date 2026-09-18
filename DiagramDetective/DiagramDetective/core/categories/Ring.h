#pragma once

#include "art/Category.h"

// Rings.
class Ring : public Category
{
	Q_OBJECT
public:
	explicit Ring(QGraphicsItem* parent = nullptr);

public:
	QString objectName() const override { return QStringLiteral("ring"); }
	QString morphismName() const override { return QStringLiteral("ring homomorphism"); }

protected:
	QChar firstLetter() const override { return QChar('R'); }
};
