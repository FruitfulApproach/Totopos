#pragma once

#include "ArrowProp.h"

// The remaining built-in arrow properties are plain claims: nothing to
// construct, nothing to keep in step - just a fact the user asserts about
// this one arrow, drawn the way the fact is usually written by hand.

// f is cancellable on the left: for g, h : Z -> X, f o g = f o h implies
// g = h. Drawn with a hooked tail, the way an inclusion usually is (X ↪ Y).
class Monomorphism : public ArrowProp
{
	Q_OBJECT
public:
	explicit Monomorphism(Arrow* arrow = nullptr) : ArrowProp(arrow) {}
	static QString Key() { return QStringLiteral("monomorphism"); }
	QString key() const override { return Key(); }
	QString label() const override { return QStringLiteral("Monomorphism"); }
	QString description() const override
	{
		return QStringLiteral("Cancellable on the left: for g, h : Z → X, f∘g = f∘h implies g = h. "
		                      "Drawn with a hooked tail, the way an inclusion usually is.");
	}
};

// f is cancellable on the right: for g, h : Y -> Z, g o f = h o f implies
// g = h. Drawn with a doubled head, the way a quotient usually is (X ↠ Y).
class Epimorphism : public ArrowProp
{
	Q_OBJECT
public:
	explicit Epimorphism(Arrow* arrow = nullptr) : ArrowProp(arrow) {}
	static QString Key() { return QStringLiteral("epimorphism"); }
	QString key() const override { return Key(); }
	QString label() const override { return QStringLiteral("Epimorphism"); }
	QString description() const override
	{
		return QStringLiteral("Cancellable on the right: for g, h : Y → Z, g∘f = h∘f implies g = h. "
		                      "Drawn with a doubled head, the way a quotient usually is.");
	}
};
