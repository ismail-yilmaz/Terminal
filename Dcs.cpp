#include "Console.h"


#define LLOG(x)	// RLOG("Console: " << x)

namespace Upp {

void Console::ParseDeviceControlStrings(const VTInStream::Sequence& seq)
{
	LLOG(Format("DCS: %c, %[N/A]~s, %s, [%s]",
			seq.opcode, seq.intermediate, seq.parameters.ToString(), seq.payload));

	bool refresh;
	switch(FindSequenceId(VTInStream::Sequence::DCS, clevel, seq, refresh)) {
	case SequenceId::DECRQSS:
		ReportControlFunctionSettings(seq);
		break;
	case SequenceId::DECRSPS:
		RestorePresentationState(seq);
		break;
	case SequenceId::DECUDK:
		SetUserDefinedKeys(seq);
		break;
	default:
		LLOG("Unhandled device control string.");
		break;
	}
	if(refresh)
		RefreshPage();
}

void Console::SetUserDefinedKeys(const VTInStream::Sequence& seq)
{
	if(!IsUDKEnabled() || IsUDKLocked())
		return;

	bool clear = seq.GetInt(1) == 0;
	bool lock  = seq.GetInt(2) == 0;
	int modifier = seq.GetInt(3, 0);

	Vector<String> vudk = Split(seq.payload, ';', false);

	if(clear)
		udk.Clear();

	String key, val;
	for(const String& pair : vudk)
		if(SplitTo(pair, '/', key, val))
			udk.Put(StrInt(key), ScanHexString(val));

	if(lock)
		LockUDK();
}

bool Console::GetUDKString(byte key, String& val)
{
	if(!IsLevel2() || udk.IsEmpty())
		return false;

	int i = udk.Find(key);
	if(i >= 0)
		val = udk[i];

	return i >= 0;
}

void Console::ReportControlFunctionSettings(const VTInStream::Sequence& seq)
{
	// TODO

	String reply = "0$r";				// Invalid request (unhandled sequence)

	if(seq.payload.IsEqual("r")) {		// DECSTBM
		Rect margins = page->GetMargins();
		reply = Format("%d`$r%d`;%d`r", 1, margins.top, margins.bottom);
	}
	else
	if(seq.payload.IsEqual("s")) {		// DECSLRM
		Rect margins = page->GetMargins();
		reply = Format("%d`$r%d`;%d`s", 1, margins.left, margins.right);
	}
	else
	if(seq.payload.IsEqual("m")) {		// SGR
		reply = Format("%d`$r%s`m", 1, GetGraphicsRenditionOpcodes(cellattrs));
	}
	else
	if(seq.payload.IsEqual("\"p")) {	// DECSCL
		int level = 62;
		switch(clevel) {
		case LEVEL_0:
		case LEVEL_1:
			level = 61;
			break;
		case LEVEL_2:
			level = 62;
			break;
		case LEVEL_3:
			level = 63;
			break;
		case LEVEL_4:
			level = 64;
			break;
		}
		reply = Format("%d`$r%d`;%[1:2;1]s`\"p", 1, level, Is8BitsMode());
	}
	else
	if(seq.payload.IsEqual(" q")) {		// DECSCUSR
		int style = 0;
		switch(caret.GetStyle()) {
		case Caret::BLOCK:
			style = caret.IsBlinking() ? 1 : 2;
			break;
		 case Caret::UNDERLINE:
			style = caret.IsBlinking() ? 3 : 4;
			break;
		case Caret::BEAM:
			style = caret.IsBlinking() ? 5 : 6;
			break;
		}
		reply = Format("%d`$r%d q", 1, style);
	}
	else
	if(seq.payload.IsEqual("\"q")) {	// DECSCA
		reply = Format("%d`$r%[1:1;0]s`\"q", 1, cellattrs.IsProtected());
	}
	else
	if(seq.payload.IsEqual("*x")) {		// DECSACE
		reply = Format("%d`$r%[1:1;2]s`*x", 1, streamfill);
	}
	else
	if(seq.payload.IsEqual("t")) {		// DECSLPP
		reply = Format("%d`$r%d`t", 1, page->GetSize().cy);
	}
	else
	if(seq.payload.IsEqual("$|")) {		// DECSCPP
		reply = Format("%d`$r%d`$|", 1, page->GetSize().cx);
	}
	else
	if(seq.payload.IsEqual("*|")) {		// DECSNLS
		reply = Format("%d`$r%d`*|", 1, page->GetSize().cy);
	}
	PutDCS(reply);
}

void Console::RestorePresentationState(const VTInStream::Sequence& seq)
{
	int which = seq.GetInt(1, 0);
	
	if(which == 1) {	// DECCIR
		Vector<String> cr = Split(seq.payload, ';');

		if(cr.IsEmpty())
			return;

		auto GetInt = [&cr](int n) -> int
		{
			bool b = cr.GetCount() < max(1, n);
			if(b) return 0;
			String s = cr[--n];
			if(64 <= s[0] && s[0] <= 65536)
				return s[0] & 0xFFFF;
			else
				return Nvl(StrInt(s), 0);
		};

		auto GetStr = [&cr](int n) -> String
		{
			bool b = cr.GetCount() < max(1, n);
			return b ? Null : cr[--n];
		};

		auto GetChrset = [=](int i) -> byte
		{
			// TODO: This can be more precise...
			if(i == '0') return CHARSET_DEC_DCS;
			if(i == '>') return CHARSET_DEC_TCS;
			if(i == '<') return CHARSET_DEC_MCS;
			if(i == 'A') return CHARSET_ISO8859_1;
			return CHARSET_TOASCII;
		};
		
		Point pt;
		pt.y      = GetInt(1);
		pt.x      = GetInt(2);
		int sgr   = GetInt(4);
		int attrs = GetInt(5);
		int flags = GetInt(6);
		int gl    = GetInt(7);
		int gr    = GetInt(8);
		int sz	  = GetInt(9);
		String gs = GetStr(10);
		
		DECom(flags & 0x01);
		DECawm(flags & 0x08);
		
		cellattrs.Bold(sgr & 0x01);
		cellattrs.Underline(sgr & 0x02);
		cellattrs.Blink(sgr & 0x04);
		cellattrs.Invert(sgr & 0x08);
		cellattrs.Protect(attrs & 0x01);

		page->Attributes(cellattrs);

		page->MoveTo(pt);
		
		if(flags & 0x02)
			gsets.SS(0x8E);
		else
		if(flags & 0x04)
			gsets.SS(0x8F);
		
		if(IsNull(gs))
			return;
		
		for(int i = 0; i < gs.GetLength(); i++) {
			switch(i) {
			case 0:
				gsets.G0(GetChrset(gs[i]));
				break;
			case 1:
				gsets.G1(GetChrset(gs[i]));
				break;
			case 2:
				gsets.G2(GetChrset(gs[i]));
				break;
			case 3:
				gsets.G3(GetChrset(gs[i]));
				break;
			}
		}
		
		switch(gl) {
		case 0:
			gsets.G0toGL();
			break;
		case 1:
			gsets.G1toGL();
			break;
		case 2:
			gsets.G2toGL();
			break;
		case 3:
			gsets.G3toGL();
			break;
		}

		switch(gr) {
		case 1:
			gsets.G1toGR();
			break;
		case 2:
			gsets.G2toGR();
			break;
		case 3:
			gsets.G3toGR();
			break;
		}
				
	}
	else
	if(which == 2) {	// DECTABSR
		Vector<String> stab = Split(seq.payload, '/');

		page->ClearTabs();

		for(const auto& s : stab) {
			int pos = StrInt(s);
			if(pos > 0)
				page->SetTabAt(pos, true);
		}
			
	}
}
}