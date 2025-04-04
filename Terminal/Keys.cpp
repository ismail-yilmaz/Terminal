#include "Terminal.h"

#define LLOG(x)     // RLOG("TerminalCtrl (#" << this << "]: " << x)
#define LTIMING(x)	// RTIMING(x)

namespace Upp {

using namespace TerminalCtrlKeys;

void TerminalCtrl::ProcessSelectorKey(dword key, int count)
{
	if(key & K_KEYUP)
		return;
	
	Size psz = GetPageSize();
	int cy = GetPage().GetLineCount() - 1;
	
	dword k = 0;

	if(Match(AK_SELECTOR_EXIT, key) || key  == K_ESCAPE) {
		EndSelectorMode();
		return;
	}
	else
	if(Match(AK_SELECTOR_START, key)) {
		selecting = true;
		if(seltype == SEL_NONE) // Initialize
			seltype = SEL_TEXT;
	}
	else
	if(Match(AK_SELECTOR_COPY, key)) {
		if(IsSelection()) Copy();
		return;
	}
	else
	if(Match(AK_SELECTOR_CANCEL, key)) {
		selecting = false;
	}
	else
	if(Match(AK_SELECTOR_TEXTMODE, key)) {
		seltype = SEL_TEXT;
	}
	else
	if(Match(AK_SELECTOR_LINEMODE, key)) {
		seltype = SEL_LINE;
	}
	else
	if(Match(AK_SELECTOR_RECTMODE, key)) {
		seltype = SEL_RECT;
	}
	else
	if(Match(AK_SELECTOR_WORDMODE, key)) {
		seltype = SEL_WORD;
	}
	else
	if(Match(AK_SELECTOR_UP, key)) {
		cursor.y = max(0, cursor.y - 1);
	}
	else
	if(Match(AK_SELECTOR_DOWN, key)) {
		cursor.y = min(cy, cursor.y + 1);
	}
	else
	if(Match(AK_SELECTOR_LEFT, key)) {
		cursor.x = max(0, cursor.x - 1);
		k = K_LEFT;
	}
	else
	if(Match(AK_SELECTOR_RIGHT, key)) {
		cursor.x = min(psz.cx, cursor.x + 1);
		k = K_RIGHT;
	}
	else
	if(Match(AK_SELECTOR_LEFTMOST, key)) {
		cursor.x = 0;
	}
	else
	if(Match(AK_SELECTOR_RIGHTMOST, key)) {
		cursor.x = psz.cx;
	}
	else
	if(Match(AK_SELECTOR_HOME, key)) {
		cursor = { 0, 0 };
	}
	else
	if(Match(AK_SELECTOR_END, key)) {
		cursor = { psz.cx, cy };
	}
	else
	if(Match(AK_SELECTOR_PAGEUP, key)) {
		cursor.y = max(0, cursor.y - psz.cy);
	}
	else
	if(Match(AK_SELECTOR_PAGEDOWN, key)) {
		cursor.y = min(cy, cursor.y + psz.cy);
	}
	else
		return;

	if(!selecting)
		anchor = selpos = cursor;
	else
	if(seltype == SEL_WORD) {
		while(!GetWordSelection(cursor, anchor, selpos)) { // Skip text and space.
			if(k == K_LEFT) {
				cursor.x = max(cursor.x - 1, 0);
			}
			else
			if(k == K_RIGHT) {
				cursor.x = min(cursor.x + 1, psz.cx);
			}
			else
				break;
			if(cursor.x == 0 || cursor.x == psz.cx)
				break;
		}
		if(k == K_LEFT)
			cursor = anchor;
		else
		if(k == K_RIGHT)
			cursor = selpos;
	}
	else
	if(seltype == SEL_LINE) {
		while(!GetLineSelection(cursor, anchor, selpos)) {
			if(k == K_UP) {
				cursor.y = max(cursor.y - 1, 0);
			}
			else
			if(k == K_DOWN) {
				cursor.y = min(cursor.y + 1, psz.cy);
			}
			else
				break;
			if(cursor.y == 0 || cursor.y == psz.cy)
				break;
		}
		if(k == K_UP)
			cursor = anchor;
		else
		if(k == K_DOWN)
			cursor = selpos;

	}
	else
		selpos = cursor;

	Goto(cursor.y);

	if(IsSelection())
		SetSelection(anchor, selpos, seltype);
	
	PlaceCaret();
	Refresh();
}

bool TerminalCtrl::ProcessKey(dword key, bool ctrlkey, bool altkey, int count)
{
	if((key = EncodeCodepoint(key, gsets.Get(key, IsLevel2()))) == DEFAULTCHAR)
		return false;

	if(ctrlkey)
		key = ToAscii(key) & 0x1F;

	if(key < 0x80 && altkey && metakeyflags != MKEY_NONE) {
		if(metakeyflags & MKEY_SHIFT)
			key |= 0x80;
		if((metakeyflags & MKEY_ESCAPE) || modes[XTALTESCM])
			PutESC(key, count);
		else
			Put(key, count);
	}
	else
		Put(key, count);

	return true;
}

bool TerminalCtrl::ProcessVTStyleFunctionKey(const FunctionKey& k, dword modkeys, int count)
{
	if(k.type == FunctionKey::Cursor) {
		modes[DECCKM] ? PutSS3(k.code, count) : PutCSI(k.code, count);
		return true;
	}
	else
	if(k.type == FunctionKey::EditPad) {
		PutCSI(String(k.code) << "~", count);
		return true;
	}
	else
	if(k.type == FunctionKey::NumPad && modes[DECKPAM]) {
		PutSS3(k.code, count);
		return true;
	}
	else
	if(k.type == FunctionKey::Programmable) {
		PutSS3(k.code, count);
		return true;
	}
	else
	if(k.type == FunctionKey::Function) {
		PutCSI(String(k.code) << "~", count);
		return true;
	}

	return false;
}

bool TerminalCtrl::ProcessPCStyleFunctionKey(const FunctionKey& k, dword modkeys, int count)
{
	int modifiers = 0;

	switch(modkeys) {
	case K_SHIFT:
		modifiers = 2;
		break;
	case K_ALT:
		modifiers = 3;
		break;
	case K_ALT|K_SHIFT:
		modifiers = 4;
		break;
	case K_CTRL:
		modifiers = 5;
		break;
	case K_CTRL|K_SHIFT:
		modifiers = 6;
		break;
	case K_CTRL|K_ALT:
		modifiers = 7;
		break;
	case K_SHIFT|K_ALT|K_CTRL:
		modifiers = 8;
		break;
	default:
		break;
	}

	if(modifiers) {
		if(k.type == FunctionKey::Cursor || (k.type == FunctionKey::NumPad && modes[DECKPAM])) {
			PutCSI(~Format("1;%d`%s", modifiers, k.code));
			return true;
		}
		else
		if(k.type == FunctionKey::Programmable) {
			PutCSI(Format("1;%d`%s", modifiers, k.code));
			return true;
		}
		else
		if(k.type == FunctionKey::EditPad && k.altcode) {
			PutCSI(Format("1;%d`%s", modifiers, k.altcode));
			return true;
		}
		else
		if(k.type == FunctionKey::EditPad || k.type == FunctionKey::Function) {
			PutCSI(Format("%s;%d~", k.code, modifiers));
			return true;
		}
	}
	else
	if(k.type == FunctionKey::EditPad && k.altcode) {
		if(modes[DECKPAM])
			PutSS3(k.altcode, count);
		else
			PutCSI(k.altcode, count); // CSI H and CSI F
		return true;
	}

	// Basically, all other f-keys are same as in VT-style f-keys.
	return ProcessVTStyleFunctionKey(k, modkeys, count);
}

bool TerminalCtrl::VTKey(dword key, int count)
{
	const static VectorMap<dword, FunctionKey> sFunctionKeyMap = {
        { { K_UP,       }, { FunctionKey::Cursor,       LEVEL_0, "A"  } },
        { { K_DOWN,     }, { FunctionKey::Cursor,       LEVEL_0, "B"  } },
        { { K_RIGHT,    }, { FunctionKey::Cursor,       LEVEL_0, "C"  } },
        { { K_LEFT,     }, { FunctionKey::Cursor,       LEVEL_0, "D"  } },
        { { K_INSERT,   }, { FunctionKey::EditPad,      LEVEL_2, "2"  } },
        { { K_DELETE,   }, { FunctionKey::EditPad,      LEVEL_2, "3"  } },
        { { K_HOME,     }, { FunctionKey::EditPad,      LEVEL_2, "1", "H"  } },
        { { K_END,      }, { FunctionKey::EditPad,      LEVEL_2, "4", "F"  } },
        { { K_PAGEUP,   }, { FunctionKey::EditPad,      LEVEL_2, "5"  } },
        { { K_PAGEDOWN, }, { FunctionKey::EditPad,      LEVEL_2, "6"  } },
        { { K_MULTIPLY, }, { FunctionKey::NumPad,       LEVEL_0, "j"  } },
        { { K_ADD,      }, { FunctionKey::NumPad,       LEVEL_0, "k"  } },
        { { K_SEPARATOR,}, { FunctionKey::NumPad,       LEVEL_0, "l"  } },
        { { K_SUBTRACT, }, { FunctionKey::NumPad,       LEVEL_0, "m"  } },
        { { K_DECIMAL,  }, { FunctionKey::NumPad,       LEVEL_0, "n"  } },
        { { K_DIVIDE,   }, { FunctionKey::NumPad,       LEVEL_0, "o"  } },
        { { K_NUMPAD0,  }, { FunctionKey::NumPad,       LEVEL_0, "p"  } },
        { { K_NUMPAD1,  }, { FunctionKey::NumPad,       LEVEL_0, "q"  } },
        { { K_NUMPAD2,  }, { FunctionKey::NumPad,       LEVEL_0, "r"  } },
        { { K_NUMPAD3,  }, { FunctionKey::NumPad,       LEVEL_0, "s"  } },
        { { K_NUMPAD4,  }, { FunctionKey::NumPad,       LEVEL_0, "t"  } },
        { { K_NUMPAD5,  }, { FunctionKey::NumPad,       LEVEL_0, "u"  } },
        { { K_NUMPAD6,  }, { FunctionKey::NumPad,       LEVEL_0, "v"  } },
        { { K_NUMPAD7,  }, { FunctionKey::NumPad,       LEVEL_0, "w"  } },
        { { K_NUMPAD8,  }, { FunctionKey::NumPad,       LEVEL_0, "x"  } },
        { { K_NUMPAD9,  }, { FunctionKey::NumPad,       LEVEL_0, "y"  } },
        { { K_F1,       }, { FunctionKey::Programmable, LEVEL_0, "P"  } },  // PF1
        { { K_F2,       }, { FunctionKey::Programmable, LEVEL_0, "Q"  } },  // PF2
        { { K_F3,       }, { FunctionKey::Programmable, LEVEL_0, "R"  } },  // PF3
        { { K_F4,       }, { FunctionKey::Programmable, LEVEL_0, "S"  } },  // PF4
        { { K_F5,       }, { FunctionKey::Function,     LEVEL_2, "15" } },
        { { K_F6,       }, { FunctionKey::Function,     LEVEL_2, "17" } },
        { { K_F7,       }, { FunctionKey::Function,     LEVEL_2, "18" } },
        { { K_F8,       }, { FunctionKey::Function,     LEVEL_2, "19" } },
        { { K_F9,       }, { FunctionKey::Function,     LEVEL_2, "20" } },
        { { K_F10,      }, { FunctionKey::Function,     LEVEL_2, "21" } },
        { { K_F11,      }, { FunctionKey::Function,     LEVEL_2, "23" } },
        { { K_F12,      }, { FunctionKey::Function,     LEVEL_2, "24" } },
        { { K_CTRL_F1,  }, { FunctionKey::Function,     LEVEL_2, "25" } },  // In VT-key mode: F13
        { { K_CTRL_F2,  }, { FunctionKey::Function,     LEVEL_2, "26" } },  // In VT-key mode: F14
        { { K_CTRL_F3,  }, { FunctionKey::Function,     LEVEL_2, "28" } },  // In VT-key mode: F15
        { { K_CTRL_F4,  }, { FunctionKey::Function,     LEVEL_2, "29" } },  // In VT-key mode: F16
        { { K_CTRL_F5,  }, { FunctionKey::Function,     LEVEL_2, "31" } },  // In VT-key mode: F17
        { { K_CTRL_F6,  }, { FunctionKey::Function,     LEVEL_2, "32" } },  // In VT-key mode: F18
        { { K_CTRL_F7,  }, { FunctionKey::Function,     LEVEL_2, "33" } },  // In VT-key mode: F19
        { { K_CTRL_F8,  }, { FunctionKey::Function,     LEVEL_2, "34" } },  // In VT-key mode: F20
	};

	dword keymask = K_SHIFT|K_ALT|(pcstylefunctionkeys * K_CTRL);

	int i = sFunctionKeyMap.Find(key & ~keymask);
	if(i < 0)
		return false;
	
	const FunctionKey& k = sFunctionKeyMap[i];
	if(k.level > clevel)
		return false;
	
	if(IsLevel0()) { // VT52
			if(k.type == FunctionKey::Cursor || k.type == FunctionKey::Programmable) {
				PutESC(k.code, count);
				return true;
			}
			else
			if(k.type == FunctionKey::NumPad && modes[DECKPAM]) {
				PutESC(String("?") << k.code, count);
				return true;
			}
	}
	else
	if(IsLevel1()) { // ANSI/PC
		if(pcstylefunctionkeys)
			return ProcessPCStyleFunctionKey(k, key & keymask, count);
		else
			return ProcessVTStyleFunctionKey(k, key & keymask, count);
	}

	return false;
}

bool TerminalCtrl::UDKey(dword key, int count)
{
	if(!HasUDK())
		return false;

	// DEC user-defined keys (DECUDK) support
	
	const static Tuple<dword, dword> sUDKMap[] = {
        { K_F1,      11 },  { K_F2,      12 },
        { K_F3,      13 },  { K_F4,      14 },
        { K_F5,      15 },  { K_F6,      17 },
        { K_F7,      18 },  { K_F8,      19 },
        { K_F9,      20 },  { K_F10,     21 },
        { K_F11,     23 },  { K_F12,     24 },
        { K_CTRL_F1, 25 },  { K_CTRL_F2, 26 },
        { K_CTRL_F3, 28 },  { K_CTRL_F4, 29 },
        { K_CTRL_F5, 31 },  { K_CTRL_F6, 32 },
        { K_CTRL_F7, 33 },  { K_CTRL_F8, 34 },
        { K_CTRL_F9, 35 },  { K_CTRL_F10,36 },
	};

	auto *k = FindTuple(sUDKMap, __countof(sUDKMap), key & ~(K_SHIFT|K_ALT));
	if(!k)
		return false;

	dword userkey  = 0;
	
	if(pcstylefunctionkeys) {
		if(key & (K_SHIFT|K_ALT)) {
			userkey = k->b < 25 ? (K_SHIFT|K_ALT|k->b) : 0;
		}
		else
		if(key & K_ALT) {
			userkey = k->b < 25 ? (K_ALT|k->b) : 0;
		}
		else
		if(key & K_SHIFT) {
			userkey = K_SHIFT|k->b;
		}
	}
	else
	if((key & K_SHIFT) && k->b < 35) {
		userkey = K_SHIFT|k->b;
	}

	if(userkey) {
		String s;
		if(GetUDKString(userkey, s)) {
			Put(s.ToWString(), count);
			return true;
		}
	}

	return false;
}

bool TerminalCtrl::NavKey(dword key, int count)
{
	if(!keynavigation)
		return false;

	if(Match(AK_MOVE_UP, key)) {
		sb.PrevLine();
	}
	else
	if(Match(AK_MOVE_DOWN, key)) {
		sb.NextLine();
	}
	else
	if(Match(AK_MOVE_PAGEUP, key)) {
		sb.PrevPage();
	}
	else
	if(Match(AK_MOVE_PAGEDOWN, key)) {
		sb.NextPage();
	}
	else
	if(Match(AK_MOVE_HOME, key)) {
		sb.Begin();
	}
	else
	if(Match(AK_MOVE_END, key)) {
		sb.End();
	}
	else
		return false;
	
	return true;
}

bool TerminalCtrl::Key(dword key, int count)
{
	if(IsReadOnly()	|| (!modes[DECARM] && count > 1))
		return MenuBar::Scan(WhenBar, key);

	if(IsSelectorMode()) {
		ProcessSelectorKey(key, count);
		return true;
	}

	bool ctrlkey  = key & K_CTRL;
	bool altkey   = key & K_ALT;
	bool shiftkey = key & K_SHIFT;
	
	if(UDKey(key, count)) {
		SyncSb(true);
		goto End;
	}
	else
	if(NavKey(key, count))
		goto End;
	else
	if(MenuBar::Scan(WhenBar, key))
		return true;

	if(key & K_KEYUP)	// We don't really need to handle key-ups...
		return false;

#ifdef PLATFORM_COCOA
	if(findarg(key & ~(K_CTRL|K_ALT|K_SHIFT|K_OPTION), K_CTRL_KEY, K_ALT_KEY, K_SHIFT_KEY, K_OPTION_KEY) >= 0)
		return false;
	key &= ~K_OPTION;
#else
	if(findarg(key & ~(K_CTRL|K_ALT|K_SHIFT), K_CTRL_KEY, K_ALT_KEY, K_SHIFT_KEY) >= 0)
		return false;
#endif

	if(key == K_RETURN) {
		PutEol();
	}
	else {
		// Handle character.
		if(!shiftkey && key >= ' ' && key < K_CHAR_LIM) {
			if(!ProcessKey(key, ctrlkey, altkey, count))
				return false;
		}
		else {
			// Handle control key (including information separators).
			switch(key & ~(K_ALT|K_SHIFT)) {
			case K_BACKSPACE:
				key = modes[DECBKM] ? 0x08 : 0x7F;
				break;
			case K_ESCAPE:
				key = 0x1B;
				break;
			case K_TAB:
				key = 0x09;
				break;
			case K_CTRL_BREAK:
				key = 0x03;
				break;
			case K_CTRL_LBRACKET:
				key = '[';
				break;
			case K_CTRL_RBRACKET:
				key = ']';
				break;
			case K_CTRL_MINUS:
				key = '-';
				break;
			case K_CTRL_GRAVE:
				key = '`';
				break;
			case K_CTRL_SLASH:
				key = '/';
				break;
			case K_CTRL_BACKSLASH:
				key = '\\';
				break;
			case K_CTRL_COMMA:
				key = ',';
				break;
			case K_CTRL_PERIOD:
				key = '.';
				break;
			#ifndef PLATFORM_WIN32 // U++ ctrl + period and ctrl + semicolon enumeratos have the same value on Windows (a bug?)
			case K_CTRL_SEMICOLON:
				key = ';';
				break;
			#endif
			case K_CTRL_EQUAL:
				key = '=';
				break;
			case K_CTRL_APOSTROPHE:
				key = '\'';
				break;
			default:
				if(VTKey(key, count)) {
					SyncSb(true);
					goto End;
				}
				if(ctrlkey || altkey) {
					key &= ~(K_CTRL|K_ALT|K_SHIFT);
					if(key >= K_A && key <= K_Z) {
						key = 'a' + (key - K_A);
					}
					else
					if(key == K_2) {
						key = '@';
					}
					else
					if(key >= K_3 && key <= K_8) {
						key = '[' + (key - K_3);
					}
				}
			}
			if(key > K_DELTA || !ProcessKey(key, ctrlkey, altkey, count))
				return false;
		}
	}
	
	SyncSb(true);

End:
	if(hidemousecursor)
		mousehidden = true;
	return true;
}
}
