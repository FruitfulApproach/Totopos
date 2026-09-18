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

// ------------------------------------------------------- what people type

namespace
{
	// The commands, without their backslash, and the character each one means.
	//
	// Code points rather than literal characters on purpose: a source file's
	// encoding is one more thing that can go wrong between an editor, a
	// compiler switch and a build machine, and a table of numbers reads the
	// same everywhere. All of these are in the basic plane, so one QChar does.
	struct Command
	{
		const char* name;
		ushort code;
	};

	const Command kCommands[] = {
		// ---- the ones this program is actually for
		{ "circ", 0x2218 },      { "to", 0x2192 },          { "rightarrow", 0x2192 },
		{ "leftarrow", 0x2190 }, { "gets", 0x2190 },        { "mapsto", 0x21A6 },
		{ "longrightarrow", 0x27F6 }, { "longleftarrow", 0x27F5 },
		{ "longmapsto", 0x27FC }, { "leftrightarrow", 0x2194 },
		{ "hookrightarrow", 0x21AA }, { "hookleftarrow", 0x21A9 },
		{ "twoheadrightarrow", 0x21A0 }, { "rightarrowtail", 0x21A3 },
		{ "Rightarrow", 0x21D2 }, { "Leftarrow", 0x21D0 },  { "Leftrightarrow", 0x21D4 },
		{ "uparrow", 0x2191 },   { "downarrow", 0x2193 },   { "updownarrow", 0x2195 },
		{ "nearrow", 0x2197 },   { "searrow", 0x2198 },     { "swarrow", 0x2199 },
		{ "nwarrow", 0x2196 },   { "rightsquigarrow", 0x21DD },

		// ---- binary operators
		{ "cdot", 0x22C5 },      { "times", 0x00D7 },       { "div", 0x00F7 },
		{ "pm", 0x00B1 },        { "mp", 0x2213 },          { "ast", 0x2217 },
		{ "star", 0x22C6 },      { "bullet", 0x2219 },      { "diamond", 0x22C4 },
		{ "oplus", 0x2295 },     { "ominus", 0x2296 },      { "otimes", 0x2297 },
		{ "oslash", 0x2298 },    { "odot", 0x2299 },        { "boxtimes", 0x22A0 },
		{ "wedge", 0x2227 },     { "vee", 0x2228 },         { "land", 0x2227 },
		{ "lor", 0x2228 },       { "cap", 0x2229 },         { "cup", 0x222A },
		{ "sqcap", 0x2293 },     { "sqcup", 0x2294 },       { "uplus", 0x228E },
		{ "setminus", 0x2216 },  { "amalg", 0x2A3F },       { "wr", 0x2240 },
		{ "ltimes", 0x22C9 },    { "rtimes", 0x22CA },      { "dagger", 0x2020 },
		{ "ddagger", 0x2021 },   { "triangleleft", 0x25C1 },{ "triangleright", 0x25B7 },

		// ---- relations
		{ "leq", 0x2264 },       { "le", 0x2264 },          { "geq", 0x2265 },
		{ "ge", 0x2265 },        { "neq", 0x2260 },         { "ne", 0x2260 },
		{ "ll", 0x226A },        { "gg", 0x226B },          { "equiv", 0x2261 },
		{ "sim", 0x223C },       { "simeq", 0x2243 },       { "cong", 0x2245 },
		{ "approx", 0x2248 },    { "asymp", 0x224D },       { "doteq", 0x2250 },
		{ "propto", 0x221D },    { "bowtie", 0x22C8 },      { "prec", 0x227A },
		{ "succ", 0x227B },      { "preceq", 0x2AAF },      { "succeq", 0x2AB0 },
		{ "subset", 0x2282 },    { "supset", 0x2283 },      { "subseteq", 0x2286 },
		{ "supseteq", 0x2287 },  { "subsetneq", 0x228A },   { "supsetneq", 0x228B },
		{ "sqsubseteq", 0x2291 },{ "sqsupseteq", 0x2292 },  { "in", 0x2208 },
		{ "notin", 0x2209 },     { "ni", 0x220B },          { "vdash", 0x22A2 },
		{ "dashv", 0x22A3 },     { "models", 0x22A8 },      { "perp", 0x22A5 },
		{ "parallel", 0x2225 },  { "mid", 0x2223 },         { "top", 0x22A4 },
		{ "bot", 0x22A5 },

		// ---- the rest of the furniture
		{ "infty", 0x221E },     { "emptyset", 0x2205 },    { "varnothing", 0x2205 },
		{ "forall", 0x2200 },    { "exists", 0x2203 },      { "nexists", 0x2204 },
		{ "neg", 0x00AC },       { "lnot", 0x00AC },        { "partial", 0x2202 },
		{ "nabla", 0x2207 },     { "sum", 0x2211 },         { "prod", 0x220F },
		{ "coprod", 0x2210 },    { "int", 0x222B },         { "oint", 0x222E },
		{ "sqrt", 0x221A },      { "angle", 0x2220 },       { "triangle", 0x25B3 },
		{ "square", 0x25A1 },    { "ldots", 0x2026 },       { "dots", 0x2026 },
		{ "cdots", 0x22EF },     { "vdots", 0x22EE },       { "ddots", 0x22F1 },
		{ "aleph", 0x2135 },     { "hbar", 0x210F },        { "ell", 0x2113 },
		{ "wp", 0x2118 },        { "Re", 0x211C },          { "Im", 0x2111 },
		{ "langle", 0x27E8 },    { "rangle", 0x27E9 },      { "lceil", 0x2308 },
		{ "rceil", 0x2309 },     { "lfloor", 0x230A },      { "rfloor", 0x230B },

		// ---- Greek
		{ "alpha", 0x03B1 },  { "beta", 0x03B2 },    { "gamma", 0x03B3 },
		{ "delta", 0x03B4 },  { "epsilon", 0x03B5 }, { "varepsilon", 0x03B5 },
		{ "zeta", 0x03B6 },   { "eta", 0x03B7 },     { "theta", 0x03B8 },
		{ "iota", 0x03B9 },   { "kappa", 0x03BA },   { "lambda", 0x03BB },
		{ "mu", 0x03BC },     { "nu", 0x03BD },      { "xi", 0x03BE },
		{ "pi", 0x03C0 },     { "rho", 0x03C1 },     { "sigma", 0x03C3 },
		{ "tau", 0x03C4 },    { "upsilon", 0x03C5 }, { "phi", 0x03C6 },
		{ "varphi", 0x03C6 }, { "chi", 0x03C7 },     { "psi", 0x03C8 },
		{ "omega", 0x03C9 },
		{ "Gamma", 0x0393 },  { "Delta", 0x0394 },   { "Theta", 0x0398 },
		{ "Lambda", 0x039B }, { "Xi", 0x039E },      { "Pi", 0x03A0 },
		{ "Sigma", 0x03A3 },  { "Upsilon", 0x03A5 }, { "Phi", 0x03A6 },
		{ "Psi", 0x03A8 },    { "Omega", 0x03A9 },
	};

	// \mathbb{Z} -> the double-struck Z, \mathcal{C} -> the script C.
	//
	// These take an argument, so they are not in the table. Most of each
	// alphabet sits in a run high up in plane 1, but the letters that were in
	// Unicode before those runs existed were left where they already were, and
	// the run has a hole at each of them. Hence the two lists of exceptions:
	// without them a blackboard Z comes out as a hole in a font, not a Z.
	uint styledLetter(const QString& style, QChar letter)
	{
		if (!letter.isUpper())
		{
			// only the lowercase script letters have a run of their own
			if (style == QLatin1String("mathcal") && letter.isLower())
				return letter == QLatin1Char('e') ? 0x212F
				     : letter == QLatin1Char('g') ? 0x210A
				     : letter == QLatin1Char('o') ? 0x2134
				     : 0x1D4B6 + uint(letter.unicode() - u'a');
			return 0;
		}
		const uint at = uint(letter.unicode() - u'A');
		if (style == QLatin1String("mathbb"))
		{
			switch (letter.unicode())
			{
			case u'C': return 0x2102;
			case u'H': return 0x210D;
			case u'N': return 0x2115;
			case u'P': return 0x2119;
			case u'Q': return 0x211A;
			case u'R': return 0x211D;
			case u'Z': return 0x2124;
			default:   return 0x1D538 + at;
			}
		}
		if (style == QLatin1String("mathcal") || style == QLatin1String("mathscr"))
		{
			switch (letter.unicode())
			{
			case u'B': return 0x212C;
			case u'E': return 0x2130;
			case u'F': return 0x2131;
			case u'H': return 0x210B;
			case u'I': return 0x2110;
			case u'L': return 0x2112;
			case u'M': return 0x2133;
			case u'R': return 0x211B;
			default:   return 0x1D49C + at;
			}
		}
		if (style == QLatin1String("mathfrak"))
		{
			switch (letter.unicode())
			{
			case u'C': return 0x212D;
			case u'H': return 0x210C;
			case u'I': return 0x2111;
			case u'R': return 0x211C;
			case u'Z': return 0x2128;
			default:   return 0x1D504 + at;
			}
		}
		return 0;
	}

	QString fromCodePoint(uint code)
	{
		if (code < 0x10000)
			return QString(QChar(ushort(code)));
		QString pair;
		pair += QChar(QChar::highSurrogate(code));
		pair += QChar(QChar::lowSurrogate(code));
		return pair;
	}
}

// ------------------------------------------------- and the way back again

namespace
{
	// Characters that MEAN one of the table's symbols but are not the code
	// point the table produces.
	//
	// Unicode has several rings, several crosses and several dots, and which
	// one arrives here depends on the keyboard, the web page it was copied
	// from, or the other program it came out of. Left alone they would be
	// different labels that look identical on screen - a rule written with one
	// would silently fail to match a diagram drawn with the other, and the
	// person would have no way of seeing why.
	//
	// Only lookalikes go in here. Characters that a mathematician distinguishes
	// deliberately do NOT: an isomorphism and a homotopy equivalence are both
	// squiggles and both stay exactly as they were typed.
	struct Lookalike
	{
		ushort typed;
		ushort meant;
	};

	const Lookalike kLookalikes[] = {
		{ 0x25E6, 0x2218 },   // white bullet -> ring operator
		{ 0x25CB, 0x2218 },   // white circle
		{ 0x26AC, 0x2218 },   // medium white circle
		{ 0x2022, 0x2219 },   // bullet -> bullet operator
		{ 0x00B7, 0x22C5 },   // middle dot -> dot operator
		{ 0x2A2F, 0x00D7 },   // vector cross product -> multiplication sign
		{ 0x2715, 0x00D7 },   // multiplication x
		{ 0x2A09, 0x00D7 },   // n-ary times
		{ 0x2300, 0x2205 },   // diameter sign -> empty set
		{ 0x2B0D, 0x2195 },   // up down black arrow
		{ 0x03D5, 0x03C6 },   // phi symbol -> phi
		{ 0x03F5, 0x03B5 },   // lunate epsilon -> epsilon
		{ 0x03D1, 0x03B8 },   // theta symbol -> theta
		{ 0x03D6, 0x03C0 },   // pi symbol -> pi
		{ 0x03F1, 0x03C1 },   // rho symbol -> rho
		{ 0x03F0, 0x03BA },   // kappa symbol -> kappa
		{ 0x2206, 0x0394 },   // increment -> capital delta
		{ 0x2126, 0x03A9 },   // ohm sign -> capital omega
		{ 0x212A, 0x004B },   // kelvin sign -> plain K
		{ 0x212B, 0x00C5 },   // angstrom sign
	};

	QString normalise(const QString& text)
	{
		QString out = text;
		for (int at = 0; at < out.size(); ++at)
		{
			const ushort code = out.at(at).unicode();
			if (code < 0x80)
				continue;   // nothing in the table is ASCII
			for (const Lookalike& one : kLookalikes)
				if (one.typed == code)
				{
					out[at] = QChar(one.meant);
					break;
				}
		}
		return out;
	}
}

QString Notation::autoCorrect(const QString& text)
{
	if (!text.contains(QLatin1Char('\\')))
		// A symbol typed or pasted straight in gets the same treatment as
		// one spelled out as a command, so the two routes to a label cannot
		// leave different characters in it.
		return normalise(text);   // no commands here, but still lookalikes

	QString out;
	out.reserve(text.size());
	int at = 0;
	while (at < text.size())
	{
		if (text.at(at) != QLatin1Char('\\'))
		{
			out += text.at(at);
			++at;
			continue;
		}

		// the longest run of letters after the backslash IS the name: taking
		// all of them is what keeps \subseteq from being read as \subset
		int end = at + 1;
		while (end < text.size() && text.at(end).isLetter())
			++end;
		const QString name = text.mid(at + 1, end - at - 1);
		if (name.isEmpty())
		{
			out += text.at(at);   // a lone backslash is a character like any other
			++at;
			continue;
		}

		// \mathbb{Z} and its kind: a name, then one letter in braces
		if (end + 2 < text.size() && text.at(end) == QLatin1Char('{')
		    && text.at(end + 2) == QLatin1Char('}'))
		{
			const uint styled = styledLetter(name, text.at(end + 1));
			if (styled != 0)
			{
				out += fromCodePoint(styled);
				at = end + 3;
				continue;
			}
		}

		bool found = false;
		for (const Command& command : kCommands)
		{
			if (name == QLatin1String(command.name))
			{
				out += QChar(command.code);
				found = true;
				break;
			}
		}
		if (!found)
		{
			// not one of ours: leave it exactly as typed. Someone may simply
			// have a backslash in a name.
			out += text.mid(at, end - at);
		}
		at = end;
	}
	return normalise(out);
}

QString Notation::toCommand(uint codePoint)
{
	// The FIRST name wins, which is why the table is written with the ordinary
	// spelling of each symbol before its aliases: an arrow comes back as \to,
	// not as \rightarrow, and a comparison as \leq, not \le.
	if (codePoint < 0x10000)
	{
		for (const Command& command : kCommands)
			if (command.code == ushort(codePoint))
				return QStringLiteral("\\") + QLatin1String(command.name);
	}

	// the styled alphabets, which are not in the table because they take an
	// argument: work back from the character to the letter it is a version of
	for (const char* style : { "mathbb", "mathcal", "mathfrak" })
		for (char letter = 'A'; letter <= 'z'; ++letter)
		{
			if (letter > 'Z' && letter < 'a')
				continue;   // the punctuation between the two alphabets
			if (styledLetter(QLatin1String(style), QLatin1Char(letter)) == codePoint)
				return QString("\\%1{%2}").arg(QLatin1String(style)).arg(QLatin1Char(letter));
		}

	return QString();
}

QString Notation::toCommands(const QString& text)
{
	QString out;
	out.reserve(text.size());
	for (int at = 0; at < text.size(); ++at)
	{
		const QChar here = text.at(at);
		uint code = here.unicode();
		int width = 1;
		if (here.isHighSurrogate() && at + 1 < text.size() && text.at(at + 1).isLowSurrogate())
		{
			code = QChar::surrogateToUcs4(here, text.at(at + 1));
			width = 2;
		}

		const QString command = code < 0x80 ? QString() : toCommand(code);
		if (command.isEmpty())
			out += text.mid(at, width);
		else
			out += command;
		at += width - 1;
	}
	return out;
}
