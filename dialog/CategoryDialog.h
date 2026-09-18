#pragma once

#include <QDialog>
#include <QStringList>
#include "ui_CategoryDialog.h"

QT_BEGIN_NAMESPACE
namespace Ui { class CategoryDialogClass; }
QT_END_NAMESPACE

// Defines a custom category: page 1 its name, page 2 the structure it has.
class CategoryDialog : public QDialog
{
	Q_OBJECT

public:
	explicit CategoryDialog(QWidget* parent = nullptr);
	~CategoryDialog() override;

	QString name() const;
	void setName(const QString& name);

	// the checked structure, by checkbox object name: "hasProducts", "isAbelian", ...
	QStringList properties() const;
	void setProperties(const QStringList& properties);

private slots:
	void back();
	void next();
	void syncButtons();

private:
	Ui::CategoryDialogClass* ui;
};
