#pragma once

#include <QDialog>
#include <QList>
#include <QVariant>
#include <functional>

class QTreeWidget;
class QTreeWidgetItem;
class QStackedWidget;
class QLineEdit;
class QLabel;

// Tools > Settings, laid out like Visual Studio's Options: a search box over a
// tree of pages on the left, the selected page's options on the right, and
// OK / Cancel / Apply. Pages are built in code from a small option list, so
// the search can look inside them.
class SettingsDialog : public QDialog
{
	Q_OBJECT

public:
	explicit SettingsDialog(QWidget* parent = nullptr);

private:
	// one option: its label, and how to load / store it
	struct Option
	{
		QString label;
		const char* key = nullptr;
		QWidget* editor = nullptr;
		std::function<void()> load;    // settings -> editor
		std::function<void()> store;   // editor -> settings
	};
	struct Page
	{
		QString category;
		QString title;
		QTreeWidgetItem* item = nullptr;
		QWidget* widget = nullptr;
		QList<Option> options;
	};

	void buildPages();
	Page& addPage(const QString& category, const QString& title, const QString& blurb);
	void addBool(Page& page, const QString& label, const char* key, const QString& tip = QString());
	void addDouble(Page& page, const QString& label, const char* key, double min, double max, double step, const QString& suffix = QString(), const QString& tip = QString());
	void addChoice(Page& page, const QString& label, const char* key, const QStringList& choices, const QString& tip = QString());
	// A COLOUR. A button wearing the colour it sets, which opens the colour
	// dialog - the same chip the Properties page uses, so a colour is picked
	// the one way everywhere.
	void addColour(Page& page, const QString& label, const char* key, const QString& tip = QString());

	void loadAll();
	void storeAll();
	void applyAndNotify();
	void filter(const QString& text);
	void showPage(QTreeWidgetItem* item);
	void resetPageToDefaults();

	QTreeWidget* m_tree = nullptr;
	QStackedWidget* m_stack = nullptr;
	QLineEdit* m_search = nullptr;
	QLabel* m_header = nullptr;
	QList<Page> m_pages;
};
