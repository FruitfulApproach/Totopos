#pragma once

#include <QDockWidget>

class DiagramScene;
class QTextBrowser;
class QLabel;
class ToggleSwitch;

// The diagram read out in English, with the mathematics written as
// mathematics. It follows whichever diagram is in front; the switch narrows it
// to whatever is selected, so one square of a large chase can be read on its
// own.
class EnglishDock : public QDockWidget
{
	Q_OBJECT

public:
	explicit EnglishDock(QWidget* parent = nullptr);

	void setScene(DiagramScene* scene);
	DiagramScene* scene() const { return m_scene; }

	// is it reading the selection rather than the whole diagram?
	bool selectionOnly() const;

public slots:
	void refresh();

private:
	DiagramScene* m_scene = nullptr;
	QLabel* m_note = nullptr;
	QTextBrowser* m_text = nullptr;
	ToggleSwitch* m_selectionOnly = nullptr;
};
