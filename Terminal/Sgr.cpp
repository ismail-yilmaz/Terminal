#include "Terminal.h"

#define LLOG(x)     // RLOG("TerminalCtrl (#" << this << "]: " << x)
#define LTIMING(x)	// RTIMING(x)

namespace Upp {

void TerminalCtrl::SelectGraphicsRendition(const AnsiParser::Sequence& seq)
{
	SetGraphicsRendition(cellattrs, seq.parameters);
	page->Attributes(cellattrs);	// This update is required for BCE (background color erase).
}

void TerminalCtrl::SetGraphicsRendition(VTCell& attrs, const Vector<String>& opcodes)
{
	LTIMING("TerminalCtrl::SetGraphicsRendition");

	for(int i = 0; i < opcodes.GetCount(); i++) {
		int opcode = ReadInt(opcodes[i], 0);
		switch(opcode) {
		case 0:
			attrs.Reset();
			break;
		case 1:
			attrs.Bold().Faint(false);
			break;
		case 2:
			attrs.Faint();
			break;
		case 3:
			attrs.Italic();
			break;
		case 4:
			//Check for extended underline sub-parameters (e.g., "4:3")
			ParseExtendedUnderlines(attrs, opcodes, i);
			break;
		case 5:
		case 6:
			attrs.Blink();
			break;
		case 7:
			attrs.Invert();
			break;
		case 8:
			attrs.Conceal();
			break;
		case 9:
			attrs.Strikeout();
			break;
		case 14:
			 //ACS on
			break;
		case 15:
			 //ACS off
			break;
		case 21:
			// Legacy double underline
			attrs.SetUnderlineStyle(VTCell::UNDERLINE_DOUBLE);
			break;
		case 22:
			attrs.Faint(false).Bold(false);
			break;
		case 23:
			attrs.Italic(false);
			break;
		case 24:
			attrs.Underline(false);
			break;
		case 25:
		case 26:
			attrs.Blink(false);
			break;
		case 27:
			attrs.Invert(false);
			break;
		case 28:
			attrs.Conceal(false);
			break;
		case 29:
			attrs.Strikeout(false);
			break;
		case 38:
			ParseExtendedColors(attrs, opcodes, i);
			break;
		case 39:
			attrs.ink = Null;
			break;
		case 48:
			ParseExtendedColors(attrs, opcodes, i);
			break;
		case 49:
			attrs.paper = Null;
			break;
		case 53:
			attrs.Overline();
			break;
		case 55:
			attrs.Overline(false);
			break;
		default:
			if(opcode >= 30 && opcode <= 37)
				attrs.ink = Color::Special(opcode - 30);
			else
			if(opcode >= 40 && opcode <= 47)
				attrs.paper = Color::Special(opcode - 40);
			else
			if(opcode >= 90 && opcode <= 97)
				attrs.ink = Color::Special(opcode - 82);
			else
			if(opcode >= 100 && opcode <= 107)
				attrs.paper = Color::Special(opcode - 92);
			else
				LLOG("Unhandled SGR code: " << opcode);
			break;
		}
	}
}

void TerminalCtrl::InvertGraphicsRendition(VTCell& attrs, const Vector<String>& opcodes)
{
    for(const auto& opcode : opcodes) {
        switch(ReadInt(opcode, 0)) {
        case 0:
            attrs.Reset();
            break;
        case 1:
            attrs.Bold(!attrs.IsBold());
            break;
        case 3:
            attrs.Italic(!attrs.IsItalic());
            break;
        case 4:
            attrs.Underline(!attrs.IsUnderlined());
            break;
        case 5:
            attrs.Blink(!attrs.IsBlinking());
            break;
        case 7:
            attrs.Invert(!attrs.IsInverted());
            break;
        case 8:
            attrs.Conceal(!attrs.IsConcealed());
            break;
        case 9:
            attrs.Strikeout(!attrs.IsStrikeout());
            break;
        case 53:
            attrs.Overline(!attrs.IsOverlined());
            break;
        default:
            break;
        }
    }
}

String TerminalCtrl::GetGraphicsRenditionOpcodes(const VTCell& attrs)
{
	Vector<String> v;

	v.Add("0");

	if(attrs.IsBold())
		v.Add("1");
	if(attrs.IsItalic())
		v.Add("3");
	if(attrs.IsUnderlined())
		v.Add("4");
	if(attrs.IsBlinking())
		v.Add("5");
	if(attrs.IsInverted())
		v.Add("7");
	if(attrs.IsConcealed())
		v.Add("8");
	if(attrs.IsStrikeout())
		v.Add("9");
	if(attrs.IsOverlined())
		v.Add("53");

	if(attrs.ink.GetSpecial() == -1)    // Direct color (24 bit)
		v.Add(Format("38:2::%ld:%ld:%ld",
				attrs.ink.GetR(),
				attrs.ink.GetG(),
				attrs.ink.GetB()));

	if(attrs.paper.GetSpecial() == -1)  // Direct color (24 bit)
		v.Add(Format("48:2::%ld:%ld:%ld",
				attrs.paper.GetR(),
				attrs.paper.GetG(),
				attrs.paper.GetB()));

	return Join(v, ";", true);

}

void TerminalCtrl::ParseExtendedUnderlines(VTCell& attrs, const Vector<String>& opcodes, int& index)
{
	int i = opcodes[index].Find(":");
	if(i < 0) {
		attrs.Underline();
		return;
	}
	
	switch(ReadInt(~opcodes[index] + i + 1, 0)) {
	case 2:
		attrs.SetUnderlineStyle(VTCell::UNDERLINE_DOUBLE);
		break;
	case 3:
		attrs.SetUnderlineStyle(VTCell::UNDERLINE_CURLY);
		break;
	default:
		attrs.SetUnderlineStyle(VTCell::UNDERLINE_SINGLE);
		break;
	}
}

}
