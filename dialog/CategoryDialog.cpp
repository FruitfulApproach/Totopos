#include "dialog/CategoryDialog.h"

#include <QCheckBox>
#include <QPushButton>

CategoryDialog::CategoryDialog(QWidget* parent)
	: QDialog(parent)
	, ui(new Ui::CategoryDialogClass())
{
	ui->setupUi(this);
	ui->stack->setCurrentIndex(0);

	connect(ui->backButton, &QPushButton::clicked, this, &CategoryDialog::back);
	connect(ui->nextButton, &QPushButton::clicked, this, &CategoryDialog::next);
	connect(ui->nameEdit, &QLineEdit::textChanged, this, &CategoryDialog::syncButtons);
	connect(ui->nameEdit, &QLineEdit::returnPressed, this, &CategoryDialog::next);
	connect(ui->stack, &QStackedWidget::currentChanged, this, &CategoryDialog::syncButtons);

	// Enter must not fire OK from the name page; Next handles it there
	ui->buttonBox->button(QDialogButtonBox::Ok)->setAutoDefault(false);
	ui->nextButton->setAutoDefault(false);
	ui->backButton->setAutoDefault(false);

	syncButtons();
	ui->nameEdit->setFocus();
}

CategoryDialog::~CategoryDialog()
{
	delete ui;
}

QString CategoryDialog::name() const
{
	return ui->nameEdit->text().trimmed();
}

void CategoryDialog::setName(const QString& name)
{
	ui->nameEdit->setText(name);
}

QStringList CategoryDialog::properties() const
{
	QStringList props;
	for (auto* box : ui->pageProps->findChildren<QCheckBox*>())
		if (box->isChecked())
			props << box->objectName();
	return props;
}

void CategoryDialog::setProperties(const QStringList& properties)
{
	for (auto* box : ui->pageProps->findChildren<QCheckBox*>())
		box->setChecked(properties.contains(box->objectName()));
}

void CategoryDialog::back()
{
	ui->stack->setCurrentIndex(qMax(0, ui->stack->currentIndex() - 1));
}

void CategoryDialog::next()
{
	if (name().isEmpty())
		return;
	ui->stack->setCurrentIndex(qMin(ui->stack->count() - 1, ui->stack->currentIndex() + 1));
}

void CategoryDialog::syncButtons()
{
	const int page = ui->stack->currentIndex();
	const bool named = !name().isEmpty();
	ui->backButton->setEnabled(page > 0);
	ui->nextButton->setEnabled(named && page < ui->stack->count() - 1);
	ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(named);
}
