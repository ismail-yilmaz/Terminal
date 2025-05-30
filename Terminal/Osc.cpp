#include "Terminal.h"

#define LLOG(x)     // RLOG("TerminalCtrl (#" << this << "]: " << x)
#define LTIMING(x)	// RTIMING(x)

namespace Upp {

void TerminalCtrl::ParseOperatingSystemCommands(const VTInStream::Sequence& seq)
{
	LLOG(seq);

	int opcode = seq.GetInt(1, 0);

	switch(opcode) {
	case 0:		// Set window titile
	case 2:		// Set window title (and icon name)
		WhenTitle(DecodeDataString(seq.GetStr(2)).ToString());
		break;
	case 4:		// Set ANSI colors
	case 10:	// Set dynamic color (ink)
	case 11:	// Set dynamic color (paper)
	case 17:	// Set dynamic color (selection ink)
	case 19:	// Set dynamic color (selection paper)
		SetProgrammableColors(seq, opcode);
		break;
	case 104:	// Reset ANSI colors
	case 110:	// Reset dynamic color (ink)
	case 111:	// Reset dynamic color (paper)
	case 117:	// Reset dynamic color (selection ink)
	case 119:	// Reset dynamic color (selection paper)
		ResetProgrammableColors(seq, opcode);
		break;
	case 7:     // Change the current working directory.
		ParseWorkingDirectoryChangeRequest(seq);
		break;
	case 8:		// Explicit hyperlinks protocol.
		ParseHyperlinks(seq);
		break;
	case 9:     // Parse ConEMU specific protocols
		ParseConEmuProtocols(seq);
		break;
	case 52:	// Clipboard manipulation protocol.
		ParseClipboardRequests(seq);
		break;
	case 133:   // Semantic information (a.k.a. "semantic prompts")
		ParseSemanticInformation(seq);
		break;
	case 444:	// Jexer inline images protocol.
		ParseJexerGraphics(seq);
		break;
	case 1337:	// iTerm2 protocols.
		ParseiTerm2Protocols(seq);
		break;
	case 8100:  // TerminalCtrl protocols
		ParseTerminalCtrlProtocols(seq);
		break;
	default:
		LLOG(Format("Unhandled OSC opcode: %d", opcode));
		break;
	}
}

void TerminalCtrl::ParseJexerGraphics(const VTInStream::Sequence& seq)
{
	// For more information on Jexer image protocol, see:
	// https://gitlab.com/klamonte/jexer/-/wikis/jexer-images

	if(!jexerimages)
		return;
	
	int type = seq.GetInt(2, Null);
	if(type > 2 || IsNull(type))	// V1 defines 3 types (0-based).
		return;

	ImageString simg;
	bool scroll = false;

	if(type == 0) {	// Bitmap
		simg.size.cx = min(seq.GetInt(3), 10000);
		simg.size.cy = min(seq.GetInt(4), 10000);
		scroll       = seq.GetInt(5, 0) > 0;
		simg.data    = pick(seq.GetStr(6));
	}
	else { // Other image formats (jpg, png, etc.)
		scroll       = seq.GetInt(3, 0) > 0;
		simg.data    = pick(seq.GetStr(4));
	}

	cellattrs.Hyperlink(false);

	RenderImage(simg, scroll);
}

void TerminalCtrl::ParseiTerm2Protocols(const VTInStream::Sequence& seq)
{
	if(iterm2images
		&& ParseiTerm2Graphics(seq))
			return;
	if(nobackground
		&& ParseiTerm2BackgroundChange(seq))
			return;
// if(annotations
//		&& ParseiTerm2Annotations(seq))
//			return;
	
}

bool TerminalCtrl::ParseiTerm2Graphics(const VTInStream::Sequence& seq)
{
	// iTerm2's file and image download and display protocol,
	// Currently, we only support its inline images  portion.
	// See: https://iterm2.com/documentation-images.html

	int pos = 0;
	String options, enc;
	if(!SplitTo(seq.payload, ':', false, options, enc)
	|| (pos = ToLower(options).FindAfter("file=")) < 0 || IsNull(enc))
		return false;

	auto GetVal = [this](const String& s, int p, int f) -> int
	{
		int n = ReadInt(s, 0);
		if(!n)
			return n;
		if(s.IsEqual("auto"))
			return 0;
		if(s.EndsWith("px"))
			return min(n, 10000);
		if(s.EndsWith("%"))
			return (min(n, 1000) * p * f) / 100;
		return n * f;
	};

	ImageString simg(pick(enc));
	
	simg.size.Clear();
	bool show = false;
	
	for(const String& s : Split(options.Mid(pos), ';', false)) {
		String key, val;
		if(SplitTo(ToLower(s), '=', false, key, val)) {
			if(key.IsEqual("inline"))
				show = val == "1";
			else
			if(key.IsEqual("width"))
				simg.size.cx = GetVal(val, GetPageSize().cx, GetCellSize().cx);
			else
			if(key.IsEqual("height"))
				simg.size.cy = GetVal(val, GetPageSize().cy, GetCellSize().cy);
			else
			if(key.IsEqual("preserveaspectratio"))
				simg.keepratio = val == "1";
		}
	}
	
	if(show) {
		if(simg.size.cx == 0 && simg.size.cy == 0)
			simg.size.SetNull();
		RenderImage(simg, !modes[DECSDM]);	// Rely on sixel scrolling mode.
	}
	
	return true;
}

bool TerminalCtrl::ParseiTerm2BackgroundChange(const VTInStream::Sequence& seq)
{
	String path = seq.GetStr(2);
	int pos = path.FindAfter("SetBackgroundImageFile=");
	if(pos >= 0)
		WhenBackgroundChange(Base64Decode(path.Mid(pos)));
	return pos >= 0;
}

bool TerminalCtrl::ParseiTerm2Annotations(const VTInStream::Sequence& seq)
{
	// TODO
	return false;
}

void TerminalCtrl::ParseHyperlinks(const VTInStream::Sequence& seq)
{
	// For more information on explicit hyperlinks, see:
	// https://gist.github.com/egmontkob/eb114294efbcd5adb1944c9f3cb5feda

	if(!hyperlinks)
		return;

	constexpr const int MAX_URI_LENGTH = 2084;

	String uri = seq.GetStr(3);

	if(IsNull(uri) || uri.GetLength() > MAX_URI_LENGTH) {
		cellattrs.Hyperlink(false);
		cellattrs.data = 0;
	}
	else {
		uri = UrlDecode(uri);
		cellattrs.Image(false)
				 .Annotation(false)
				 .Hyperlink(true).data = RenderHypertext(uri);
	}
}

void TerminalCtrl::ParseClipboardRequests(const VTInStream::Sequence& seq)
{
	// For more information on application clipboard access, see:
	// https://invisible-island.net/xterm/ctlseqs/ctlseqs.html
	
	if(!IsClipboardAccessPermitted() || !HasFocus())
		return;

	auto CheckInvalidBase64Chars = [](int c) -> bool
	{
		return !IsAlNum(c) && c != '=' && c != '+' && c != '/';
	};
	
	String params = seq.GetStr(2);	// We don't support multiple clipboard buffers...
	String data   = seq.GetStr(3);

	if(IsClipboardReadPermitted() && data.IsEqual("?")) {
		WString o = GetWString(Selection());
		if(IsNull(o))
			o = GetWString(Clipboard());
		PutOSC("52;s0;" + Base64Encode(EncodeDataString(o)));
	}
	else
	if(IsClipboardWritePermitted()) {
		if(FindMatch(data, CheckInvalidBase64Chars) < 0) {
			String in = Base64Decode(data);
			if(!IsNull(in))
				Copy(DecodeDataString(in));
		}
		else
			ClearClipboard();
	}
}

void TerminalCtrl::ParseWorkingDirectoryChangeRequest(const VTInStream::Sequence& seq)
{
	WhenDirectoryChange(seq.GetStr(2));
}

void TerminalCtrl::ParseSemanticInformation(const VTInStream::Sequence& seq)
{
	// For more information on semantic prompts, see:
	// https://gitlab.freedesktop.org/Per_Bothner/specifications/blob/master/proposals/semantic-prompts.md

	if(!semanticinformation)
		return;
	
	String s = seq.GetStr(2);

	// ATM, we only care about a minimal subset of this protocol.

	switch(s[0]) {
	case 'A': cellattrs.SetAsPrompt(); break;
	case 'B': cellattrs.SetAsInput();  break;
	case 'C': cellattrs.SetAsOutput(); break;
	default:  cellattrs.ClearSemanticInfo(); break;
	}
}

void TerminalCtrl::ParseTerminalCtrlProtocols(const VTInStream::Sequence& seq)
{
	if(seq.GetInt(2) == 1)
		ParseTerminalCtrlAnnotations(seq);
}

void TerminalCtrl::ParseTerminalCtrlAnnotations(const VTInStream::Sequence& seq)
{
	if(!annotations || seq.parameters.GetCount() != 4)
		return;
	
	constexpr const int MAX_ANNOTATION_LENGTH = 65536;

	String type = seq.GetStr(3);
	String anno = seq.GetStr(4);

	anno = Base64Decode(anno);
	
	if(IsNull(anno) || anno.GetLength() > MAX_ANNOTATION_LENGTH) {
		cellattrs.Annotation(false);
		cellattrs.data = 0;
	}
	else {
		cellattrs.Image(false)
				 .Hyperlink(false)
				 .Annotation(true).data = RenderHypertext(anno);
	}
}

void TerminalCtrl::ParseConEmuProtocols(const VTInStream::Sequence& seq)
{
	// For more information on ConEMU specific commands, see:
	// https://conemu.github.io/en/AnsiEscapeCodes.html#ConEmu_specific_OSC
	
	int opcode = seq.GetInt(2);
	
	switch(opcode) {
	case 2: ParseConEmuMessageBoxMessage(seq);             break;
	case 4: ParseConEmuProgressEvent(seq);                 break;
	case 9: ParseConEmuWorkingDirectoryChangeRequest(seq); break;
	default: LLOG("Unhandled ConEmu opcode: " << opcode);  break;
	}
}

void TerminalCtrl::ParseConEmuProgressEvent(const VTInStream::Sequence& seq)
{
	// https://learn.microsoft.com/en-us/windows/terminal/tutorials/progress-bar-sequences

	if(!notifyprogress)
		return;
	
	int n = seq.GetInt(4);
           
	switch(seq.GetInt(3)) {
	case 1: WhenProgress(PROGRESS_NORMAL, clamp(n, 0, 100));  break;
	case 2: WhenProgress(PROGRESS_ERROR, n);   break;
	case 3: WhenProgress(PROGRESS_BUSY, 0);    break;
	case 4: WhenProgress(PROGRESS_WARNING, n); break;
	default: WhenProgress(PROGRESS_OFF, 0);    break;
	}
}

void TerminalCtrl::ParseConEmuWorkingDirectoryChangeRequest(const VTInStream::Sequence& seq)
{
	// https://learn.microsoft.com/en-us/windows/terminal/tutorials/new-tab-same-directory
	
	WhenDirectoryChange(seq.GetStr(3));
}

void TerminalCtrl::ParseConEmuMessageBoxMessage(const VTInStream::Sequence& seq)
{
	// https://conemu.github.io/en/AnsiEscapeCodes.html#ConEmu_specific_OSC
	
	WhenMessage(seq.GetStr(3));
}

}