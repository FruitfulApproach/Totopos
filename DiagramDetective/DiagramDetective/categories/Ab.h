#pragma once

#include "../Category.h"

// Abelian groups.
class Ab : public Category
{
	Q_OBJECT
public:
	explicit Ab(QGraphicsItem* parent = nullptr);

public:
	QString objectName() const override { return QStringLiteral("abelian group"); }
	QString morphismName() const override { return QStringLiteral("homomorphism"); }

protected:
	QChar firstLetter() const override { return QChar('A'); }
};
