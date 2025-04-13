#include "Terminal.h"

#define LLOG(x)     // RLOG("TerminalCtrl (#" << this << "]: " << x)
#define LTIMING(x)	// RTIMING(x)

namespace Upp {

#define KEYGROUPNAME TERMINALCTRL_KEYGROUPNAME
#define KEYNAMESPACE TerminalCtrlKeys
#define KEYFILE <Terminal/Terminal.key>
#include <CtrlLib/key_source.h>

using namespace TerminalCtrlKeys;

bool sDefaultCellFilter(const VTCell& cell)
{
	return !cell.IsImage() && (IsLeNum(cell) || findarg(cell, '_', '-') >= 0);
}

bool IsWCh(const VTCell& cell, bool line_wrap, Gate<const VTCell&> f = Null)
{
	return (cell == 0 && line_wrap) || cell == 1 || (f ? f(cell) : sDefaultCellFilter(cell));
}

TerminalCtrl::TerminalCtrl()
: page(&dpage)
, legacycharsets(false)
, eightbit(false)
, windowactions(false)
, windowreports(false)
, sixelimages(false)
, jexerimages(false)
, iterm2images(false)
, hyperlinks(false)
, annotations(false)
, semanticinformation(false)
, reversewrap(false)
, hidemousecursor(false)
, sizehint(true)
, delayedrefresh(true)
, lazyresize(false)
, blinkingtext(true)
, adjustcolors(false)
, lightcolors(false)
, dynamiccolors(false)
, intensify(false)
, nobackground(false)
, alternatescroll(false)
, keynavigation(true)
, userdefinedkeys(false)
, userdefinedkeyslocked(true)
, pcstylefunctionkeys(true)
, streamfill(false)
, scrolltoend(true)
, highlight(false)
, notifyprogress(false)
, ambiguouschartowide(false)
{
	Unicode();
	SetLevel(LEVEL_4);
	SetCharset(CHARSET_UNICODE);
	InitParser(parser);
	SetImageDisplay(NormalImageCellDisplay());
	SetFrame(NullFrame());
	History();
	ResetColors();
	HideScrollBar();
	TreatAmbiguousCharsAsWideChars(ambiguouschartowide);
	WhenBar = [=](Bar& menu) { StdBar(menu); };
	sb.WhenScroll = [=]()    { Scroll(); };
	caret.WhenAction = [=]() { ScheduleRefresh(); };
	dpage.WhenUpdate = [=]() { ScheduleRefresh(); };
	apage.WhenUpdate = [=]() { ScheduleRefresh(); };
}

TerminalCtrl::~TerminalCtrl()
{
	// Make sure that no callback is left dangling...
	KillTimeCallback(TIMEID_REFRESH);
	KillTimeCallback(TIMEID_SIZEHINT);
	KillTimeCallback(TIMEID_BLINK);
}

TerminalCtrl& TerminalCtrl::SetFont(Font f)
{
	Size fsz = GetFontSize();
	font = f;
	return SetPadding(iscale(GetFontSize(), padding, max(Size(1, 1), fsz)));
}

TerminalCtrl& TerminalCtrl::SetPadding(Size sz)
{
	padding = clamp(sz, Size(0, 0), GetFontSize() * 2);
	Layout();
	return *this;
}

void TerminalCtrl::PlaceCaret(bool scroll)
{
	Rect oldrect = caretrect;

	if(!IsSelectorMode()) {
		if(!modes[DECTCEM] && !IsSelectorMode()) {
			caretrect = Null;
			if(!caret.IsBlinking())
				Refresh(oldrect);
			return;
		}
		caretrect = GetCaretRect();
	}
	else
		caretrect = GetSelectorCaretRect();
	
	if(!caret.IsBlinking()) {
		Refresh(oldrect);
		Refresh(caretrect);
	}
	if(scroll && IsDefaultPage()) {
		sb.ScrollInto(GetCursorPos().y);
	}
}

Rect TerminalCtrl::MakeCaretRect(Point pt, const VTCell& cell) const
{
	Size csz = GetCellSize();
	pt *= csz;
	int cw = cell.GetWidth(GetPage().GetAmbiguousCellWidth());

	pt.y -= (csz.cy * GetSbPos());

	switch(caret.GetStyle()) {
	case Caret::BEAM:
		csz.cx = 1;
		break;
	case Caret::UNDERLINE:
		csz.cx *= max(cw, 1); // Adjust the caret width to cell size.
		pt.y += csz.cy - 1;
		csz.cy = 1;
		break;
	case Caret::BLOCK:
		csz.cx *= max(cw, 1); // Adjust the caret width to cell size.
		break;
	}

	return Rect(pt, csz);
}

Rect TerminalCtrl::GetCaretRect() const
{
	Rect r = MakeCaretRect(GetCursorPos(), page->GetCell());
	return Rect(GetSize()).Contains(r) ? r : Null;
}

Rect TerminalCtrl::GetSelectorCaretRect() const
{
	Rect r = MakeCaretRect(cursor, page->FetchCell(cursor));
	if(cursor.x == GetPageSize().cx) {
		r.left = GetSize().cx - 1;
		r.right += r.left + 1;
	}
	return r;
}

Point TerminalCtrl::GetCursorPoint() const
{
	Size csz = GetCellSize();
	Point pt = GetCursorPos() * csz;
	pt.y -= (csz.cy * GetSbPos());
	return pt;
}

void TerminalCtrl::Copy(const WString& s)
{
	if(!IsNull(s)) {
		ClearClipboard();
		AppendClipboardUnicodeText(s);
		AppendClipboardText(s.ToString());
	}
}

void TerminalCtrl::Paste(const WString& s, bool filter)
{
	if(IsReadOnly())
		return;

	if(modes[XTBRPM]) {
		PutCSI("200~");
		PutEncoded(s, filter);
		PutCSI("201~");
	}
	else
		PutEncoded(s, filter);

	SyncSb(true);
}

void TerminalCtrl::SelectAll(bool history)
{
	Size psz = GetPageSize();
	bool h = IsDefaultPage() && history;
	Rect r = RectC(0, h ? 0 : sb, psz.cx, (h ? sb + (sb.GetTotal() - sb) : psz.cy) - 1);
	SetSelection(r.TopLeft(), r.BottomRight(), SEL_TEXT);
}

String TerminalCtrl::GetSelectionData(const String& fmt) const
{
	return IsSelection() ? GetTextClip(GetSelectedText().ToString(), fmt) : Null;
}

TerminalCtrl& TerminalCtrl::SetWordSelectionFilter(Gate<const VTCell&> filter)
{
	cellfilter = filter;
	return *this;
}

Gate<const VTCell&> TerminalCtrl::GetWordSelectionFilter() const
{
	return cellfilter;
}

void TerminalCtrl::SyncSize(bool notify)
{
	// Apparently, the window minimize event on Windows "really" minimizes
	// the window. This results in a damaged terminal display. In order to
	// avoid this, we check the  new page size, and  do not attempt resize
	// the page if the requested page size is < 2 x 2 cells.

	Size newsize = GetPageSize();
	resizing = page->GetSize() != newsize;

	auto OnSizeHint	= [=]
	{
		RefreshSizeHint();
		hinting = false;
	};
	
	auto OnResize = [=]
	{
		ClearSelection();
		resizing = false;
		WhenResize();
		ScheduleRefresh();
	};
	
	if(resizing && newsize.cx > 1 && 1 < newsize.cy) {
		page->SetSize(newsize);
		if(notify) {
			if(sizehint) {
				hinting = true;
				KillSetTimeCallback(1000, OnSizeHint, TIMEID_SIZEHINT);
			}
			if(lazyresize)
				KillSetTimeCallback(100, OnResize, TIMEID_REFRESH);
			else
				OnResize();
		}
		else
			resizing = false;
	}
	else {
		page->Invalidate();
		RefreshDisplay();
	}
}

void TerminalCtrl::ScheduleRefresh()
{
	if(!delayedrefresh) {
		SyncSb();
		RefreshDisplay();
	}
	else
	if((!lazyresize || !resizing)
	&& !ExistsTimeCallback(TIMEID_REFRESH))  // Don't cancel a pending refresh.
		SetTimeCallback(16, [=] { SyncSb(); RefreshDisplay(); }, TIMEID_REFRESH);
}

Tuple<String, Size> TerminalCtrl::GetSizeHint()
{
	Tuple<String, Size> hint;
	Size psz = GetPageSize();
	hint.a << psz.cx << " x " << psz.cy;
	hint.b = GetTextSize(hint.a, StdFont());
	return hint;
}

void TerminalCtrl::RefreshSizeHint()
{
	Refresh(GetViewRect().CenterRect(GetSizeHint().b).Inflated(12));
}

void TerminalCtrl::SyncSb(bool forcescroll)
{
	if(IsAlternatePage())
		return;

	int  pcy = sb.GetPage();
	int  tcy = sb.GetTotal();
	
	if(!forcescroll)
		forcescroll = (scrolltoend || (sb + pcy == tcy)) && !ignorescroll;
	
	sb.SetTotal(page->GetLineCount());
	sb.SetPage(page->GetSize().cy);
	sb.SetLine(1);
	
	if(forcescroll)
		sb.End();
	else {
		// This is to keep the display up-to-date (refreshed) on no-autoscrolling mode.
		Refresh();
		PlaceCaret();
	}
}

void TerminalCtrl::Scroll()
{
	// It is possible to  have  an  alternate screen buffer with a history  buffer.
	// Some terminal  emulators already  come  with  this feature enabled. Terminal
	// ctrl can  also support this  feature out-of-the-box, as it uses  the  VTPage
	// class for both its default and alternate screen buffers. Thus the difference
	// is only semantic and practical. At the  moment, however, this feature is n0t
	// enabled. This may change in the future.

	if(IsAlternatePage())
		return;

	WhenScroll();
	Refresh();
	PlaceCaret();
}

void TerminalCtrl::SwapPage()
{
	SyncSize(false);
	SyncSb();
	ClearSelection();
}

void TerminalCtrl::RefreshDisplay()
{
	const Size wsz = GetSize();
	const Size psz = GetPageSize();
	const Size csz = GetCellSize();
	const int  pos = GetSbPos();
	
	LTIMING("TerminalCtrl::RefreshDisplay");
	
	const int cnt = min(pos + psz.cy, page->GetLineCount());
	int blinking_cells = 0;
	int hypertext_cells = 0;

	const bool hypertext = hyperlinks || annotations;
	const bool plaintext = !hypertext && !blinkingtext;
	
	Rect rdirty = Null;
	Rect rblink = Null;
	Rect rhtext = Null;
	
	for(int i = pos; i < cnt; i++) {
		const VTLine& line = page->FetchLine(i);
		int y = i * csz.cy - (csz.cy * pos);
		bool invalid = line.IsInvalid();
		
		if(!plaintext) {
			for(int j = 0; j < line.GetCount(); j++) {
				const VTCell& cell = line[j];
				int x = j * csz.cx;
				if(hypertext && cell.IsHypertext()
				&& (cell.data == activehtext || cell.data == prevhtext)) {
					hypertext_cells++;
						if(!invalid)
							rhtext.Union(RectC(x, y, csz.cx, csz.cy));
				}
				else
				if(blinkingtext && cell.IsBlinking()) {
					blinking_cells++;
					if(!invalid)
						rblink.Union(RectC(x, y, csz.cx, csz.cy));
				}
			}
		}
		if(invalid) {
			rdirty.Union(RectC(0, y, wsz.cx, csz.cy));
			if(i == cnt - 1) rdirty.bottom = wsz.cy;
			line.Validate();
		}

	}
	
	if(!rdirty.IsEmpty())
		Refresh(rdirty.Inflated(4));
	
	if(!plaintext) {
		if(!rblink.IsEmpty())
			Refresh(rblink.Inflated(4));
		
		if(!rhtext.IsEmpty())
			Refresh(rhtext.Inflated(4));
	}
	
	PlaceCaret();
	Blink(blinking_cells > 0);
}

void TerminalCtrl::Blink(bool b)
{
	bool bb = ExistsTimeCallback(TIMEID_BLINK);
	if(blinkingtext && b && !bb)
		SetTimeCallback(-blinkinterval, [=]{ blinking ^= 1; RefreshDisplay(); }, TIMEID_BLINK);
	else
	if(!blinkingtext || !b) {
		blinking = false;
		if(bb)
			KillTimeCallback(TIMEID_BLINK);
	}
}

void TerminalCtrl::DragAndDrop(Point pt, PasteClip& d)
{
	if(IsReadOnly() || IsDragAndDropSource())
		return;
	
	WString s;

	if(AcceptFiles(d)) {
		for(const auto& f : GetFiles(d)) {
			s.Cat('\'');
			s.Cat(f.ToWString());
			s.Cat('\'');
		}
		s = TrimRight(s);
	}
	else
	if(AcceptText(d))
		s = GetWString(d);
	else
		return;

	d.SetAction(DND_COPY);
	
	bool noctl = WhenClip(d);

	if(d.IsAccepted())
		Paste(s, noctl);
}

void TerminalCtrl::LeftDown(Point pt, dword keyflags)
{
	SetFocus();
	if(IsMouseTracking(keyflags))
		VTMouseEvent(pt, LEFTDOWN, keyflags);
	else{
		if(IsSelectorMode()) {
			ClearSelection();
		}
		else
		if(IsSelected(ClientToPagePos(pt))) {
			return;
		}
		else {
			pt = SelectionToPagePos(pt);
			SetSelection(pt, pt, (keyflags & K_CTRL) ? SEL_RECT : SEL_TEXT);
		}
	}
	SetCapture();
}

void TerminalCtrl::LeftUp(Point pt, dword keyflags)
{
	if(IsMouseTracking(keyflags)) {
		if(!modes[XTX10MM])
			VTMouseEvent(pt, LEFTUP, keyflags);
	}
	else {
		pt = ClientToPagePos(pt);
		if(multiclick)
			multiclick = false;
		else
		if(!HasCapture() && IsSelected(pt))
			ClearSelection();
	}
	ReleaseCapture();
}

void TerminalCtrl::LeftDrag(Point pt, dword keyflags)
{
	pt = ClientToPagePos(pt);
	bool modifier = keyflags & K_CTRL;
	
	if(!IsMouseTracking(keyflags)) {
		VectorMap<String, ClipData> data;
		if(!HasCapture() && !modifier && IsSelected(pt)) {
			WString tsample = GetSelectedText();
			Append(data, tsample);
			Size tsz = StdSampleSize();
			ImageDraw iw(tsz);
			iw.DrawRect(tsz, Black());
			iw.Alpha().DrawRect(tsz, Black());
			DrawTLText(iw.Alpha(), 0, 0, tsz.cx, tsample, font, White());
			DoDragAndDrop(data, iw, DND_COPY);
		}
		else
		if(modifier && (IsMouseOverHyperlink(pt) || IsMouseOverAnnotation(pt))) {
			WString lsample = GetHypertextContent(pt, modifier).ToWString();
			Append(data, lsample);
			Size lsz = StdSampleSize();
			ImageDraw iw(lsz);
			iw.DrawRect(lsz, Black());
			iw.Alpha().DrawRect(lsz, Black());
			DrawTLText(iw.Alpha(), 0, 0, lsz.cx, lsample, font, White());
			DoDragAndDrop(data, iw, DND_COPY);
			ClearSelection();
		}
		else
		if(modifier && IsMouseOverImage(pt)) {
		// Unfortunately, Turtle and VirtualGui (e.g. linux framebuffer)
		// backends do  not support image  drag-and-drop, at the moment.
		#if !defined(TURTLE) && !defined(VIRTUALGUI)
			Image isample = GetInlineImage(pt, modifier);
			Append(data, isample);
			Size isz = GetFitSize(isample.GetSize(), StdSampleSize());
			ImageDraw iw(isz);
			iw.DrawImage(isz, isample);
			DoDragAndDrop(data, iw, DND_COPY);
			ClearSelection();
		#endif
		}
	}
}

void TerminalCtrl::LeftDouble(Point pt, dword keyflags)
{
	if(IsMouseTracking(keyflags))
		Ctrl::LeftDouble(pt, keyflags);
	else {
		ClearSelection();
		if(IsSelectorMode())
			return;
		pt = ClientToPagePos(pt);
		if((keyflags & K_CTRL) == K_CTRL) {
			if(IsMouseOverImage(pt)) {
				Image img = GetInlineImage(pt, true);
				if(!IsNull(img))
					WhenImage(PNGEncoder().SaveString(img));
			}
			else
			if(IsMouseOverHyperlink(pt)) {
				String uri = GetHypertextContent(pt, true);
				if(!IsNull(uri))
					WhenLink(uri);
			}
			else
			if(IsMouseOverAnnotation(pt)) {
				EditAnnotation();
			}
		}
		else {
			Point pl, ph;
			if(GetWordSelection(pt, pl, ph)) {
				SetSelection(pl, ph, SEL_WORD, true);
			}
		}
	}
}

void TerminalCtrl::LeftTriple(Point pt, dword keyflags)
{
	if(IsMouseTracking(keyflags))
		Ctrl::LeftTriple(pt, keyflags);
	else {
		ClearSelection();
		if(IsSelectorMode())
			return;
		Point pl, ph;
		GetLineSelection(ClientToPagePos(pt), pl, ph);
		SetSelection(pl, ph, SEL_LINE);
		multiclick = IsSelection();
	}
}

void TerminalCtrl::MiddleDown(Point pt, dword keyflags)
{
	SetFocus();
	if(IsMouseTracking(keyflags))
		VTMouseEvent(pt, MIDDLEDOWN, keyflags);
	else {
		if(IsSelectorMode()) {
			ClearSelection();
		}
		else {
			WString w;
			if(IsSelection())
				w = GetSelectedText();
			else
			if(AcceptText(Selection()))
				w = GetWString(Selection());
			if(!IsNull(w))
				Paste(w);
		}
	}
}

void TerminalCtrl::MiddleUp(Point pt, dword keyflags)
{
	if(IsMouseTracking(keyflags) && !modes[XTX10MM])
		VTMouseEvent(pt, MIDDLEUP, keyflags);
}

void TerminalCtrl::RightDown(Point pt, dword keyflags)
{
	SetFocus();
	if(IsMouseTracking(keyflags))
		VTMouseEvent(pt, RIGHTDOWN, keyflags);
	else {
		pt = ClientToPagePos(pt);
		if(!IsSelected(pt))
			ClearSelection();
		MenuBar::Execute(WhenBar);
//		SetFocus();
	}
}

void TerminalCtrl::RightUp(Point pt, dword keyflags)
{
	if(IsMouseTracking(keyflags) && !modes[XTX10MM])
		VTMouseEvent(pt, RIGHTUP, keyflags);
}

void TerminalCtrl::MouseMove(Point pt, dword keyflags)
{
	auto sGetMouseMotionEvent = [](bool b) -> dword
	{
		if(!b) return Ctrl::MOUSEMOVE;
		if(GetMouseLeft()) return Ctrl::LEFTDRAG;
		if(GetMouseRight()) return Ctrl::RIGHTDRAG;
		if(GetMouseMiddle()) return Ctrl::MIDDLEDRAG;
		return 0;
	};

	pt = GetViewRect().Bind(pt);
	bool captured = HasCapture();

	if(IsMouseTracking(keyflags)) {
		if((modes[XTDRAGM] && captured) || modes[XTANYMM])
			VTMouseEvent(pt, sGetMouseMotionEvent(captured), keyflags);
	}
	else
	if(captured) {
		selpos = SelectionToPagePos(pt);
		Refresh();
	}
	else
	if(hyperlinks || annotations) {
		HighlightHypertext(ClientToPagePos(pt));
	}
}

void TerminalCtrl::MouseWheel(Point pt, int zdelta, dword keyflags)
{
	bool b = IsMouseTracking(keyflags);
	if(!b && page->HasHistory())
		sb.Wheel(zdelta, wheelstep);
	else
	if(zdelta != 0) {
		if(IsAlternatePage() &&
			(alternatescroll || (alternatescroll && modes[XTASCM])))
				VTKey(zdelta > 0 ? K_UP : K_DOWN, wheelstep);
		else
		if(b && !modes[XTX10MM])
			VTMouseEvent(pt, MOUSEWHEEL, keyflags, zdelta);
	}
}

Image TerminalCtrl::MouseEvent(int event, Point pt, int zdelta, dword keyflags)
{
	if(hidemousecursor) {
		if(mousehidden && event == Ctrl::CURSORIMAGE)
			return Null;
		else mousehidden = false;
	}
	return Ctrl::MouseEvent(event, pt, zdelta, keyflags);
}

void TerminalCtrl::VTMouseEvent(Point pt, dword event, dword keyflags, int zdelta)
{
	if(IsReadOnly())
		return;
	
	int  mouseevent = 0;

	// Some interactive applications, particularly those using a Text User Interface (TUI),
	// do not utilize the alternate buffer for drawing their interface (e.g., Far Manager on Windows).
	// This behavior leads to offset mouse position reports because the scroll(bar) position is updated
	// when the history or scrollback buffer is enabled. To address this issue, a workaround is implemented
	// by refraining from calculating the scrolled position with VT mouse events.
	
	if(!modes[XTSGRPXMM])
		pt = ClientToPagePos(pt, true) + 1;

	switch(event) {
	case LEFTUP:
	case LEFTDOWN:
		mouseevent = 0x00;
		break;
	case LEFTDRAG:
		if(pt == mousepos)
			return;
		mouseevent = 0x20;
		break;
	case MIDDLEUP:
	case MIDDLEDOWN:
		mouseevent = 0x01;
		break;
	case MIDDLEDRAG:
		if(pt == mousepos)
			return;
		mouseevent = 0x21;
		break;
	case RIGHTUP:
	case RIGHTDOWN:
		mouseevent = 0x02;
		break;
	case RIGHTDRAG:
		if(pt == mousepos)
			return;
		mouseevent = 0x22;
		break;
	case MOUSEMOVE:
		if(pt == mousepos)
			return;
		mouseevent = 0x23;
		break;
	case MOUSEWHEEL:
		mouseevent = zdelta > 0 ? 0x40 : 0x41;
		break;
	default:
		ReleaseCapture();
		return;
	}

	mousepos = pt;

	if(keyflags & K_SHIFT) mouseevent |= 0x04;
	if(keyflags & K_ALT)   mouseevent |= 0x08;
	if(keyflags & K_CTRL)  mouseevent |= 0x10;

	bool buttondown = false;

	if((event & UP) == UP) {
		if(HasCapture())
			ReleaseCapture();
	}
	else {
		buttondown = true;	// Combines everything else with a button-down event
		if((event & DOWN) == DOWN)
			if(!HasCapture())
				SetCapture();
	}

	if(modes[XTSGRMM] || modes[XTSGRPXMM]) {
		PutCSI(Format("<%d;%d;%d%[1:M;m]s", mouseevent, pt.x, pt.y, buttondown));
	}
	else {
		if(!buttondown)
			mouseevent = 0x03;
		mouseevent += 0x20;
		pt += 0x20;
		// Note: We can't use PutCSI method to send X11 and UTF mouse coordinates here as
		// it won't pass values >= 128 unmodified, unless the terminal is in  8-bit mode.
		if(modes[XTUTF8MM]) {
			WString s;
			s.Cat(clamp(pt.x, 32, 2047));
			s.Cat(clamp(pt.y, 32, 2047));
			PutRaw(Format("\033[M%c%s", mouseevent, ToUtf8(s))).Flush();
		}
		else {
			pt.x = clamp(pt.x, 32, 255);
			pt.y = clamp(pt.y, 32, 255);
			PutRaw(Format("\033[M%c%c%c", mouseevent, pt.x, pt.y)).Flush();
		}
	}
}

bool TerminalCtrl::IsMouseTracking(dword keyflags) const
{
	return (keyflags & overridetracking) != overridetracking
		&& (modes[XTX10MM]
		|| modes[XTX11MM]
		|| modes[XTANYMM]
		|| modes[XTDRAGM]);
}

Point TerminalCtrl::ClientToPagePos(Point pt, bool ignoresb) const
{
	Sizef csz = GetCellSize();
	return (Point) Pointf(pt.x / csz.cx, pt.y / csz.cy + (ignoresb ? 0 : GetSbPos()));
}

Point TerminalCtrl::SelectionToPagePos(Point pt) const
{
	// Aligns the anchor or selection point to cell boundaries.

	Size csz = GetCellSize();
	int mx = pt.x % csz.cx;
	pt.x += int(mx >= csz.cx / 2) * csz.cx - mx;
	return ClientToPagePos(pt);
}

void TerminalCtrl::SetSelection(Point pl, Point ph, dword type, bool multi_click)
{
	anchor = pl;
	selpos = ph;
	seltype = type;
	multiclick = multi_click;
	SetSelectionSource(ClipFmtsText());
	Refresh();
}

bool TerminalCtrl::GetSelection(Point& pl, Point& ph) const
{
	if(IsNull(anchor) || anchor == selpos) {
		pl = ph = selpos;
		return false;
	}
	
	if(anchor.y == selpos.y || anchor.x == selpos.x || seltype == SEL_RECT) {
		pl = min(anchor, selpos);
		ph = max(anchor, selpos);
	}
	else
	if(anchor.y > selpos.y) {
		pl = selpos;
		ph = anchor;
	}
	else {
		pl = anchor;
		ph = selpos;
	}

	if(seltype == SEL_LINE) {
		// Updates the horizontal highlight on display resize.
		ph.x = page->FetchLine(ph.y).GetCount();
	}
	return true;
}

Rect TerminalCtrl::GetSelectionRect() const
{
	Point pl, ph;
	return GetSelection(pl, ph) ? Rect(pl, ph) : Null;
}

void TerminalCtrl::ClearSelection()
{
	ReleaseCapture();
	anchor = Null;
	selpos = Null;
//	seltype = SEL_NONE;
	selecting = false;
	multiclick = false;
	Refresh();
}

bool TerminalCtrl::IsSelected(Point pt) const
{
	Point pl, ph;
	if(!GetSelection(pl, ph))
		return false;

	if(seltype == SEL_RECT) {
		return pt.x >= pl.x
			&& pt.y >= pl.y
			&& pt.x <  ph.x
			&& pt.y <= ph.y;
	}
	else
	if(pl.y == ph.y) {
		return pt.y == pl.y
			&& pt.x >= pl.x
			&& pt.x <  ph.x;
	}
	else
	if(pt.y == pl.y) {
		Size psz = GetPageSize();
		return pt.x >= pl.x
			&& pt.x <  psz.cx;
	}
	else
	if(pt.y == ph.y) {
		return pt.x >= 0 && pt.x < ph.x;
	}

	return pl.y <= pt.y && pt.y <= ph.y;
}

WString TerminalCtrl::GetSelectedText() const
{
	return AsWString((const VTPage&)*page, GetSelectionRect(), seltype == SEL_RECT);
}

bool TerminalCtrl::GetLineSelection(const Point& pt, Point& pl, Point& ph) const
{
	pl = ph = pt;
	
	Tuple<int, int> span = page->GetLineSpan(pt.y);
	pl.x = 0;
	pl.y = span.a;
	ph.x = GetPageSize().cx;
	ph.y = span.b;

	if(pl.y == ph.y)
		ph.x = page->FetchLine(ph.y).GetCount();

	return !(pl == ph);
}

bool TerminalCtrl::GetWordSelection(const Point& pt, Point& pl, Point& ph) const
{
	pl = ph = pt;
	
	const VTLine& line = page->FetchLine(pt.y);
	if(!line.IsVoid()) {
		const VTCell& cell = page->FetchCell(pt);
		if(!cell.IsVoid() && IsWCh(cell, line.IsWrapped(), cellfilter)) {
			ph.x++;
			GetWordPosL(line, pl);
			GetWordPosH(line, ph);
			return true;
		}
	}

	return false;
}

void TerminalCtrl::GetWordPosL(const VTLine& line, Point& pl) const
{
	bool stopped = false;
	bool wrapped = line.IsWrapped();

	while(pl.x > 0 && !(stopped = !IsWCh(line[pl.x - 1], wrapped, cellfilter)))
		pl.x--;

	if(pl.x == 0 && !stopped) {
		const VTLine& prev = page->FetchLine(pl.y - 1);
		if(prev.IsWrapped()) {
			pl.x = prev.GetCount();
			pl.y--;
			GetWordPosL(prev, pl);
		}
	}
}

void TerminalCtrl::GetWordPosH(const VTLine& line, Point& ph) const
{
	bool stopped = false;
	bool wrapped = line.IsWrapped();

	while(ph.x < line.GetCount() && !(stopped = !IsWCh(line[ph.x], wrapped, cellfilter)))
		ph.x++;

	if(ph.x == line.GetCount() && !stopped) {
		const VTLine& next = page->FetchLine(ph.y + 1);
		if(line.IsWrapped()) {
			ph.x = 0;
			ph.y++;
			GetWordPosH(next, ph);
		}
	}
}

void TerminalCtrl::BeginSelectorMode()
{
	if(IsSelectorMode())
		return;
	selectormode = true;
	ClearSelection();
	seltype = SEL_NONE;
	anchor = selpos = cursor = GetCursorPos();
	caretbackup = clone(caret);
	caret.Unlock().Beam().Blink(false).Lock();
	PlaceCaret();
}

void TerminalCtrl::EndSelectorMode()
{
	ClearSelection();
	cursor = Null;
	seltype = SEL_NONE;
	caret = clone(caretbackup);
	selectormode = false;
	PlaceCaret();
}

Image TerminalCtrl::GetInlineImage(Point pt, bool modifier)
{
	if(modifier) {
		const VTCell& cell = page->FetchCell(pt);
		if(cell.IsImage()) {
			Image img = GetCachedImageData(cell.chr, Null, GetCellSize()).image;
			if(!IsNull(img))
				return pick(img);
			LLOG("Unable to retrieve image from cache. Image id: " << cell.chr);
		}
	}
	return Null;
}

String TerminalCtrl::GetHypertextContent(Point pt, bool modifier)
{
	if(modifier) {
		const VTCell& cell = page->FetchCell(pt);
		if(cell.IsHypertext()) {
			String htxt = GetCachedHypertext(cell.data);
			if(!IsNull(htxt))
				return htxt;
			LLOG("Unable to retrieve hypertext from the hypertext cache. Htext id: " << cell.data);
		}
	}
	return Null;
}

void TerminalCtrl::HighlightHypertext(Point pt)
{
	if(mousepos != pt) {
		mousepos = pt;
		const VTCell& cell = page->FetchCell(pt);
		if((!HasAnnotations() && cell.IsAnnotation())
		|| (!HasHyperlinks() && cell.IsHyperlink())) {
			activehtext = 0;
			return;
		}
		if(cell.IsHypertext() || activehtext > 0) {
			if(cell.data != activehtext) {
				prevhtext = activehtext;
				activehtext = cell.data;
				RefreshDisplay();
			}
			String htxt = GetCachedHypertext(activehtext);
			Tip(cell.IsAnnotation() ? "\1[g " + htxt + " ]" : htxt); // Use qtf for annotations.
		}
		else {
			Tip("");
		}
	}
}

bool TerminalCtrl::SelectAnnotatedCells(Point pt, const Event<VTCell&>& fn)
{
	const VTCell& cell = page->FetchCell(pt);
	if(cell.IsVoid() || !cell.IsAnnotation())
		return false;
	auto f = GetWordSelectionFilter();
	SetWordSelectionFilter([](const VTCell& cell) { return cell.IsAnnotation(); });
	Point pl, ph;
	bool ok = GetWordSelection(pt, pl, ph);
	SetWordSelectionFilter(f);
	if(ok)
		page->FetchCellsMutable(pl, ph, fn);
	return ok;
}

void TerminalCtrl::AddAnnotation(const String& txt)
{
	Point pl, ph;
	if(!IsNull(txt) && GetSelection(pl, ph)) {
		dword id = RenderHypertext(txt);
		page->FetchCellsMutable(pl, ph, [id](VTCell& cell) {
			cell.Image(false).Hyperlink(false).Annotation().data = id;
		});
		RefreshDisplay();
	}
}

void TerminalCtrl::AddAnnotation()
{
	String txt;
	if(WhenAnnotation(GetMouseViewPos(), txt) && !IsNull(txt))
		AddAnnotation(txt);
}

void TerminalCtrl::EditAnnotation()
{
	if(!IsMouseOverAnnotation(mousepos))
		return;
	dword id = page->FetchCell(mousepos).data;
	String txt = GetCachedHypertext(id);
	if(!IsNull(txt) && WhenAnnotation(GetMouseViewPos(), txt)) {
		id = RenderHypertext(txt);
		SelectAnnotatedCells(mousepos, [id](VTCell& cell) {
			cell.Image(false).Hyperlink(false).Annotation().data = id;
		});
		RefreshDisplay();
	}
}

void TerminalCtrl::DeleteAnnotation()
{
	SelectAnnotatedCells(mousepos, [](VTCell& cell) {
		if(cell.IsAnnotation())
			cell.Annotation(false).data = 0;
	});
	RefreshDisplay();
}

void TerminalCtrl::ShowAnnotation(Point pt, const String& s)
{
	auto& p = Single<ToolTip>();
	if(s.IsEmpty()) {
		HideAnnotation();
		return;
	}

	p.Set(s);
	Size sz = p.GetMinSize();
	Rect r = GetMouseWorkArea();
	pt = GetMousePos();
	pt.x = max(pt.x + sz.cx > r.right ? pt.x - sz.cx : pt.x + Zx(4), r.left);
	pt.y = max(pt.y + sz.cy > r.bottom ? pt.y - sz.cy: pt.y + Zy(4), r.top);
	if(!p.IsOpen())
		p.PopUp(this, pt, false);
	p.Refresh();
}

void TerminalCtrl::HideAnnotation()
{
	auto& p = Single<ToolTip>();
	if(p.IsOpen())
		p.Close();
}

void TerminalCtrl::Search(const WString& s, int begin, int end, bool visibleonly, bool co,
								Gate<const VectorMap<int, WString>&, const WString&> fn)
{
	if(searching || s.IsEmpty() || begin >= end)
		return;
	
	searching = true;
	
	if(visibleonly) {
		begin = GetSbPos() + max(begin, 0);
		end   = min(end, min(begin + GetPageSize().cy, page->GetLineCount()));
	}
	else {
		begin = max(begin, 0);
		end   = min(end, page->GetLineCount());
	}
	
	auto ScanBuffer = [this, &s, &fn](int i, int& o) {
		VectorMap<int, WString> m;
		o = page->FetchLine(i, m) + 1;
		return m.IsEmpty() || fn(m, s);
	};

	if(co) {
		LTIMING("TerminalCtrl::Search (MT)");
		Vector<int> cache;
		while(begin < end) {
			auto t = page->GetLineSpan(begin);
			cache.Add(t.a);
			begin = t.b + 1;
		}
		CoFor(cache.GetCount(), [&](int i) {
			if(ScanBuffer(cache[i], i))
				return;
		});
	}
	else {
		LTIMING("TerminalCtrl::Search (ST)");
		while(begin < end)
			if(ScanBuffer(begin, begin))
				break;
	}

	searching = false;
}

void TerminalCtrl::Find(const WString& s, int begin, int end, bool visibleonly,
						Gate<const VectorMap<int, WString>&, const WString&> fn)
{
	Search(s, begin, end, visibleonly, false, fn);
}

void TerminalCtrl::Find(const WString& s, bool visibleonly,
	Gate<const VectorMap<int, WString>&, const WString&> fn)
{
	Search(s, 0, page->GetLineCount(), visibleonly, false, fn);
}

void TerminalCtrl::CoFind(const WString& s, int begin, int end, bool visibleonly,
						Gate<const VectorMap<int, WString>&, const WString&> fn)
{
	Search(s, begin, end, visibleonly, true, fn);
}

void TerminalCtrl::CoFind(const WString& s, bool visibleonly,
	Gate<const VectorMap<int, WString>&, const WString&> fn)
{
	Search(s, 0, page->GetLineCount(), visibleonly, true, fn);
}

void TerminalCtrl::StdBar(Bar& menu)
{
	menu.Sub(t_("Options"), [=](Bar& menu) { OptionsBar(menu); });
	menu.Separator();
	menu.Add(t_("Read only"), [=] { SetEditable(IsReadOnly()); })
		.Key(K_SHIFT_CTRL_L)
		.Check(IsReadOnly());
	if(IsMouseOverImage()) {
		menu.Separator();
		ImagesBar(menu);
	}
	else
	if(IsMouseOverHyperlink()) {
		menu.Separator();
		LinksBar(menu);
	}
	else
	if(IsMouseOverAnnotation()) {
		menu.Separator();
		AnnotationsBar(menu);
	}
	else {
		menu.Separator();
		EditBar(menu);
	}
}

void TerminalCtrl::EditBar(Bar& menu)
{
	bool b = IsEditable() && !IsSelectorMode();
	bool q = b && annotations && IsSelection() && !IsSelectorMode();
	menu.Add(IsSelection(), AK_COPY,  [=] { Copy();  });
	menu.Add(b, AK_PASTE, CtrlImg::paste(), [=] { Paste(); });
	menu.Separator();
	if(HasAnnotations()) {
		menu.Add(q, AK_ANNOTATE, [=] { AddAnnotation(); });
		menu.Separator();
	}
	menu.Add(b, AK_SELECTALL, CtrlImg::select_all(), [=] { SelectAll(); });
	menu.Separator();
	menu.Add(t_("Selector mode"),[=] { IsSelectorMode() ? EndSelectorMode() : BeginSelectorMode(); })
		.Check(IsSelectorMode())
		.Key(K_SHIFT_CTRL_X);
}

void TerminalCtrl::LinksBar(Bar& menu)
{
	if(!HasHyperlinks())
		return;
	
	String uri = GetHyperlinkUri();
	if(IsNull(uri))
		return;

	menu.Add(AK_COPYLINK, CtrlImg::copy(), [=] { Copy(uri.ToWString()); });
	menu.Add(AK_OPENLINK, CtrlImg::open(), [=] { WhenLink(uri); });
}

void TerminalCtrl::AnnotationsBar(Bar& menu)
{
	if(!HasAnnotations())
		return;
	
	String txt = GetAnnotationText();
	if(IsNull(txt))
		return;

	bool b = IsEditable() && !IsSelectorMode();
	menu.Add(AK_COPYANNOTATION, CtrlImg::copy(),       [=] { Copy(txt.ToWString()); });
	menu.Add(b, AK_EDITANNOTATION, CtrlImg::open(),    [=] { EditAnnotation();   });
	menu.Add(b, AK_DELETEANNOTATION, CtrlImg::remove(),[=] { DeleteAnnotation(); });
}

void TerminalCtrl::ImagesBar(Bar& menu)
{
	if(!HasInlineImages())
		return;
	
	Point pt = mousepos;

	menu.Add(AK_COPYIMAGE, CtrlImg::copy(), [=]
		{
			Image img = GetInlineImage(pt, true);
			if(!IsNull(img))
				AppendClipboardImage(img);
		});
	menu.Add(AK_OPENIMAGE, CtrlImg::open(), [=]
		{
			Image img = GetInlineImage(pt, true);
			if(!IsNull(img))
				WhenImage(PNGEncoder().SaveString(img));
		});
}

void TerminalCtrl::OptionsBar(Bar& menu)
{
	bool inlineimages = jexerimages || sixelimages || iterm2images;

	menu.Sub(t_("Cursor style"), [=](Bar& menu)
		{
			byte cstyle   = caret.GetStyle();
			bool unlocked = !caret.IsLocked() &&  !IsSelectorMode();
			menu.Add(unlocked,
				t_("Block"),
				[=] { caret.Block(caret.IsBlinking()); })
				.Radio(cstyle == Caret::BLOCK);
			menu.Add(unlocked,
				t_("Beam"),
				[=] { caret.Beam(caret.IsBlinking()); })
				.Radio(cstyle == Caret::BEAM);
			menu.Add(unlocked,
				t_("Underline"),
				[=] { caret.Underline(caret.IsBlinking()); })
				.Radio(cstyle == Caret::UNDERLINE);
			menu.Separator();
			menu.Add(unlocked,
				t_("Blinking"),
				[=] { caret.Blink(!caret.IsBlinking());	 })
				.Check(caret.IsBlinking());
			menu.Separator();
			menu.Add(t_("Locked"),
				[=] { caret.Lock(!caret.IsLocked()); })
				.Check(!unlocked);
		});
	menu.Separator();
	menu.Add(AK_SCROLLBAR,
		[=] { ShowScrollBar(!sb.IsChild()); })
		.Check(sb.IsChild());
	menu.Add(AK_ALTERNATESCROLL,
		[=] { AlternateScroll(!alternatescroll); })
		.Check(alternatescroll);
	menu.Add(AK_KEYNAVIGATION,
		[=] { KeyNavigation(!keynavigation); })
		.Check(keynavigation);
	menu.Add(AK_VTFUNCTIONKEYS,
		[=] { PCStyleFunctionKeys(!pcstylefunctionkeys); })
		.Check(pcstylefunctionkeys);
	menu.Add(AK_AUTOSCROLL,
		[=] { ScrollToEnd(!scrolltoend); })
		.Check(scrolltoend);
	menu.Add(AK_HIDEMOUSE,
		[=] { AutoHideMouseCursor(!hidemousecursor); })
		.Check((hidemousecursor));
	menu.Add(AK_BLINKINGTEXT,
		[=] { BlinkingText(!blinkingtext); })
		.Check(blinkingtext);
	menu.Add(AK_REVERSEWRAP,
		[=] { ReverseWrap(!reversewrap); })
		.Check(reversewrap);
	menu.Add(AK_HYPERLINKS,
		[=] { Hyperlinks(!hyperlinks); })
		.Check(hyperlinks);
	menu.Add(AK_ANNOTATIONS,
		[=] { Annotations(!annotations); })
		.Check(annotations);
	menu.Add(AK_INLINEIMAGES,
		[=] { InlineImages(!inlineimages); })
		.Check(inlineimages);
	menu.Add(AK_DYNAMICCOLORS,
		[=] { DynamicColors(!dynamiccolors); })
		.Check(dynamiccolors);
	menu.Add(AK_BRIGHTCOLORS,
		[=] { LightColors(!lightcolors); })
		.Check(lightcolors);
	menu.Add(AK_ADJUSTCOLORS,
		[=] { AdjustColors(!adjustcolors); })
		.Check(adjustcolors);
	menu.Add(AK_SIZEHINT,
		[=] { ShowSizeHint(!sizehint); })
		.Check(sizehint);
	menu.Add(AK_BUFFEREDREFRESH,
		[=] { DelayedRefresh(!delayedrefresh); })
		.Check(delayedrefresh);
	menu.Add(AK_LAZYRESIZE,
		[=] { LazyResize(!lazyresize); })
		.Check(lazyresize);
}

TerminalCtrl& TerminalCtrl::ShowScrollBar(bool b)
{
	GuiLock __;

	if(!sb.IsChild() && b) {
		ignorescroll = true;
		AddFrame(sb.AutoDisable());
	}
	else
	if(sb.IsChild() && !b) {
		ignorescroll = true;
		RemoveFrame(sb);
	}
	ignorescroll = false;
	return *this;
}

Image TerminalCtrl::CursorImage(Point p, dword keyflags)
{
	if(IsMouseTracking(keyflags))
		return Image::Arrow();
	else
	if((HasAnnotations() && IsMouseOverAnnotation())
	|| (HasHyperlinks()  && IsMouseOverHyperlink()))
		return Image::Hand();
	else
		return Image::IBeam();
}

void TerminalCtrl::State(int reason)
{
	if(reason == Ctrl::OPEN)
		WhenResize();
}

int TerminalCtrl::ReadInt(const String& s, int def)
{
	const char *p = ~s;
	int c = 0, n = 0;
	while(*p && dword((c = *p++) - '0') < 10)
		n = n * 10 + (c - '0');
	return n < 1 ? def : n;
}

TerminalCtrl::Caret::Caret()
: style(BLOCK)
, blinking(true)
, locked(false)
{
}

TerminalCtrl::Caret::Caret(int style_, bool blink, bool lock)
{
	Set(style_, blink);
	locked = lock;
}

void TerminalCtrl::Caret::Set(int style_, bool blink)
{
	if(!locked) {
		style = clamp(style_, int(BLOCK), int(UNDERLINE));
		blinking = blink;
		WhenAction();
	}
}

void TerminalCtrl::Caret::Serialize(Stream& s)
{
	int version = 1;
	s / version;
	if(version >= 1) {
		s % style;
		s % locked;
		s % blinking;
	}
}

void TerminalCtrl::Caret::Jsonize(JsonIO& jio)
{
	jio ("Style", style)
		("Locked", locked)
		("Blinking", blinking);
}

void TerminalCtrl::Caret::Xmlize(XmlIO& xio)
{
	XmlizeByJsonize(xio, *this);
}

INITBLOCK
{
	Value::Register<TerminalCtrl::InlineImage>();
}

}
