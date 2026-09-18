#pragma once

#include "art/Category.h"

// Abelian groups.
class Ab : public Category
{
	Q_OBJECT
public:
	explicit Ab(QGraphicsItem* parent = nullptr);

	QString builtInName() const override { return QStringLiteral("Ab"); }

public:
	QString objectName() const override { return QStringLiteral("abelian group"); }
	QString implicitArrowName() const override { return QStringLiteral("0"); }   // the zero map
	QString morphismName() const override { return QStringLiteral("homomorphism"); }

protected:
	QChar firstLetter() const override { return QChar('A'); }
};
