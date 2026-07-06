#include "Terminal.h"

// Basic ANSI, dynamic, and extended colors support.
// See: https://invisible-island.net/xterm/ctlseqs/ctlseqs.html#h2-Operating-System-Commands

#define LLOG(x)     // RLOG("TerminalCtrl (#" << this << "]: " << x)
#define LTIMING(x)  // RTIMING(x)

namespace Upp {

TerminalCtrl& TerminalCtrl::ResetColors()
{
	// The U++ color constants with 'S' prefix are automatically adjusted
	// to the color theme of OS. On the other hand, the 8 ANSI colors and
	// their brighter counterparts are assumed to be constant. Therefore
	// it would be better if we avoid using the auto-adjusted versions by
	// default, and leave it up to client code to change them on demand.
	// Note that this rule does not apply to the default ink, paper, and
	// selection colors.

	colortable[COLOR_BLACK] = Black();
	colortable[COLOR_RED] = Red();
	colortable[COLOR_GREEN] = Green();
	colortable[COLOR_YELLOW] = Yellow();
	colortable[COLOR_BLUE] = Blue();
	colortable[COLOR_MAGENTA] = Magenta();
	colortable[COLOR_CYAN] = Cyan();
	colortable[COLOR_WHITE] = White();

	colortable[COLOR_LTBLACK] = Black();
	colortable[COLOR_LTRED] = LtRed();
	colortable[COLOR_LTGREEN] = LtGreen();
	colortable[COLOR_LTYELLOW] = LtYellow();
	colortable[COLOR_LTBLUE] = LtBlue();
	colortable[COLOR_LTMAGENTA] = LtMagenta();
	colortable[COLOR_LTCYAN] = LtCyan();
	colortable[COLOR_LTWHITE] = White();

	colortable[COLOR_INK] = SColorText;
	colortable[COLOR_INK_SELECTED] = SColorHighlightText;
	colortable[COLOR_PAPER] = SColorPaper;
	colortable[COLOR_PAPER_SELECTED] = SColorHighlight;
	colortable[COLOR_ANNOTATION_UNDERLINE] = SYellow;

	return *this;
}

void TerminalCtrl::SetInkAndPaperColor(const VTCell& cell, Color& ink, Color& paper)
{
	ink = GetColorFromIndex(cell, COLOR_INK);
	paper = GetColorFromIndex(cell, COLOR_PAPER);

	bool invert = cell.IsInverted()
				^ modes[DECSCNM]
				^ ((hyperlinks || annotations) && cell.IsHypertext() && activehtext == cell.data);

	if(invert)
		Swap(ink, paper);
}

Color TerminalCtrl::GetColorFromIndex(const VTCell& cell, int which) const
{
	Color color = (which == COLOR_INK) ? cell.ink : cell.paper;
	bool dim = (which == COLOR_INK) && cell.IsFaint();
	int index = which;

	if(!IsNull(color)) {
		int c = color.GetSpecial();
		if(c >= 0) {
			index = c;
			if(index > 15 && index < 256) {
				int v = index - 16;
				if(index < 232) {
					// 256-color (6x6x6 cube & grayscale)
					auto cstep = [](int i) {
						return i == 0 ? 0 : 55 + i * 40;
					};
					color = Color(cstep(v / 36), cstep((v % 36) / 6), cstep(v % 6));
				}
				else {
					// Grayscale is correct: 232 = 8, 255 = 238
					int gray = (index - 232) * 10 + 8;
					color = Color(gray, gray, gray);
				}
			}
		}
	}

	// Only map through colortable if it's the default background/foreground
	// OR if it's an explicit 0-15 base special color index.
	if(IsNull(color) || (color.GetSpecial() >= 0 && color.GetSpecial() <= 15)) {
		if(lightcolors || (intensify && which == COLOR_INK && cell.IsBold())) {
			if(index < 8)
				index += 8;
		}
		color = colortable[index];

		if(adjustcolors)
			color = AdjustIfDark(color);
	}

	return dim ? Blend(color, Black(), 77) : color;
}

void TerminalCtrl::ReportANSIColor(int opcode, int index, const Color& c)
{
	String reply = Format("%d;%d;%", opcode, index, ConvertColor().Format(c));
	LLOG("ReportAnsiColor() -> OSC " << reply);
	PutOSC(reply);
}

void TerminalCtrl::ReportDynamicColor(int opcode, const Color& c)
{
	String reply = Format("%d;%", opcode, ConvertColor().Format(c));
	LLOG("ReportDynamicColor() -> OSC " << reply);
	PutOSC(reply);
}

void TerminalCtrl::SetProgrammableColors(const AnsiParser::Sequence& seq, int opcode)
{
	if(!dynamiccolors || seq.parameters.GetCount() < decode(opcode, 4, 3, 2))
		return;

	int changed_colors = 0;

	// OSC 4;[color;spec|...] or OSC [10|11|17|19];[spec|...]
	// Note: Both OSC can set multiple colors at once.

	if(opcode == 4) { // ANSI + aixterm colors.
		for(int i = 1; i < seq.parameters.GetCount(); i += 2) {
			int j = seq.GetInt(i + 1, 0);
			if(j >= 0 && j < ANSI_COLOR_COUNT) {
				String s = seq.GetStr(i + 2);
				if(s.IsEqual("?")) {
					ReportANSIColor(opcode, j, colortable[j]);
				}
				else
				if(!IsNull(s)) {
					if(SetSaveColor(j, ConvertColor().Scan(s)))
						changed_colors++;
				}
			}
		}
	}
	else { // xterm dynamic colors.
		auto GetColorIndex = [](int op) {
			return decode(op,
				10, COLOR_INK,
				11, COLOR_PAPER,
				17, COLOR_INK_SELECTED,
				19, COLOR_PAPER_SELECTED, 0);
		};
		for(int i = 1; i < seq.parameters.GetCount(); i++, opcode++) {
			int j = GetColorIndex(opcode);
			if(!j)
				continue;
			String s = seq.GetStr(i + 1);
			if(s.IsEqual("?")) {
				ReportDynamicColor(opcode, colortable[j]);
			}
			else
			if(!IsNull(s)) {
				if(SetSaveColor(j, ConvertColor().Scan(s)))
					changed_colors++;
			}
		}
	}

	if(changed_colors > 0)
		Ctrl::Refresh();
}

void TerminalCtrl::ResetProgrammableColors(const AnsiParser::Sequence& seq, int opcode)
{
	if(!dynamiccolors || seq.parameters.GetCount() < decode(opcode, 104, 2, 1))
		return;

	int changed_colors = 0;

	// OSC 104;[color;...] or OSC [110|111|117|119]
	if(opcode == 104 && seq.GetInt(2, -1) == -1) { // Reset all ANSI + aixterm colors.
		savedcolors.Clear();
		ResetColors();
		Ctrl::Refresh();
		return;
	}

	for(int i = decode(opcode, 104, 1, 0); i < seq.parameters.GetCount(); i++) {
		int j = seq.GetInt(i + 1, 0);
		if(opcode == 104) {
			if(j >= 0 && j < ANSI_COLOR_COUNT) {
				if(ResetLoadColor(j))
					changed_colors++;
			}
		}
		else {
			j = decode(j,
					110, COLOR_INK,
					111, COLOR_PAPER,
					117, COLOR_INK_SELECTED,
					119, COLOR_PAPER_SELECTED, 0);
			if(j > 0 && ResetLoadColor(j))
				changed_colors++;
		}
	}

	if(changed_colors > 0)
		Ctrl::Refresh();
}

bool TerminalCtrl::SetSaveColor(int index, const Color& c)
{
	LLOG("SetSaveColor(" << index << ")");
	if(IsNull(c)) return false;
	if(savedcolors.Find(index) < 0)
		savedcolors.Add(index, colortable[index]);
	colortable[index] = c;
	return true;
}

bool TerminalCtrl::ResetLoadColor(int index)
{
	LLOG("ResetLoadColor(" << index << ")");
	int i = savedcolors.Find(index);
	if(i < 0) return false;
	colortable[index] = savedcolors[i];
	savedcolors.Remove(i);
	return true;
}

void TerminalCtrl::ParseExtendedColors(VTCell& attrs, const Vector<String>& opcodes, int& index)
{
	LTIMING("TerminalCtrl::ParseExtendedColors");

	// Handles ISO-8613-6 (mixed colons/semicolons) color formats

	int values[8] = {0};
	int count = 0;
	int opconsumed = 0;
	bool hascolon = false;

	for(int i = index; i < opcodes.GetCount() && count < 8; i++) {
		const String& op = opcodes[i];
		if(op.Find(':') >= 0)
			hascolon = true;

		const char* p = op.Begin();
		const char* e = op.End();

		while(p < e && count < 8) {
			if(*p == ':') { // empty parameter (e.g. ignored CS)
				count++;
				p++;
			}
			else
			if(IsDigit(*p)) {
				int val = 0;
				while(p < e && IsDigit(*p))
					val = val * 10 + (*p++ - '0');
				values[count++] = val;
				if(p < e && *p == ':')
					p++;
			}
			else {
				break;
			}
		}

		opconsumed++;

		if(count >= 2) {
			int type = values[1];
			int expected = 0;

			if(type == 5)
				expected = 3;
			else
			if(type == 2 || type == 3)
				expected = hascolon ? 6 : 5; // potential ColorSpace ID included
			else
			if(type == 4)
				expected = hascolon ? 7 : 6;

			if(count >= expected)
				break;
			if(hascolon && (type == 2 || type == 3) && count == 5 && i == opcodes.GetCount() - 1)
				break; // Fallback for omitted ColorSpace ID
		}
	}

	if(count < 3 || (values[0] != 38 && values[0] != 48))
		return;

	int which = values[0];
	int palette = values[1];
	int cidx = 2; // Offset for where color variables actually start

	// ISO-8613-6 specifies an optional color space ID before the colors.
	if(hascolon) {
		if((palette == 2 || palette == 3) && count >= 6)
			cidx++;
		else
		if(palette == 4 && count >= 7)
			cidx++;
	}

	Color color = Null;

	if(palette == 2 && count >= cidx + 3) {
		color = Color(clamp(values[cidx], 0, 255),
					clamp(values[cidx+1], 0, 255),
					clamp(values[cidx+2], 0, 255));
	}
	else
	if(palette == 3 && count >= cidx + 3) {
		color = CmykColorf(values[cidx] / 100.0, values[cidx+1] / 100.0, values[cidx+2] / 100.0, 0.0);
	}
	else
	if(palette == 4 && count >= cidx + 4) {
		color = CmykColorf(values[cidx] / 100.0, values[cidx+1] / 100.0, values[cidx+2] / 100.0, values[cidx+3] / 100.0);
	}
	else
	if(palette == 5 && count >= 3) {
		color = Color::Special(clamp(values[cidx], 0, 255));
	}
	else
		return; // Malformed sequence

	if(which == 38)
		attrs.ink = color;
	else if(which == 48)
		attrs.paper = color;

	// Fast-forward external loop tracker
	index += opconsumed - 1;
}

void TerminalCtrl::ColorTableSerializer::Serialize(Stream& s)
{
	for(int i = 0; i < TerminalCtrl::MAX_COLOR_COUNT; i++)
		s % table[i];
}

void TerminalCtrl::ColorTableSerializer::Jsonize(JsonIO& jio)
{
	for(int i = 0; i < TerminalCtrl::MAX_COLOR_COUNT; i++)
		switch(i) {
		case TerminalCtrl::COLOR_INK:
			jio("Ink", table[i]);
			break;
		case TerminalCtrl::COLOR_PAPER:
			jio("Paper", table[i]);
			break;
		case TerminalCtrl::COLOR_INK_SELECTED:
			jio("SelectionInk", table[i]);
			break;
		case TerminalCtrl::COLOR_PAPER_SELECTED:
			jio("SelectionPaper", table[i]);
			break;
		case TerminalCtrl::COLOR_ANNOTATION_UNDERLINE:
			jio("AnnotationUnderline", table[i]);
			break;
		default:
			jio(Format("Color_%d", i), table[i]);
			break;
		}
}

void TerminalCtrl::ColorTableSerializer::Xmlize(XmlIO& xio)
{
	XmlizeByJsonize(xio, *this);
}

int ConvertHashColorSpec::Filter(int chr) const
{
	return (IsXDigit(chr) || chr == '#') ? chr : 0;
}

Value ConvertHashColorSpec::Scan(const Value& text) const
{
	const String& s = text;
	if(s.IsEmpty() || s[0] != '#')
		return ErrorValue(t_("Bad hash color text format"));

	const char *p = ~s;
	int len = 0;
	uint64 x = 0;

	for(p++; *p; p++) {
		byte c = *p;
		if(c >= '0' && c <= '9') {
			if(len < 16) x = (x << 4) | (c - '0');
			len++;
		}
		else
		if((c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f')) {
			if(len < 16) x = (x << 4) | ((c & 0x0F) + 9); // Highly optimized hex addition math
			len++;
		}
		else
		if(c != ' ' && c != '\t') {
			break;
		}
	}

	switch(len) {
	case 3:
		return Color(byte(x >> 4) & 0xF0, byte(x) & 0xF0, byte(x << 4));
	case 6:
		return Color(byte(x >> 16), byte(x >> 8), byte(x));
	case 9:
		return Color(byte(x >> 28), byte(x >> 16), byte(x >> 4));
	case 12:
		return Color(byte(x >> 40), byte(x >> 24), byte(x >> 8));
	default:
		return ErrorValue(t_("Bad hash color text format"));
	}
}

Value ConvertHashColorSpec::Format(const Value& q) const
{
	if(q.Is<Color>()) {
		const Color& c = q.To<Color>();
		return Upp::Format("#%02x%02x%02x", c.GetR(), c.GetG(), c.GetB());
	}
	return Upp::ErrorValue(t_("Bad color value"));
}

int ConvertRgbColorSpec::Filter(int chr) const
{
	return IsXDigit(chr) || findarg(chr, 'r', 'g', 'R', 'G', ':', '/', ',') >= 0 ? chr : 0;
}

Value ConvertRgbColorSpec::Scan(const Value& text) const
{
	const String& s = text;
	const char *p = ~s;
	int components[4] = {0, 0, 0, 255};
	int count = 0;
	int radix = 10;
	bool rgba = false;

	auto IsDelimiter = [](char c) {
		return (c == ':' || c == '/' || c == ',' || c == '(' || c == ')' || c == ' ' || c == '\t');
	};

	while(*p == ' ' || *p == '\t') p++;

	if(s.GetLength() >= 3 && (*p == 'r' || *p == 'R')) {
		if((p[1] == 'g' || p[1] == 'G') && (p[2] == 'b' || p[2] == 'B')) {
			p += 3;
			rgba = (*p == 'a' || *p == 'A');
			if(rgba) p++;
			radix = 16;
		}
	}

	// Zero-allocation strtol parsing
	while(count < 4 && *p) {
		while(*p && IsDelimiter(*p)) p++;
		if(!*p) break;

		char *end = nullptr;
		int val = strtol(p, &end, radix);
		if(end == p) break;

		components[count++] = val;
		p = end;
	}

	if((count == 3) || (rgba && count == 4)) {
		RGBA rgba;
		rgba.r = byte(components[0] > 255 ? components[0] >> 8 : components[0]);
		rgba.g = byte(components[1] > 255 ? components[1] >> 8 : components[1]);
		rgba.b = byte(components[2] > 255 ? components[2] >> 8 : components[2]);
		rgba.a = byte(count == 4 ? (components[3] > 255 ? components[3] >> 8 : components[3]) : 255);
		return Color(rgba);
	}

	return ErrorValue(t_("Bad rgb/a color text format"));
}

Value ConvertRgbColorSpec::Format(const Value& q) const
{
	if(q.Is<Color>()) {
		const Color& c = q.To<Color>();
		return Upp::Format("rgb:%04x/%04x/%04x", c.GetR() * 257, c.GetG() * 257, c.GetB() * 257);
	}
	return Upp::ErrorValue(t_("Bad color value"));
}

int ConvertCmykColorSpec::Filter(int chr) const
{
	return IsXDigit(chr) || findarg(chr, 'm', 'y', 'k', 'M', 'Y', 'K', ':', '/', ',', '.') >= 0 ? chr : 0;
}

Value ConvertCmykColorSpec::Scan(const Value& text) const
{
	const String& s = text;
	const char *p = ~s;

	while(*p == ' ' || *p == '\t') p++;

	bool is_cmyk = false;
	if((*p == 'c' || *p == 'C') && (p[1] == 'm' || p[1] == 'M') && (p[2] == 'y' || p[2] == 'Y')) {
		p += 3;
		if(*p == 'k' || *p == 'K') {
			is_cmyk = true;
			p++;
		}
	} else {
		return ErrorValue(t_("Bad cmy/k color text format"));
	}

	auto IsDelimiter = [](char c) {
		return (c == ':' || c == '/' || c == ' ' || c == '\t');
	};

	double components[4] = {0, 0, 0, 0};
	int count = 0;

	// Zero-allocation strtod parsing replaces dynamic string Splitting
	while(count < 4 && *p) {
		while(*p && IsDelimiter(*p)) p++;
		if(!*p) break;

		char *end = nullptr;
		double val = strtod(p, &end);
		if(end == p) break;

		components[count++] = val;
		p = end;
	}

	if(count == 4 && is_cmyk)
		return CmykColorf(components[0], components[1], components[2], components[3]);
	else if(count == 3 && !is_cmyk)
		return CmykColorf(components[0], components[1], components[2], 0.0);

	return ErrorValue(t_("Bad cmy/k color text format"));
}

Value ConvertCmykColorSpec::Format(const Value& q) const
{
	if(q.Is<Color>()) {
		const Color& r = q.To<Color>();
		double c, m, y, k;
		RGBtoCMYK(r.GetR() / 255.0, r.GetG() / 255.0, r.GetB() / 255.0, c, m, y, k);
		return Upp::Format("cmyk:%f/%f/%f/%f", c, m, y, k);
	}
	return Upp::ErrorValue(t_("Bad color value"));
}

Value ConvertColor::Scan(const Value& text) const
{
	Value v = ConvertHashColorSpec().Scan(text);
	if(IsError(v)) {
		v = ConvertRgbColorSpec().Scan(text);
		if(IsError(v))
			v = ConvertCmykColorSpec().Scan(text);
	}
	return v;
}

Value ConvertColor::Format(const Value& q) const
{
	return ConvertRgbColorSpec().Format(q);
}

}