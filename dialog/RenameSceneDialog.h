#pragma once

#include <QDialog>
#include <QString>

class QLineEdit;
class QComboBox;
class QLabel;
class QPushButton;

// Renaming a file of the library. A file says what it IS in its own name -
// kernel.definition.totopos - so the name and the kind are asked for together
// and the filename is built out of the two.
//
// Nothing happens until Ok (or Ctrl+Enter). The dialog only reports what the
// new name WOULD be; the caller does the renaming, and only then does anything
// else in the program hear about it.
class RenameSceneDialog : public QDialog
{
	Q_OBJECT

public:
	// `path` is the file being renamed: its own name and kind fill the dialog in
	explicit RenameSceneDialog(const QString& path, QWidget* parent = nullptr);

	// what was asked for, once the dialog has been accepted
	QString baseName() const;
	int kind() const;
	// the whole filename, "<name>.<kind>.totopos" - no folder
	QString fileName() const;
	// and where it would go, beside the file it is renaming
	QString newPath() const;

private slots:
	void revalidate();

private:
	// Empty when the two answers make a name that can be used, otherwise what
	// is wrong with them, in one sentence.
	QString whatIsWrong() const;

	QString m_path;
	QLineEdit* m_name = nullptr;
	QLabel* m_state = nullptr;
	QComboBox* m_kind = nullptr;
	QPushButton* m_ok = nullptr;
};
