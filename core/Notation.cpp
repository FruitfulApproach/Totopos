#include "core/Notation.h"

namespace
{
	// One pass over the text. `write` is handed each run: the text and the tag
	// it belongs in ("sub", "sup", or empty for ordinary text). Scanning and
	// writing are the same walk, so hasScripts() and toHtml() cannot disagree
	// about what counts as a script.
	//
	// NOT called `emit`: that is a Qt macro that expands to nothing, so
	// write(run, tag) would quietly become the comma expression (run, tag) and
	// this would output the empty string for everything.
	template <typename Write>
	void scan(const QString& text, Write write)
	{
		int at = 0;
		while (at < text.size())
		{
			const QChar marker = text.at(at);
			const bool isMarker = marker == QLatin1Char('_') || marker == QLatin1Char('^');
			if (!isMarker || at + 1 >= text.size())
			{
				// a marker with nothing after it is just a character
				write(QString(marker), QString());
				++at;
				continue;
			}

			const char* tag = marker == QLatin1Char('_') ? "sub" : "sup";
			const bool braced = text.at(at + 1) == QLatin1Char('{');
			int from = at + 1;
			int end = 0;          // one past the group
			int consumed = 0;     // one past everything this took, braces included

			if (braced)
			{
				const int close = text.indexOf(QLatin1Char('}'), from + 1);
				if (close < 0)
				{
					// an opening brace with no closing one: as typed, and go
					// on from the marker so the rest still reads normally
					write(QString(marker), QString());
					++at;
					continue;
				}
				from = from + 1;
				end = close;
				consumed = close + 1;
			}
			else
			{
				end = from + 1;
				consumed = end;
			}

			const QString group = text.mid(from, end - from);
			if (group.isEmpty())
				write(text.mid(at, consumed - at), QString());   // x_{} is nothing to raise
			else
				write(group, QString::fromLatin1(tag));
			at = consumed;
		}
	}
}

QString Notation::toHtml(const QString& text)
{
	QString out;
	scan(text, [&out](const QString& run, const QString& tag) {
		if (tag.isEmpty())
		{
			out += run.toHtmlEscaped();
			return;
		}
		out += QStringLiteral("<%1>%2</%1>").arg(tag, run.toHtmlEscaped());
	});
	return out;
}

bool Notation::hasScripts(const QString& text)
{
	bool found = false;
	scan(text, [&found](const QString&, const QString& tag) {
		if (!tag.isEmpty())
			found = true;
	});
	return found;
}
