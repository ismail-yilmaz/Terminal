#include "Terminal.h"

// Basic ANSI, dynamic, and extended colors support.
// See: https://invisible-island.net/xterm/ctlseqs/ctlseqs.html#h2-Operating-System-Commands

#define LLOG(x)     // RLOG("TerminalCtrl (#" << this << "]: " << x)
#define LTIMING(x)	// RTIMING(x)

namespace Upp {

TerminalCtrl& TerminalCtrl::ResetColors()
{
	// The U++ color constants with 'S' prefix are automatically adjusted
	// to the color theme of OS. On the other hand, the 8 ANSI colors and
	// their brighter  counterparts are assumed to be constant. Therefore
	// it would be better if we avoid using the auto-adjusted versions by
	// default, and leave it up to client code to change them  on demand.
	// Note that this rule does not apply to the  default ink, paper, and
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

	// This is a hot-path. Hence, the slightly obscured but optimized code.
	// Logic: Odd numbers (1-3) should swap, and even numbers (0, 2) should retain colors.

	int invert = (int) cell.IsInverted()
			   + (int) modes[DECSCNM]
	           + (int) ((hyperlinks || annotations) && cell.IsHypertext() && activehtext == cell.data);
	
	if(invert & 1)
		Swap(ink, paper);
}

Color TerminalCtrl::GetColorFromIndex(const VTCell& cell, int which) const
{
	Color color = which == COLOR_INK ? cell.ink : cell.paper;
	bool dim = which == COLOR_INK && cell.IsFaint();

	auto AdjustBrightness = [](Color c, double v) -> Color
	{
		double hc, sc, vc;
		RGBtoHSV(c.GetR() / 255.0, c.GetG() / 255.0, c.GetB() / 255.0, hc, sc, vc);
		return HsvColorf(hc, sc, vc * v);
	};

	int index = which;
	
	if(!IsNull(color)) {
		int c = color.GetSpecial();
		if(c >= 0) {
			index = c;
			if((index) > 15) {	// 256-color (6x6x6 cube)
				byte r, g, b;
				if(index < 232) {
					r =	(( index - 16) / 36) * 51,
					g =	(((index - 16) % 36) / 6) * 51,
					b =	(( index - 16) % 6)  * 51;
				}
				else  // Grayscale
					r = g = b = (index - 232) * 10 + 8;
				color = Color(r, g, b);
				goto End;
			}
		}
		else
			goto End;
	}

	if(lightcolors ||
		(intensify && which == COLOR_INK && cell.IsBold()))
			if(index < 8)
				index += 8;

	color = colortable[index];	// Adjust only the first 16 colors.

	if(adjustcolors)
		color = AdjustIfDark(color);

End:
	return dim ? AdjustBrightness(color, 0.70) : color;
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

void TerminalCtrl::SetProgrammableColors(const VTInStream::Sequence& seq, int opcode)
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
		auto GetColorIndex = [](int opcode)
		{
			return decode(opcode,
				10, COLOR_INK,
				11, COLOR_PAPER,
				17, COLOR_INK_SELECTED,
				19, COLOR_PAPER_SELECTED, 0);
		};
		for(int i = 1; i < seq.parameters.GetCount(); i++, opcode++) {
			int j = GetColorIndex(opcode);
			if(!j) continue;
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

void TerminalCtrl::ResetProgrammableColors(const VTInStream::Sequence& seq, int opcode)
{
	if(!dynamiccolors || seq.parameters.GetCount() < decode(opcode, 104, 2, 1))
		return;

	int changed_colors = 0;
	
	// OSC 104;[color;...] or OSC [110|111|117|119]
	// Note: Both OSC can reset multiple colors at once.
	
	if(opcode == 104 && seq.GetInt(2, -1) == -1) { // Reset all ANSI + aixterm colors.
			savedcolors.Clear();
			ResetColors();
			Ctrl::Refresh();
			return;
	}
	
	for(int i = decode(opcode, 104, 1, 0); i < seq.parameters.GetCount(); i++) {
		int  j = seq.GetInt(i + 1, 0);
		if(opcode == 104) { // ANSI + aixterm colors.
			if(j >= 0 && j < ANSI_COLOR_COUNT) {
				if(ResetLoadColor(j))
					changed_colors++;
			}
		}
		else { // xterm dynamic colors.
			int j = decode(j,
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
static int sParseExtendedColorFormat(Color& color, int& which, int& palette, const String& s, int format)
{
	const char *p = s.Begin(), *e = s.End();
	int values[8] = { 0 };
	int count = 0;
	
	// Lambda: Try to parse number at current position
	auto tryParseNumber = [&]() -> bool {
		bool negative = false;
		if(*p == '-') {
			negative = true;
			p++;
		}
		
		int val = 0;
		bool has_digits = false;
		while(p < e && IsDigit(*p)) {
			val = val * 10 + (*p++ - '0');
			has_digits = true;
		}
		
		if(has_digits) {
			if(negative)
				val = 0;  // Clamp negative to zero
			values[count++] = val;
			return true;
		}
		return false;
	};
	
	// Lambda: Handle non-digit token
	auto handleNonDigitToken = [&]() {
		// Non-digit character that's not a separator – count it as a token anyway
		if(p < e && *p != ':' && *p != ';') {
			count++;
		}
	};
	
	// Lambda: Skip separator if present
	auto skipSeparator = [&]() -> bool {
		if(p < e && (*p == ':' || *p == ';')) {
			p++;
			return true;
		}
		return false;
	};
	
	// Lambda: Check if we should stop parsing
	auto shouldStopParsing = [&]() -> bool {
		// Non-separator, non-digit character, stop parsing tokens
		return (p < e && *p != ':' && *p != ';' && !IsDigit(*p) && *p != '-');
	};
	
	// Lambda: Validate basic format requirements
	auto isValidFormat = [&]() -> bool {
		return !(count < 3 || count >= 8 || (values[0] != 38 && values[0] != 48));
	};
	
	// Lambda: Create RGB color
	auto createRGB = [&](int index) -> int {
		if(count <= 4)
			return 1;
		index += int(count == 6 && format != 3);
		int r = clamp(values[index++], 0, 255);
		int g = clamp(values[index++], 0, 255);
		int b = clamp(values[index], 0, 255);
		color = Color(r, g, b);
		return index;
	};
	
	// Lambda: Create CMY color
	auto createCMY = [&](int index) -> int {
		if(count <= 4)
			return 1;
		index += int(count == 6 && format != 3);
		double c = values[index++] * 0.01;
		double m = values[index++] * 0.01;
		double y = values[index] * 0.01;
		color = CmykColorf(c, m, y, 0.0);
		return index;
	};
	
	// Lambda: Create CMYK color
	auto createCMYK = [&](int index) -> int {
		if(!(count == 6 || count == 7))
			return 1;
		index += int(count > 6 && format != 3);
		double c = values[index++] * 0.01;
		double m = values[index++] * 0.01;
		double y = values[index++] * 0.01;
		double k = values[index] * 0.01;
		color = CmykColorf(c, m, y, k);
		return index;
	};
	
	// Lambda: Create indexed color
	auto createIndexed = [&](int index) -> int {
		int ix = clamp(values[index], 0, 255);
		color = Color::Special(ix);
		return index;
	};
	
	// Parse tokens separated by ':' or ';'
	while(p < e && count < 8) {
		if(!tryParseNumber())
			handleNonDigitToken();
		if(!skipSeparator() && shouldStopParsing())
				break;
	}
	
	if(!isValidFormat())
		return 1;
	
	which = values[0];
	palette = values[1];
	int index = 2;
	
	if(palette == 2 && count > 4) {						// True color (RGB)
		return createRGB(index);
	}
	else
	if(palette == 3 && count > 4) {						// True color (CMY)
		return createCMY(index);
	}
	else
	if(palette == 4 && (count == 6 || count == 7)) {	// True color (CMYK)
		return createCMYK(index);
	}
	else
	if(palette == 5) {									// Indexed (256-color)
		return createIndexed(index);
	}
	
	return 1;
}

void TerminalCtrl::ParseExtendedColors(VTCell& attrs, const Vector<String>& opcodes, int& index)
{
	// TODO: Optimixization.
	
	LTIMING("TerminalCtrl::SetISOColor");

	// Recognized color sequene formats:

	// ISO-8613-6:
	// SGR 38 : 2 : [IGNORED] : R : G : B
	// SGR 48 : 2 : [IGNORED] : R : G : B
	// SGR 38 : 3 : [IGNORED] : C : M : Y
	// SGR 48 : 3 : [IGNORED] : C : M : Y
	// SGR 38 : 4 : [IGNORED] : C : M : Y : K
	// SGR 48 : 4 : [IGNORED] : C : M : Y : K

	// SGR 38 : 2 : R : G : B
	// SGR 48 : 2 : R : G : B
	// SGR 38 : 3 : C : M : Y
	// SGR 48 : 3 : C : M : Y
	// SGR 38 : 4 : C : M : Y : K
	// SGR 48 : 4 : C : M : Y : K
	// SGR 38 : 5 : I
	// SGR 38 : 5 : I


	// ISO-8613-6 (mixed):
	// SGR 38 ; 2 : [IGNORED] : R : G : B
	// SGR 48 ; 2 : [IGNORED] : R : G : B
	// SGR 38 ; 3 : [IGNORED] : C : M : Y
	// SGR 48 ; 3 : [IGNORED] : C : M : Y
	// SGR 38 ; 4 : [IGNORED] : C : M : Y : K
	// SGR 48 ; 4 : [IGNORED] : C : M : Y : K

	// SGR 38 ; 2 : R : G : B
	// SGR 48 ; 2 : R : G : B
	// SGR 38 ; 3 : C : M : Y
	// SGR 48 ; 3 : C : M : Y
	// SGR 38 ; 4 : C : M : Y : K
	// SGR 48 ; 4 : C : M : Y : K
	// SGR 38 ; 5 : I
	// SGR 38 ; 5 : I

	// Legacy (used by xterm):
	// SGR 38 ; 2 ; R ; G ; B
	// SGR 48 ; 2 ; R ; G ; B
	// SGR 38 ; 5 ; I
	// SGR 38 ; 5 ; I

	Color color = Null;
	int which   = 0;
	int palette = 0;
	int remaining = opcodes.GetCount() - index;
		
	if(opcodes[index].Find(':') >= 0) {
		int format = 1;
		index += sParseExtendedColorFormat(color, which, palette, opcodes[index], format);
	}
	else
	if(remaining > 1) {
		if(opcodes[index + 1].Find(':') >= 0) {
			int format = 2;
			String s = opcodes[index] + ":" + opcodes[index + 1];
			index += sParseExtendedColorFormat(color, which, palette, s, format);
		}
		else {
			int format = 3;
			int count = min(remaining, 6);
			auto r = SubRange(opcodes, index, count);
			String s = Join((Vector<String>&) r, ":", false);
			index += sParseExtendedColorFormat(color, which, palette, s, format);
		}
	}
	else return;

	if(2 <= palette && palette <= 5) {
		switch(which) {
		case 38:
			attrs.ink = color;
			break;
		case 48:
			attrs.paper = color;
			break;
		}
	}
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

static int sCharFilterHashHex(int c)
{
	return IsXDigit(c) || c == '#' ? c : 0;
}

int ConvertHashColorSpec::Filter(int chr) const
{
	return sCharFilterHashHex(chr);
}

Value ConvertHashColorSpec::Scan(const Value& text) const
{
    const String& s = text;
    if(s.IsEmpty() || s[0] != '#')
        return ErrorValue(t_("Bad hash color text format"));
    
    const char *p = ~s;
    int len = 0;
    uint64 x = 0;
    
    // Fast filter and parse in single pass
    for(p++; *p; p++) {
        byte c = *p;
        if(c >= '0' && c <= '9') {
            if(len < 16)
				x = (x << 4) | (c - '0');
            len++;
        }
        else
        if((c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f')) {
            if(len < 16)
				x = (x << 4) | ((c & 0x0F) + 9);
            len++;
        }
        else
        if(c != ' ' && c != '\t') {
            break; // Stop at first non-hex, non-whitespace
        }
    }
    
    if(len != 3 && len != 6 && len != 9 && len != 12)
        return ErrorValue(t_("Bad hash color text format"));
    
    switch(len) {
    case 3:  // Hash3
        return Color(byte(x >> 4) & 0xF0, byte(x) & 0xF0, byte(x << 4));
    case 6:  // Hash6
        return Color(byte(x >> 16), byte(x >> 8), byte(x));
    case 9:  // Hash9
        return Color(byte(x >> 28), byte(x >> 16), byte(x >> 4));
    case 12: // Hash12
        return Color(byte(x >> 40), byte(x >> 24), byte(x >> 8));
    default:
        return ErrorValue(t_("Bad hash color text format"));
    }
}

Value ConvertHashColorSpec::Format(const Value& q) const
{
	if(q.Is<Color>()) {
		const Color& c = (const Color&) q;
		return Upp::Format("#%02x%02x%02x", c.GetR(), c.GetG(), c.GetB());
	}
	return Upp::ErrorValue(t_("Bad color value"));
}

int ConvertRgbColorSpec::Filter(int chr) const
{
	// 'B' and 'b' are also hex digits...
	return IsXDigit(chr) || findarg(chr, 'r', 'g', 'R', 'G',  ':', '/', ',') >= 0 ? chr : 0;
}

Value ConvertRgbColorSpec::Scan(const Value& text) const
{
		const String& s = text;
		const char *p = ~s;
		int components[4] = {0, 0, 0, 255};
		int count = 0;
		int radix = 10;
		bool is_rgba = false;

		auto IsDelimiter = [&](const char c) {
			return (c == ':' || c == '/' || c == ',' || c == '(' || c == ')');
		};
		
		// Skip "rgb"/"rgba" prefix if present
		if(s.GetLength() >= 3 && (*p == 'r' || *p == 'R')) {
			if((p[1] == 'g' || p[1] == 'G') && (p[2] == 'b' || p[2] == 'B')) {
				p += 3;
				is_rgba = (*p == 'a' || *p == 'A');
				if(is_rgba) p++;
				radix = 16;
				
				// Skip delimiter after prefix
				while(*p && IsDelimiter(*p))
					p++;
			}
		}

		// Parse components
		while(count < 4 && *p) {
			const char* start = p;
			while(*p && !IsDelimiter(*p))
				p++;
			
			if(p > start) {
				int val = ScanInt(String(start, p - start), nullptr, radix);
				if(IsNull(val))
					break;
				components[count++] = val;
			}
			
			// Skip delimiter
			while(*p && IsDelimiter(*p))
				p++;
		}

		if((count == 3) || (is_rgba && count == 4)) {
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
		const Color& c = (const Color&) q;
		return Upp::Format("rgb:%04x/%04x/%04x", c.GetR() * 257, c.GetG() * 257, c.GetB() * 257);
	}
	return Upp::ErrorValue(t_("Bad color value"));
}

int ConvertCmykColorSpec::Filter(int chr) const
{
	// 'C' and 'c' are also hex digits...
	return IsXDigit(chr) || findarg(chr, 'm', 'y', 'k', 'M', 'Y', 'K', ':', '/', ',', '.') >= 0 ? chr : 0;
}

Value ConvertCmykColorSpec::Scan(const Value& text) const
{
	auto Delimiters = [](int c) -> int
	{
		return c == ':' || c == '/';
	};
	
	Vector<String> h = Split(ToLower((const String&) text), Delimiters);
	int count = h.GetCount();
	if((count == 5 && h[0].IsEqual("cmyk"))		// cmyk : %f / %f / %f / %f
	|| (count == 4 && h[0].IsEqual("cmy"))) {	// cmy  : %f / %f / %f
		double c = ScanDouble(~h[1]);
		double m = ScanDouble(~h[2]);
		double y = ScanDouble(~h[3]);
		double k = count == 5 ? ScanDouble(~h[4]) : 0.0;
		if(!IsNull(c)
		&& !IsNull(m)
		&& !IsNull(y)
		&& !IsNull(k))
			return CmykColorf(c, m, y, k);
	}
	return Upp::ErrorValue(t_("Bad cmy/k color text format"));
}

Value ConvertCmykColorSpec::Format(const Value& q) const
{
	if(q.Is<Color>()) {
		const Color& r = (const Color&) q;
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