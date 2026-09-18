#pragma once

#include <QObject>
#include <QString>

// A proposition about something drawn — a category, a sketch. A subclass says
// what it asserts; the thing it is about owns it (QObject parent).
class Prop : public QObject
{
	Q_OBJECT

public:
	explicit Prop(QObject* parent = nullptr);
	~Prop() override;

	// machine name, e.g. "hasProducts" — the checkbox object name in dialogs
	virtual QString key() const = 0;
	// for people, e.g. "Has products"
	virtual QString label() const = 0;
	virtual QString description() const { return QString(); }
};
