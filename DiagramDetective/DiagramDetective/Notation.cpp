#include "Notation.h"

namespace
{
	struct Mapping
	{
		char plain;
		ushort code;
	};

	// Unicode has subscripts for the digits, a few signs, and SEVENTEEN
	// lowercase letters. There are no subscript capitals at all.
	const Mapping kSubscripts[] = {
		{'0', 0x2080}, {'1', 0x2081}, {'2', 0x2082}, {'3', 0x2083}, {'4', 0x2084},
		{'5', 0x2085}, {'6', 0x2086}, {'7', 0x2087}, {'8', 0x2088}, {'9', 0x2089},
		{'+', 0x208A}, {'-', 0x208B}, {'=', 0x208C}, {'(', 0x208D}, {')', 0x208E},
		{'a', 0x2090}, {'e', 0x2091}, {'h', 0x2095}, {'i', 0x1D62}, {'j', 0x2C7C},
		{'k', 0x2096}, {'l', 0x2097}, {'m', 0x2098}, {'n', 0x2099}, {'o', 0x2092},
		{'p', 0x209A}, {'r', 0x1D63}, {'s', 0x209B}, {'t', 0x209C}, {'u', 0x1D64},
		{'v', 0x1D65}, {'x', 0x2093},
	};

	// superscripts run to most of the lowercase alphabet and a good part of
	// the uppercase one
	const Mapping kSuperscripts[] = {
		{'0', 0x2070}, {'1', 0x00B9}, {'2', 0x00B2}, {'3', 0x00B3}, {'4', 0x2074},
		{'5', 0x2075}, {'6', 0x2076}, {'7', 0x2077}, {'8', 0x2078}, {'9', 0x2079},
		{'+', 0x207A}, {'-', 0x207B}, {'=', 0x207C}, {'(', 0x207D}, {')', 0x207E},
		{'a', 0x1D43}, {'b', 0x1D47}, {'c', 0x1D9C}, {'d', 0x1D48}, {'e', 0x1D49},
		{'f', 0x1DA0}, {'g', 0x1D4D}, {'h', 0x02B0}, {'i', 0x2071}, {'j', 0x02B2},
		{'k', 0x1D4F}, {'l', 0x02E1}, {'m', 0x1D50}, {'n', 0x207F}, {'o', 0x1D52},
		{'p', 0x1D56}, {'r', 0x02B3}, {'s', 0x02E2}, {'t', 0x1D57}, {'u', 0x1D58},
		{'v', 0x1D5B}, {'w', 0x02B7}, {'x', 0x02E3}, {'y', 0x02B8}, {'z', 0x1DBB},
		{'A', 0x1D2C}, {'B', 0x1D2E}, {'D', 0x1D30}, {'E', 0x1D31}, {'G', 0x1D33},
		{'H', 0x1D34}, {'I', 0x1D35}, {'J', 0x1D36}, {'K', 0x1D37}, {'L', 0x1D38},
		{'M', 0x1D39}, {'N', 0x1D3A}, {'O', 0x1D3C}, {'P', 0x1D3E}, {'R', 0x1D3F},
		{'T', 0x1D40}, {'U', 0x1D41}, {'V', 0x2C7D}, {'W', 0x1D42},
	};

	QChar lookupEitherCase(const Mapping* table, int count, QChar c)
	{
		for (int i = 0; i < count; ++i)
			if (QChar(QLatin1Char(table[i].plain)) == c)
				return QChar(table[i].code);
		// 1_X is written with a small x: there IS no subscript capital, and
		// the shape is what is being asked for
		if (c.isLetter())
		{
			const QChar other = c.isUpper() ? c.toLower() : c.toUpper();
			if (other != c)
				for (int i = 0; i < count; ++i)
					if (QChar(QLatin1Char(table[i].plain)) == other)
						return QChar(table[i].code);
		}
		return QChar();
	}

	// the whole group, or nothing at all
	QString convert(const QString& group, bool subscript)
	{
		QString out;
		for (QChar c : group)
		{
			const QChar mapped = subscript
				? Notation::subscriptFor(c)
				: Notation::superscriptFor(c);
			if (mapped.isNull())
				return QString();   // one character short: leave the lot alone
			out += mapped;
		}
		return out;
	}
}

QChar Notation::subscriptFor(QChar c)
{
	return lookupEitherCase(kSubscripts, int(sizeof(kSubscripts) / sizeof(kSubscripts[0])), c);
}

QChar Notation::superscriptFor(QChar c)
{
	return lookupEitherCase(kSuperscripts, int(sizeof(kSuperscripts) / sizeof(kSuperscripts[0])), c);
}

QString Notation::withScripts(const QString& text)
{
	QString out;
	int at = 0;
	while (at < text.size())
	{
		const QChar marker = text.at(at);
		const bool isMarker = marker == QLatin1Char('_') || marker == QLatin1Char('^');
		if (!isMarker || at + 1 >= text.size())
		{
			out += marker;
			++at;
			continue;
		}

		const bool subscript = marker == QLatin1Char('_');
		const bool braced = text.at(at + 1) == QLatin1Char('{');
		int from = at + 1;
		int end = 0;          // one past the group
		int consumed = 0;     // one past everything this took, braces included

		if (braced)
		{
			const int close = text.indexOf(QLatin1Char('}'), from + 1);
			if (close < 0)
			{
				out += marker;   // an opening brace with no closing one: as typed
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
		const QString converted = convert(group, subscript);
		if (group.isEmpty() || converted.isEmpty())
			out += text.mid(at, consumed - at);   // nothing doing: exactly as typed
		else
			out += converted;
		at = consumed;
	}
	return out;
}
