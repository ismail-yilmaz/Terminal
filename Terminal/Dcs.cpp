#include "Terminal.h"

#define LLOG(x)     // RLOG("TerminalCtrl (#" << this << "]: " << x)
#define LTIMING(x)	// RTIMING(x)

namespace Upp {

void TerminalCtrl::ParseDeviceControlStrings(const AnsiParser::Sequence& seq)
{
	LLOG(seq);

	const CbFunction *p = FindFunctionPtr(seq);
	if(p) p->c(*this, seq);
}

void TerminalCtrl::SetUserDefinedKeys(const AnsiParser::Sequence& seq)
{
	if(!userdefinedkeys || userdefinedkeyslocked)
		return;

	bool  clear = seq.GetInt(1) == 0;
	bool  lock  = seq.GetInt(2) == 0;
	dword modifiers = seq.GetInt(3, 0);

	Vector<String> vudk = Split(seq.payload, ';', false);

	if(clear)
		udk.Clear();

	String key, val;
	for(const String& pair : vudk)
		if(SplitTo(pair, '/', key, val)) {
			dword k = StrInt(key);
			if(findarg(modifiers, 3, 4) >= 0 && (!pcstylefunctionkeys || k > 34))
				continue;
			k |= decode(modifiers,
					0, K_SHIFT,
					2, K_SHIFT,
					3, K_ALT,
					4, K_SHIFT|K_ALT, 0);
			LLOG("UDKey: " << key << ", modifiers: " << modifiers << ", value (in hex): " << val);
			udk.Put(k, ScanHexString(val));
		}

	if(lock)
		LockUDK();
}

bool TerminalCtrl::GetUDKString(dword key, String& val)
{
	if(!IsLevel2() || udk.IsEmpty())
		return false;

	int i = udk.Find(key);
	if(i >= 0)
		val = udk[i];

	return i >= 0 && val.GetCount();
}

void TerminalCtrl::ReportControlFunctionSettings(const AnsiParser::Sequence& seq)
{
	// TODO
	String reply;// = "0$r";	// Invalid request (unhandled sequence)

	if(seq.payload.IsEqual("r")) {					// DECSTBM
		Rect margins = page->GetMargins();
		reply = Format("%d`$r%d;%d", 1, margins.top, margins.bottom);
	}
	else
	if(IsLevel4() && seq.payload.IsEqual("s")) {	// DECSLRM
		Rect margins = page->GetMargins();
		reply = Format("%d`$r%d;%d", 1, margins.left, margins.right);
	}
	else
	if(seq.payload.IsEqual("m")) {					// SGR
		reply = Format("%d`$r%s", 1, GetGraphicsRenditionOpcodes(cellattrs));
	}
	else
	if(seq.payload.IsEqual("\"p")) {				// DECSCL
		int level = decode(clevel, LEVEL_4, 64, LEVEL_3, 63, LEVEL_2, 62, 61);
		reply = Format("%d`$r%d;%d", 1, level, Is8BitMode() ? 0 : 1);
	}
	else
	if(IsLevel4() && seq.payload.IsEqual(" q")) {	// DECSCUSR
		int style = decode(caret.GetStyle(), Caret::BEAM, 6, Caret::UNDERLINE, 4, 2);
		reply = Format("%d`$r%d", 1, style - (int) caret.IsBlinking());
	}
	else
	if(seq.payload.IsEqual("\"q")) {				// DECSCA
		reply = Format("%d`$r%d", 1, (int) cellattrs.HasDECProtection());
	}
	else
	if(IsLevel4() && seq.payload.IsEqual("*x")) {	// DECSACE
		reply = Format("%d`$r%d", 1, streamfill ? 1 : 2);
	}
	else
	if(seq.payload.IsEqual("t")) {					// DECSLPP
		reply = Format("%d`$r%d`t", 1, page->GetSize().cy);
	}
	else
	if(seq.payload.IsEqual("$|")) {					// DECSCPP
		reply = Format("%d`$r%d", 1, page->GetSize().cx);
	}
	else
	if(seq.payload.IsEqual("*|")) {					// DECSNLS
		reply = Format("%d`$r%d", 1, page->GetSize().cy);
	}

	if(!IsNull(reply)) {
		reply << seq.payload;
	}
	else {
		reply = "0$r";	// Invalid request (unhandled sequence)
		LLOG("Unhandled report request. Token: %s " << seq.payload);
	}

	PutDCS(reply);
}

void TerminalCtrl::RestorePresentationState(const AnsiParser::Sequence& seq)
{
	int which = seq.GetInt(1, 0);

	if(which == 1) {	// DECRSPS/DECCIR
		Vector<String> cr = Split(seq.payload, ';');

		if(cr.IsEmpty())
			return;

		auto GetInt = [&cr](int n) -> int
		{
			String s = cr.Get(--n, Null);
			return (s[0] >= 0x40 && s[0] <= 0xFF)
					? (int) s[0]
					: Nvl(StrInt(s), 0);
		};

		auto GetStr = [&cr](int n) -> String
		{
			return cr.Get(--n, Null);
		};

		auto GetChrset = [=](int i) -> byte
		{
			// TODO: This can be more precise...
			return decode(i,
					'0', CHARSET_DEC_DCS,
					'>', CHARSET_DEC_TCS,
					'<', CHARSET_DEC_MCS,
					'A', CHARSET_ISO8859_1,
					'G', CHARSET_UNICODE, CHARSET_TOASCII);
		};

		Point pt;
		pt.y      = GetInt(1);
		pt.x      = GetInt(2);
		int sgr   = GetInt(4);
		int attrs = GetInt(5);
		int flags = GetInt(6);
		int gl    = GetInt(7);
		int gr    = GetInt(8);
		//int sz	  = GetInt(9);
		String gs = GetStr(10);

		cellattrs.Bold(sgr & 0x01);
		cellattrs.Blink(sgr & 0x04);
		cellattrs.Invert(sgr & 0x08);
		cellattrs.Underline(sgr & 0x02);
		cellattrs.ProtectDEC(attrs & 0x01);

		DECom(flags & 0x01);

		page->Attributes(cellattrs).MoveTo(pt).SetEol(flags & 0x8);

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
	if(which == 2) {	// DECRSPS/DECTABSR
		Vector<String> stab = Split(seq.payload, '/');

		page->ClearTabs();

		for(const auto& s : stab) {
			int pos = StrInt(s);
			if(pos > 0)
				page->SetTabAt(pos, true);
		}
	}
}

void TerminalCtrl::ParseSixelGraphics(const AnsiParser::Sequence& seq)
{
	if(!sixelimages)
		return;

//	int  ratio  = decode(seq.GetInt(1, 1), 5, 2, 6, 2, 3, 3, 4, 3, 2, 5, 1);
//	int  grid   = seq.GetInt(3, 0); // Omitted.

	cellattrs.Hyperlink(false);

	ImageString imgs;
	imgs.FmtSixel().Transparent(seq.GetInt(2, 0) != 2).data = seq.payload;

	if(!modes[XTSPREG])
		imgs.palette = &sixelpalette;

	RenderImage(imgs, !modes[DECSDM]);
}

void TerminalCtrl::ReportXTermCapabilities(const AnsiParser::Sequence& seq)
{
	// XTGETTCAP: Values are subject to change...

	LTIMING("ReportXTermCapabilities");

	struct Capabilities {
		const char *key;
		const char *value;
		byte minlevel;
		byte maxlevel;
	};

	static constexpr Capabilities capabilities[] = {
		// Core Identifiers
		{ "TN",       "xterm-256color", LEVEL_1, LEVEL_4 },
		{ "Co",       "256",            LEVEL_1, LEVEL_4 },
		{ "RGB",      "8",              LEVEL_1, LEVEL_4 },
		{ "Tc",       nullptr,          LEVEL_1, LEVEL_4 },
		{ "Sync",     nullptr,          LEVEL_1, LEVEL_4 },

		// Terminfo Boolean Flags (Mapped to nullptr)
		{ "am",       nullptr,          LEVEL_1, LEVEL_4 },
		{ "mir",      nullptr,          LEVEL_1, LEVEL_4 },
		{ "msgr",     nullptr,          LEVEL_1, LEVEL_4 },
		{ "xenl",     nullptr,          LEVEL_1, LEVEL_4 },
		{ "bce",      nullptr,          LEVEL_1, LEVEL_4 },
		{ "ccc",      nullptr,          LEVEL_1, LEVEL_4 },

		// Terminfo Numeric Constraints
		{ "cols",     "80",             LEVEL_0, LEVEL_4 },
		{ "lines",    "24",             LEVEL_0, LEVEL_4 },
		{ "colors",   "256",            LEVEL_0, LEVEL_4 },
		{ "pairs",    "32767",          LEVEL_1, LEVEL_4 },

		// Keyboard: Standard & Cursor Keys
		{ "kbs",      "^?",             LEVEL_0, LEVEL_4 },
		{ "kcuu1",    "\\E[A",          LEVEL_0, LEVEL_4 },
		{ "kcud1",    "\\E[B",          LEVEL_0, LEVEL_4 },
		{ "kcuf1",    "\\E[C",          LEVEL_0, LEVEL_4 },
		{ "kcub1",    "\\E[D",          LEVEL_0, LEVEL_4 },

		// Keyboard: Edit Pad Keys
		{ "kich1",    "\\E[2~",         LEVEL_2, LEVEL_4 },
		{ "kdch1",    "\\E[3~",         LEVEL_2, LEVEL_4 },
		{ "khome",    "\\E[H",          LEVEL_2, LEVEL_4 },
		{ "kend",     "\\E[F",          LEVEL_2, LEVEL_4 },
		{ "kpp",      "\\E[5~",         LEVEL_2, LEVEL_4 },
		{ "knp",      "\\E[6~",         LEVEL_2, LEVEL_4 },

		// Keyboard: Programmable & Function Keys
		{ "kf1",      "\\EOP",          LEVEL_0, LEVEL_4 },
		{ "kf2",      "\\EOQ",          LEVEL_0, LEVEL_4 },
		{ "kf3",      "\\EOR",          LEVEL_0, LEVEL_4 },
		{ "kf4",      "\\EOS",          LEVEL_0, LEVEL_4 },
		{ "kf5",      "\\E[15~",        LEVEL_2, LEVEL_4 },
		{ "kf6",      "\\E[17~",        LEVEL_2, LEVEL_4 },
		{ "kf7",      "\\E[18~",        LEVEL_2, LEVEL_4 },
		{ "kf8",      "\\E[19~",        LEVEL_2, LEVEL_4 },
		{ "kf9",      "\\E[20~",        LEVEL_2, LEVEL_4 },
		{ "kf10",     "\\E[21~",        LEVEL_2, LEVEL_4 },
		{ "kf11",     "\\E[23~",        LEVEL_2, LEVEL_4 },
		{ "kf12",     "\\E[24~",        LEVEL_2, LEVEL_4 },
		{ "kf13",     "\\E[25~",        LEVEL_2, LEVEL_4 },
		{ "kf14",     "\\E[26~",        LEVEL_2, LEVEL_4 },
		{ "kf15",     "\\E[28~",        LEVEL_2, LEVEL_4 },
		{ "kf16",     "\\E[29~",        LEVEL_2, LEVEL_4 },
		{ "kf17",     "\\E[31~",        LEVEL_2, LEVEL_4 },
		{ "kf18",     "\\E[32~",        LEVEL_2, LEVEL_4 },
		{ "kf19",     "\\E[33~",        LEVEL_2, LEVEL_4 },
		{ "kf20",     "\\E[34~",        LEVEL_2, LEVEL_4 },

		// Single-Byte & C1 Controls
		{ "bel",      "^G",             LEVEL_0, LEVEL_4 },
		{ "cub1",     "\\b",            LEVEL_0, LEVEL_4 },
		{ "ht",       "\\t",            LEVEL_0, LEVEL_4 },
		{ "cud1",     "\\n",            LEVEL_0, LEVEL_4 },
		{ "cr",       "\\r",            LEVEL_0, LEVEL_4 },
		{ "ind",      "\\ED",           LEVEL_1, LEVEL_4 },
		{ "nel",      "\\EE",           LEVEL_1, LEVEL_4 },
		{ "hts",      "\\EH",           LEVEL_1, LEVEL_4 },
		{ "ri",       "\\EM",           LEVEL_1, LEVEL_4 },

		// Cursor Movement (CSI)
		{ "cuu",      "\\E[%p1%dA",     LEVEL_1, LEVEL_4 },
		{ "cuu1",     "\\E[A",          LEVEL_1, LEVEL_4 },
		{ "cud",      "\\E[%p1%dB",     LEVEL_1, LEVEL_4 },
		{ "cuf",      "\\E[%p1%dC",     LEVEL_1, LEVEL_4 },
		{ "cuf1",     "\\E[C",          LEVEL_1, LEVEL_4 },
		{ "cub",      "\\E[%p1%dD",     LEVEL_1, LEVEL_4 },
		{ "cup",      "\\E[%i%p1%d;%p2%dH", LEVEL_1, LEVEL_4 },
		{ "home",     "\\E[H",          LEVEL_1, LEVEL_4 },
		{ "vpa",      "\\E[%i%p1%dd",   LEVEL_4, LEVEL_4 },
		{ "hpa",      "\\E[%i%p1%d`",   LEVEL_1, LEVEL_4 },
		{ "cbt",      "\\E[%p1%dZ",     LEVEL_4, LEVEL_4 },

		// Editing, Clearing, Scrolling (CSI)
		{ "clear",    "\\E[H\\E[2J",    LEVEL_1, LEVEL_4 },
		{ "ed",       "\\E[J",          LEVEL_1, LEVEL_4 },
		{ "el",       "\\E[K",          LEVEL_1, LEVEL_4 },
		{ "el1",      "\\E[1K",         LEVEL_1, LEVEL_4 },
		{ "il",       "\\E[%p1%dL",     LEVEL_1, LEVEL_4 },
		{ "il1",      "\\E[L",          LEVEL_1, LEVEL_4 },
		{ "dl",       "\\E[%p1%dM",     LEVEL_1, LEVEL_4 },
		{ "dl1",      "\\E[M",          LEVEL_1, LEVEL_4 },
		{ "dch",      "\\E[%p1%dP",     LEVEL_1, LEVEL_4 },
		{ "dch1",     "\\E[P",          LEVEL_1, LEVEL_4 },
		{ "ich",      "\\E[%p1%d@",     LEVEL_2, LEVEL_4 },
		{ "ech",      "\\E[%p1%dX",     LEVEL_2, LEVEL_4 },
		{ "indn",     "\\E[%p1%dS",     LEVEL_3, LEVEL_4 },
		{ "rin",      "\\E[%p1%dT",     LEVEL_3, LEVEL_4 },
		{ "csr",      "\\E[%i%p1%d;%p2%dr", LEVEL_1, LEVEL_4 },

		// Terminal Modes & State
		{ "sc",       "\\E7",           LEVEL_1, LEVEL_4 },
		{ "rc",       "\\E8",           LEVEL_1, LEVEL_4 },
		{ "rs1",      "\\Ec",           LEVEL_1, LEVEL_4 },
		{ "rs2",      "\\E[!p",         LEVEL_2, LEVEL_4 },
		{ "smam",     "\\E[?7h",        LEVEL_1, LEVEL_4 },
		{ "rmam",     "\\E[?7l",        LEVEL_1, LEVEL_4 },
		{ "smir",     "\\E[4h",         LEVEL_1, LEVEL_4 },
		{ "rmir",     "\\E[4l",         LEVEL_1, LEVEL_4 },
		{ "civis",    "\\E[?25l",       LEVEL_2, LEVEL_4 },
		{ "cnorm",    "\\E[?25h",       LEVEL_2, LEVEL_4 },
		{ "cvvis",    "\\E[?25h",       LEVEL_2, LEVEL_4 },
		{ "smcup",    "\\E[?1049h",     LEVEL_1, LEVEL_4 },
		{ "rmcup",    "\\E[?1049l",     LEVEL_1, LEVEL_4 },
		{ "smkx",     "\\E[?1h\\E=",    LEVEL_0, LEVEL_4 },
		{ "rmkx",     "\\E[?1l\\E>",    LEVEL_0, LEVEL_4 },
		{ "tbc",      "\\E[3g",         LEVEL_1, LEVEL_4 },

		// Styling & Colors (SGR)
		{ "sgr0",     "\\E[m",          LEVEL_1, LEVEL_4 },
		{ "bold",     "\\E[1m",         LEVEL_1, LEVEL_4 },
		{ "dim",      "\\E[2m",         LEVEL_1, LEVEL_4 },
		{ "sitm",     "\\E[3m",         LEVEL_1, LEVEL_4 },
		{ "ritm",     "\\E[23m",        LEVEL_1, LEVEL_4 },
		{ "smul",     "\\E[4m",         LEVEL_1, LEVEL_4 },
		{ "rmul",     "\\E[24m",        LEVEL_1, LEVEL_4 },
		{ "blink",    "\\E[5m",         LEVEL_1, LEVEL_4 },
		{ "rev",      "\\E[7m",         LEVEL_1, LEVEL_4 },
		{ "invis",    "\\E[8m",         LEVEL_1, LEVEL_4 },
		{ "smxx",     "\\E[9m",         LEVEL_1, LEVEL_4 },
		{ "rmxx",     "\\E[29m",        LEVEL_1, LEVEL_4 },
		{ "setaf",    "\\E[%?%p1%{8}%<%t3%p1%d%e%p1%{16}%<%t9%p1%{8}%-%d%e38;5;%p1%d%;m", LEVEL_1, LEVEL_4 },
		{ "setab",    "\\E[%?%p1%{8}%<%t4%p1%d%e%p1%{16}%<%t10%p1%{8}%-%d%e48;5;%p1%d%;m", LEVEL_1, LEVEL_4 },
		{ "op",       "\\E[39;49m",     LEVEL_1, LEVEL_4 },
		{ "Su",       nullptr,          LEVEL_1, LEVEL_4 },
	};

	Vector<String> out, err;

	for(const String& query : Split(seq.payload, ';')) {
		String decoded = HexDecode(query);

		// Optionals
		if(decoded == "Ms") {
			if(IsClipboardAccessPermitted()) out.Add(query);
			else err.Add(query);
			continue;
		}
		if(decoded == "Hls") {
			if(HasHyperlinks()) out.Add(query);
			else err.Add(query);
			continue;
		}

		bool found = false;
		for(const auto& q : capabilities) {
			if(decoded == q.key) {
				found = true;
				if(q.minlevel <= clevel && clevel <= q.maxlevel) {
					if(String& s = out.Add(query); q.value) {
						s << "=" << HexEncode(q.value);
					}
				}
				else
					err.Add(query);
				break;
			}
		}
		if(!found)
			err.Add(query);
	}

	if(out.GetCount())
		PutDCS("1+r" + Join(out, ";"));

	if(err.GetCount())
		PutDCS("0+r" + Join(err, ";"));
}

}