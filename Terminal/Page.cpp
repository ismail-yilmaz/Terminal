#include "Page.h"

#define LLOG(x)		// RLOG("VTPage [#" << this << "]: " << x)
#define LTIMING(x)	// RTIMING(x)

namespace Upp {

VTLine::VTLine()
: invalid(true)
, wrapped(false)
{
}

VTLine::VTLine(const VTLine& src, int)
{
	static_cast<Vector<VTCell>&>(*this) = clone(static_cast<const Vector<VTCell>&>(src));
	invalid = src.invalid;
	wrapped = src.wrapped;
}

void VTLine::Adjust(int cx, const VTCell& filler)
{
	if(cx < GetCount())
		wrapped = false;
	SetCount(cx, filler);
	invalid = true;
}

void VTLine::Grow(int cx, const VTCell& filler)
{
	if(cx > GetCount()) {
		wrapped = false;
		SetCount(cx, filler);
		invalid = true;
	}
}

void VTLine::Shrink(int cx)
{
	if(cx < GetCount()) {
		wrapped = false;
		SetCount(cx);
		invalid = true;
	}
}

void VTLine::ShiftLeft(int begin, int end, int n, const VTCell& filler)
{
	Insert(end, filler, n);
	Remove(begin - 1, n);
	wrapped = false;
	invalid = true;
}

void VTLine::ShiftRight(int begin, int end, int n, const VTCell& filler)
{
	Insert(begin - 1, filler, n);
	Remove(end, n);
	wrapped = false;
	invalid = true;
}

bool VTLine::FillLeft(int begin, const VTCell& filler, dword flags)
{
	for(int i = 1; i <= clamp(begin, 1, GetCount()); i++)
		At(i - 1).Fill(filler, flags);
	invalid = true;
	return true;
}

bool VTLine::FillRight(int begin, const VTCell& filler, dword flags)
{
	for(int i = max(1, begin); i <= GetCount(); i++)
		At(i - 1).Fill(filler, flags);
	invalid = true;
	return true;
}

bool VTLine::Fill(int begin, int end, const VTCell& filler, dword flags)
{
	int n = GetCount();
	int b = clamp(begin, 1, n);
	int e = clamp(end,   1, n);

	for(int i = b; i <= e; i++)
		At(i - 1).Fill(filler, flags);

	bool done = b <= e;
	if(done)
		invalid = true;
	return done;
}

bool VTLine::FillLine(const VTCell& filler, dword flags)
{
	for(VTCell& l : static_cast<Vector<VTCell>&>(*this))
		l.Fill(filler, flags);
	invalid = true;
	return true;
}

const VTLine& VTLine::Void()
{
	static VTLine line;
	return line;
}

int VTLine::GetLength() const
{
	return Upp::GetLength(*this, 0, GetCount());
}

int VTLine::GetOffset() const
{
	return Upp::GetOffset(*this, 0, GetCount());
}

String VTLine::ToString() const
{
	return ToWString().ToString();
}

WString VTLine::ToWString() const
{
	return AsWString(SubRange(Begin(), End()));
}


WString AsWString(VTLine::ConstRange& cellrange, bool tspaces)
{
	WString txt;
	int j = 0;
	for(const VTCell& cell : cellrange) {
		if((cell.chr == 0 && tspaces) || cell.IsImage())
			j++;
		else {
			if(j) {
				txt.Cat(' ', j);
				j = 0;
			}
			if(cell.chr >= 32)
				txt.Cat(cell.chr, 1);
		}
	}
	return pick(txt);
}

int GetLength(const VTLine& line, int begin, int end)
{
	int length = 0;
	for(int i = max(0, begin); i < min(end, line.GetCount()); i++) {
		const VTCell& cell = line[i];
		length += cell >= 32 || cell == 0;
	}
	return length;
}

int GetOffset(const VTLine& line, int begin, int end)
{
	int offset = 0;
	for(int i = max(0, begin); i < min(end, line.GetCount()); i++)
		offset += line[i] < 32; // Include all the special values in calculation (such as 1 = double width char, second half)
	return offset;
}

VTPage::VTPage()
: tabsize(8)
, autowrap(false)
, reversewrap(false)
, history(false)
, historysize(1024)
, size(2, 2)
, margins(Null)
, ambiguouscellwidth(1)
{
	Reset();
}

VTPage& VTPage::Reset()
{
	cellattrs = VTCell();
	cursor = Null;
	backup = Null;
	tabsync = false;
	SetTabs(tabsize);
	SetSize(size);
	AutoWrap(autowrap);
	Displaced(false);
	ErasePage();
	EraseHistory();
	MoveTopLeft();
	return *this;
}

VTPage& VTPage::Backup()
{
	backup = cursor;

	LLOG("Backup() -> " << cursor);
	return *this;
}

VTPage& VTPage::Discard()
{
	LLOG("Discard() -> " << backup);

	backup = Null;
	return *this;
}

VTPage& VTPage::Restore()
{
	cursor = backup;
	if(!autowrap)
		ClearEol();

	LLOG("Restore() -> " << cursor);
	return *this;
}

VTPage& VTPage::Displaced(bool b)
{
	LLOG("Displaced(" << b << ")");

	cursor.displaced = b;
	if(!b) ResetMargins();
	ClearEol();
	return *this;
}

VTPage& VTPage::AutoWrap(bool b)
{
	// Clear EOL flag only on reset
	// See: DEC STD 070-D, p. D-13

	LLOG("AutoWrap(" << b << ")");

	autowrap = b;
	if(!b) ClearEol();
	return *this;
}

VTPage& VTPage::ReverseWrap(bool b)
{
	// TODO
	LLOG("ReverseWrap(" << b << ")");

	reversewrap = b;
	if(!b) ClearEol();
	return *this;
}

VTPage& VTPage::History(bool b)
{
	LLOG("History(" << b << ")");

	history = b;
	return *this;
}

void VTPage::EraseHistory()
{
	saved.Clear();
	saved.Shrink();
	lines.Shrink();
	WhenUpdate();
}

void VTPage::SetHistorySize(int sz)
{
	LLOG("HistorySize(" << sz << ")");

	historysize = max(1, sz);
	AdjustHistorySize();
}

void VTPage::AdjustHistorySize(int n)
{
	const int count = saved.GetCount() + n;
	if(count > historysize) {
		saved.DropHead(count - historysize);
		LLOG("AdjustHistorySize() -> Before: " << count << ", after: " << saved.GetCount());
	}
}

bool VTPage::SaveToHistory(int pos, int n)
{
	if(margins != GetView() || n <= 0)
		return false;
	AdjustHistorySize(n);
	for(int i = 0; i < n; i++)
		saved.AddTail(pick(lines[pos - 1 + i]));
	return true;
}

void VTPage::UnwindHistory(const Size& prevsize)
{
	int delta =  min(size.cy - prevsize.cy, saved.GetCount()), n = delta;
	if(delta <= 0 )
		return;
	lines.InsertN(0, delta);
	while(delta-- > 0) {
		lines[delta] = pick(saved.Tail());
		saved.DropTail();
	}
	cursor.y += n;
}

void VTPage::RewindHistory(const Size& prevsize)
{
	int delta = min(cursor.y - size.cy, lines.GetCount());
	while(delta-- > 0) {
		saved.AddTail(pick(lines[0]));
		lines.Remove(0, 1);
	}
}

VTPage& VTPage::SetSize(Size sz)
{
	Size oldsize = GetSize();
	size = Nvl(sz, Size(2, 2));
	if(oldsize != size || HorzMarginsExist() || VertMarginsExist())
		ResetMargins();
	if(lines.IsEmpty())
		cursor.Clear();
	if(HasHistory()) {
		if(oldsize.cy < size.cy)
			UnwindHistory(oldsize);
		else
		if(oldsize.cy > size.cy)
			RewindHistory(oldsize);
		AdjustHistorySize();
	}
	lines.SetCount(size.cy);
	for(VTLine& line : lines) {
		line.Grow(size.cx, cellattrs);
		line.Invalidate();
	}
	if(tabsync)
		SetTabs(tabsize);
	return MoveTo(cursor);
}

Point VTPage::GetPos() const
{
	Point pt(cursor);
	pt.y += saved.GetCount();

	LLOG("GetPos() -> " << pt);
	return pt;
}

Point VTPage::GetRelPos() const
{
	Point pt(cursor);

	if(cursor.displaced) {
		pt.x += 1 - margins.left;
		pt.y += 1 - margins.top;
	}

	LLOG("GetRelPos() -> " << pt);
	return pt;
}

inline Point VTPage::Bind(const Rect& r, const Point& pt) const
{
	return Point(pt.x < r.left ? r.left : pt.x > r.right ? r.right : pt.x,
                 pt.y < r.top ? r.top : pt.y > r.bottom ? r.bottom : pt.y);
}

inline bool VTPage::ViewContains(const Point& pt) const
{
	Rect view = GetView();
	return view.left   <= pt.x
		&& view.right  >= pt.x
		&& view.top    <= pt.y
		&& view.bottom >= pt.y;
}

inline bool VTPage::MarginsContain(const Point& pt) const
{
	return margins.left   <= pt.x
		&& margins.right  >= pt.x
		&& margins.top    <= pt.y
		&& margins.bottom >= pt.y;
}

void VTPage::Invalidate(int begin, int end)
{
	int b = min(begin, end);
	int e = max(begin, end);
	for(int i = b; i < e; i++)
		lines[i].Invalidate();
}

VTPage& VTPage::SetCell(int x, int y, const VTCell& cell)
{
	if(ViewContains(Point(x, y)))
	{
		VTLine& line = lines[y - 1];
		line[x - 1] = cell;
		line.Invalidate();
	}
	return *this;
}

const VTCell& VTPage::GetCell(int x, int y) const
{
	return ViewContains(Point(x, y)) ? lines[y - 1][x - 1] : VTCell::Void();
}

int VTPage::CellAdd(const VTCell& cell, int width)
{
	if(width <= 0)
		return cursor.x;
	
	if(autowrap && cursor.eol)
	{
		lines[cursor.y - 1].Wrap();
		NewLine();
	}

	SetCell(cell);
	int next = cursor.x + 1;

	if(next <= margins.right)
	{
		MoveRight();
		if(width == 2)
		{
			VTCell ext = cell;
			ext.chr = 1;
			next = CellAdd(ext);
		}
	}
	else
		SetEol();

	return next;
}

VTPage& VTPage::InsertCell(const VTCell& cell)
{
	LLOG("InsertCell()");

	int width = cell.GetWidth(ambiguouscellwidth);
	if(width > 0) InsertCells(cursor.x, width);
	CellAdd(cell, width);
	return *this;
}

VTPage& VTPage::RepeatCell(int n)
{
	LLOG("RepeatCell(" << n << ")");

	TryShrinkCurrentLine();
	const VTCell& cell = GetCell(cursor.x - 1, cursor.y);
	for(int i = 0, w = cell.GetWidth(ambiguouscellwidth); i < n; i++)
		CellAdd(cell, w);
	return *this;
}

VTPage& VTPage::RewrapCursor(int n)
{
	LLOG("RewrapCursor(" << n << ")");

	int maxrewrap = margins.Width() * margins.Height() * 4;

	return margins.top == cursor.y
		? MoveBottomRight().MoveLeft(min(n - 1, maxrewrap))
		: MoveEnd().MoveUp().MoveLeft(min(n - 1, maxrewrap));
}

VTPage& VTPage::MoveHorz(int pos, dword flags)
{
	Rect view = GetView();
	int  offset = 1;
	int  left   = 1;
	int  right  = 1;
	bool scrollable = flags & Cursor::Scroller;

	LTIMING("VTPage::MoveHorz");

	if(flags & Cursor::Displaceable && cursor.displaced) {
		offset = left = margins.left;
		right  = margins.right;
	}
	else
	if(flags & Cursor::Marginal && HorzMarginsExist()) {
		if(HorzMarginsContain(cursor.x)) {
			offset = left = margins.left;
			right  = margins.right;
		}
		else {
			if(cursor.x < margins.left) {
				offset = left = view.left;
				right  = margins.right;
				
			}
			else
			if(cursor.x > margins.right) {
				left   = margins.left;
				right  = view.right;
				offset = view.left;
			}
			if(scrollable)
				scrollable = false;
		}
	}
	else { // flags == Cursor::Absolute
		offset = left = view.left;
		right  = view.right;
	}

	pos = GetNextColPos(pos, offset, flags & Cursor::Relative);
	
	if(reversewrap && (flags & Cursor::ReWrapper) && pos < left) {
		return RewrapCursor(left - pos);
	}
	
	cursor.x = clamp(pos, left, right);

	if(scrollable) {
		if(pos < left)
			CellInsert(left, left - pos, cellattrs, true);
		else
		if(pos > right)
			CellRemove(left, pos - right, cellattrs, true);
	}

	ClearEol();
	return *this;
}

VTPage& VTPage::MoveVert(int pos, dword flags)
{
	Rect view = GetView();
	int  offset = 1;
	int  top    = 1;
	int  bottom = 1;
	bool scrollable = flags & Cursor::Scroller;

	LTIMING("VTPage::MoveVert");

	if(flags & Cursor::Displaceable && cursor.displaced) {
		offset = top = margins.top;
		bottom = margins.bottom;
	}
	else
	if(flags & Cursor::Marginal && VertMarginsExist()) {
		if(VertMarginsContain(cursor.y)) {
			offset = top = margins.top;
			bottom = margins.bottom;
		}
		else {
			if(cursor.y < margins.top) {
				offset = top = view.top;
				bottom = margins.bottom;
			}
			else
			if(cursor.y > margins.bottom) {
				top    = margins.top;
				bottom = view.bottom;
				offset = view.top;
			}
			if(scrollable)
				scrollable = false;
		}
	}
	else { // flags == Cursor::Absolute
		offset = top = view.top;
		bottom = view.bottom;
	}

	pos = GetNextRowPos(pos, offset, flags & Cursor::Relative);
	cursor.y = clamp(pos, top, bottom);

	if(scrollable) {
		if(pos < top) {
			if(LineInsert(top, top - pos, cellattrs) > 0)
				WhenUpdate();
		}
		else
		if(pos > bottom) {
			if(LineRemove(top, pos - bottom, cellattrs) > 0)
				WhenUpdate();
		}
	}

	ClearEol();
	return *this;
}

VTPage& VTPage::MoveTo(int x, int y)
{
	LLOG("MoveTo(" << x << ", " << y << ")");

	return MoveVert(y, Cursor::Displaceable)
          .MoveHorz(x, Cursor::Displaceable);
}

VTPage& VTPage::MoveToLine(int n, bool relative)
{
	LLOG("MoveToLine(" << n <<")");

	return MoveVert(n,
		Cursor::Displaceable
		| (relative ? Cursor::Relative : 0));
}

VTPage& VTPage::MoveToColumn(int n, bool relative)
{
	LLOG("MoveToColumn(" << n << ")");
;
	return MoveHorz(n,
		Cursor::Displaceable
		| (relative ? Cursor::Relative : 0));
}

VTPage& VTPage::MoveUp(int n)
{
	LLOG("MoveUp(" << n << ")");

	return MoveVert(-n,
		Cursor::Marginal
		| Cursor::Relative
		| Cursor::Displaceable);
}

VTPage& VTPage::MoveDown(int n)
{
	LLOG("MoveDown(" << n << ")");

	return MoveVert(n,
		Cursor::Marginal
		| Cursor::Relative
		| Cursor::Displaceable);
}

VTPage& VTPage::MoveRight(int n)
{
	LLOG("MoveRight(" << n << ")");

	return MoveHorz(n,
		Cursor::Marginal
		| Cursor::Relative
		| Cursor::Displaceable);
}

VTPage& VTPage::MoveLeft(int n)
{
	LLOG("MoveLeft(" << n << ")");

	return MoveHorz(-n,
		Cursor::Marginal
		| Cursor::Relative
		| Cursor::ReWrapper
		| Cursor::Displaceable);
}

VTPage& VTPage::MoveHome()
{
	LLOG("MoveHome()");

	return MoveHorz(1,
		Cursor::Marginal
		| Cursor::Displaceable);
}

VTPage& VTPage::MoveEnd()
{
	LLOG("MoveEnd()");

	return MoveHorz(size.cx,
		Cursor::Marginal
		| Cursor::Displaceable);
}

VTPage& VTPage::MoveTopLeft()
{
	LLOG("MoveTopLeft()");

	dword flags = Cursor::Marginal
                | Cursor::Displaceable;

	return MoveHorz(1, flags)
		  .MoveVert(1, flags);
}

VTPage& VTPage::MoveBottomRight()
{
	LLOG("MoveBottomRight()");

	dword flags = Cursor::Marginal
                | Cursor::Displaceable;

	return MoveHorz(size.cx, flags)
		  .MoveVert(size.cy, flags);
}

VTPage& VTPage::NextLine(int n)
{
	LLOG("NextLine(" << n << ")");

	return MoveVert(n,
		Cursor::Marginal
		| Cursor::Relative
		| Cursor::Scroller
		| Cursor::Displaceable);
}

VTPage& VTPage::PrevLine(int n)
{
	LLOG("PrevLine(" << n << ")");

	return MoveVert(-n,
		Cursor::Marginal
		| Cursor::Relative
		| Cursor::Scroller
		| Cursor::Displaceable);
}

VTPage& VTPage::NextColumn(int n)
{
	LLOG("NextColumn(" << n << ")");

	return MoveHorz(n,
		Cursor::Marginal
		| Cursor::Relative
		| Cursor::Scroller
		| Cursor::Displaceable);
}

VTPage& VTPage::PrevColumn(int n)
{
	LLOG("PrevColumn(" << n << ")");

	return MoveHorz(-n,
		Cursor::Marginal
		| Cursor::Relative
		| Cursor::Scroller
		| Cursor::Displaceable);
}

int VTPage::LineInsert(int pos, int n, const VTCell& attrs)
{
	int scrolled = 0;

	n = clamp(n, 0, margins.bottom - pos + 1);

	LTIMING("VTPage::LineInsert");

	if(n > 0)
	{
		LLOG("LineInsert(" << pos << ") -> scrolling " << n << " lines up");

		if(HorzMarginsExist())
		{
			Rect r = margins;
			r.top = pos;
			r.bottom -= n;
			RectCopy(Point(r.left, pos + n), r, margins);
			r.top = pos;
			r.bottom = pos + n - 1;
			RectFill(r, cellattrs, VTCell::FILL_NORMAL);
			scrolled = n;
		}
		else {
			lines.InsertN(pos - 1, n);
			for(int i = pos - 1; i < pos - 1 + n; i++)
				lines[i].Adjust(size.cx, attrs);
			lines.Remove(margins.bottom, n);
			scrolled = n;
		}

		Invalidate(pos - 1, margins.bottom);
		ClearEol();
	}

	return scrolled;
}

int VTPage::LineRemove(int pos, int n, const VTCell& attrs)
{
	int scrolled = 0;

	n = clamp(n, 0, margins.bottom - pos + 1);

	LTIMING("VTPage::LineRemove");

	if(n > 0)
	{
		LLOG("LineRemove(" << pos << ") -> scrolling " << n << " lines down");

		if(HorzMarginsExist())
		{
			Rect r = margins;
			r.top  = pos + n;
			RectCopy(margins.TopLeft(), r, margins);
			r.top = margins.bottom - n + 1;
			RectFill(r, cellattrs, VTCell::FILL_NORMAL);
			scrolled = n;
		}
		else {
			// Batch insert lines at bottom once
			lines.InsertN(margins.bottom, n);
			for(int i = margins.bottom; i < margins.bottom + n; i++)
				lines[i].Adjust(size.cx, attrs);
			if(history && GetAbsRow(pos) == 1)
				SaveToHistory(pos, n);
			lines.Remove(pos - 1, n);
			scrolled = n;
		}

		Invalidate(pos - 1, margins.bottom);
		ClearEol();
	}

	return scrolled;
}

VTPage& VTPage::ScrollUp(int n)
{
	if(LineInsert(margins.top, n, cellattrs) > 0)
		WhenUpdate();
	return *this;
}

VTPage& VTPage::ScrollDown(int n)
{
	if(LineRemove(margins.top, n, cellattrs) > 0)
		WhenUpdate();
	return *this;
}

int VTPage::CellInsert(int pos, int n, const VTCell& attrs, bool pan)
{
	int shifted = 0;

	n = clamp(n, 0, margins.right - pos + 1);

	LTIMING("VTPage::CellInsert");

	if(n > 0)
	{
		if(pan)
		{
			for(int row = margins.top; row <= margins.bottom; row++)
			{
				lines[row - 1].ShiftRight(pos, margins.right, n, attrs);
				shifted++;
			}
		}
		else
		{
			int row = cursor.y - 1;
			lines[row].ShiftRight(pos, margins.right, n, attrs);
			shifted++;
		}

		ClearEol();
	}

	return shifted;
}

int VTPage::CellRemove(int pos, int n, const VTCell& attrs, bool pan)
{
	int shifted = 0;

	n = clamp(n, 0, margins.right - pos + 1);

	LTIMING("VTPage::CellRemove");

	if(n > 0)
	{
		if(pan)
		{
			for(int row = margins.top; row <= margins.bottom; row++)
			{
				lines[row - 1].ShiftLeft(pos, margins.right, n, attrs);
				shifted++;
			}
		}
		else
		{
			int row = cursor.y - 1;
			lines[row].ShiftLeft(pos, margins.right, n, attrs);
			shifted++;
		}

		ClearEol();
	}

	return shifted;
}

VTPage& VTPage::ScrollLeft(int n)
{
	if(MarginsContain(cursor))
		CellInsert(margins.left, n, cellattrs, true);
	return *this;
}

VTPage& VTPage::ScrollRight(int n)
{
	if(MarginsContain(cursor))
		CellRemove(margins.left, n, cellattrs, true);
	return *this;
}

VTPage& VTPage::PanLeft(int n)
{
	if(MarginsContain(cursor))
		CellRemove(cursor.x, n, cellattrs, true);
	return *this;
}

VTPage& VTPage::PanRight(int n)
{
	if(MarginsContain(cursor))
		CellInsert(cursor.x, n, cellattrs, true);
	return *this;
}

VTPage& VTPage::InsertLines(int pos, int n)
{
	LLOG("InsertLines(" << pos << ", " << n << ")");

	if(MarginsContain(cursor) && LineInsert(pos, n, cellattrs) > 0) {
		MoveHome();
		WhenUpdate();
	}
	return *this;
}

VTPage& VTPage::RemoveLines(int pos, int n)
{
	LLOG("RemoveLines(" << pos << ", " << n << ")");

	if(MarginsContain(cursor) && LineRemove(pos, n, cellattrs) > 0) {
		MoveHome();
		WhenUpdate();
	}
	return *this;
}

VTPage& VTPage::InsertCells(int pos, int n)
{
	LLOG("InsertCells(" << pos << ", " << n << ")");

	if(HorzMarginsContain(cursor.x))
		CellInsert(pos, n, cellattrs, false);
	return *this;
}

VTPage& VTPage::RemoveCells(int pos, int n)
{
	LLOG("RemoveCells(" << pos << ", " << n << ")");

	if(HorzMarginsContain(cursor.x))
		CellRemove(pos, n, cellattrs, false);
	return *this;
}

VTPage& VTPage::EraseCells(int n, dword flags)
{
	LLOG("EraseCells(" << n << ")");

	lines[cursor.y - 1].Fill(cursor.x, cursor.x + n - 1, cellattrs, flags);
	ClearEol();
	return *this;
}

VTPage& VTPage::EraseLine(dword flags)
{
	LLOG("EraseLine(" << flags << ")");
	
	VTLine& l = lines[cursor.y - 1];
	l.FillLine(cellattrs, flags);
	l.Unwrap();
	ClearEol();
	return *this;
}

VTPage& VTPage::EraseLeft(dword flags)
{
	LLOG("EraseLeft(" << flags << ")");
	
	VTLine& l =	lines[cursor.y - 1];
	l.FillLeft(cursor.x, cellattrs, flags);
	l.Unwrap();
	ClearEol();
	return *this;
}

VTPage& VTPage::EraseRight(dword flags)
{
	LLOG("EraseRight(" << flags << ")");
	
	VTLine& l =	lines[cursor.y - 1];
	l.FillRight(cursor.x, cellattrs, flags);
	l.Unwrap();
	ClearEol();
	return *this;
}

VTPage& VTPage::ErasePage(dword flags)
{
	LLOG("ErasePage(" << flags << ")");
	
	Rect r = GetView();
	for(int i = r.top; i <= r.bottom; i++) {
		VTLine& l = lines[i - 1];
		l.Shrink(size.cx);
		l.FillLine(cellattrs, flags);
		l.Unwrap();
	}
	ClearEol();
	return *this;
}

VTPage& VTPage::EraseBefore(dword flags)
{
	LLOG("EraseBefore(" << flags << ")");
	
	for(int i = 1; i < cursor.y; i++) {
		VTLine& l =	lines[i - 1];
		l.FillLine(cellattrs, flags);
		l.Unwrap();
	}
	return EraseLeft(flags);
}

VTPage& VTPage::EraseAfter(dword flags)
{
	LLOG("EraseAfter(" << flags << ")");
	
	for(int i = cursor.y + 1; i <= size.cy; i++) {
		VTLine& l =  lines[i - 1];
		l.FillLine(cellattrs, flags);
		l.Unwrap();
	}
	return EraseRight(flags);
}

int VTPage::SetTabStop(int col, bool b)
{
	LLOG("SetTabStop(" << col << ", "<< b << ")");

	tabs.Set(col, b);
	return ++col;
}

VTPage& VTPage::SetTabs(int tsz)
{
	LLOG("SetTabs(" << tsz << ")");

	tabsize = tsz;
	tabs.Clear();
	for(int i = 1; i <= size.cx; i += tabsize)
		tabs.Set(i, true);
	tabsync = true;
	return *this;
}

void VTPage::GetTabs(Vector<int>& tabstops)
{
	for(int i = 1; i <= size.cx; i++)
		if(IsTabStop(i))
			tabstops.Add(i);

	LLOG("GetTabs() -> " << tabstops);
}

VTPage& VTPage::NextTab(int n)
{
	for(int count = 0, i = cursor.x + 1; i <= margins.right && count < n; i++)
	{
		if(IsTabStop(i))
		{
			MoveToColumn(GetAbsCol(i));
			count++;
		}
		else
		if(i == margins.right)
		{
			MoveEnd();
		}
	}

	return *this;
}

VTPage& VTPage::PrevTab(int n)
{
	int left = cursor.displaced ? margins.left : GetView().left;

	for(int count = 0, i = cursor.x - 1; i >= left && count < n; i--)
	{
		if(IsTabStop(i))
		{
			MoveToColumn(GetAbsCol(i));
			count++;
		}
		else
		if(i == left)
		{
			MoveHome();
		}
	}

	return *this;
}

VTPage& VTPage::SetHorzMargins(int l, int r)
{
	Rect view = GetView();

	if(l < r
	&& l >= view.left
	&& l <= view.right
	&& r >= view.left
	&& r <= view.right
	)
	{
		margins.left  = l;
		margins.right = r;
		
		LLOG("Horizontal margins: " << Point(l, r));
	}
	else
	{
		margins.left = view.left;
		margins.right = view.right;
	}
	return MoveTo(1, 1);
}

VTPage& VTPage::SetVertMargins(int t, int b)
{
	Rect view = GetView();

	if(t < b
	&& t >= view.top
	&& t <= view.bottom
	&& b >= view.top
	&& b <= view.bottom
	)
	{
		margins.top    = t;
		margins.bottom = b;
		
		LLOG("Vertical margins: " << Point(t, b));
	}
	else
	{
		margins.top = view.top;
		margins.bottom = view.bottom;
	}

	return MoveTo(1, 1);
}

VTPage& VTPage::SetMargins(const Rect& r)
{
	if(IsNull(r))
		ResetMargins();
	else {
		SetHorzMargins(r.left, r.right);
		SetVertMargins(r.top, r.bottom);
	}
	return *this;
}

VTPage& VTPage::ResetMargins()
{
	margins = GetView();
	ClearEol();
	return *this;
}

const VTCell& VTPage::FetchCell(const Point& pt) const
{
	const VTLine& line = FetchLine(pt.y);
	return !line.IsVoid()
		&& pt.x >= 0
		&& pt.x < line.GetCount()
			? line[pt.x]
			: VTCell::Void();
}

void VTPage::FetchCellsMutable(Point pl, Point ph, Event<VTCell&> consumer)
{
	Point ptl = min(pl, ph);
	Point pth = max(pl, ph);
	
	if(ptl.y == pth.y) {
		const VTLine& line = FetchLine(ptl.y);
		if(!line.IsVoid())
			for(int i = ptl.x; i < min(pth.x, line.GetCount() - 1); i++) {
				consumer(const_cast<VTCell&>(line[i]));
			}
	}
	else {
		for(int i = ptl.y; i <= pth.y; i++) {
			const VTLine& line = FetchLine(i);
			if(line.IsVoid())
				continue;
			if(i == ptl.y) {
				for(int j = ptl.x; j < line.GetCount(); j++)
					consumer(const_cast<VTCell&>(line[j]));
			}
			else
			if(i == pth.y) {
				for(int j = 0; j < min(pth.x, line.GetCount() - 1); j++)
					consumer(const_cast<VTCell&>(line[j]));
			}
			else {
				for(int j = 0; j < line.GetCount(); j++)
					consumer(const_cast<VTCell&>(line[j]));
			}
		}
	}
}

const VTLine& VTPage::FetchLine(int i) const
{
	const int slen = saved.GetCount();
	const int llen = lines.GetCount();
	
	if(i >= 0 && i < slen)
		return saved[i];
	if(i >= slen && i < slen + llen)
		return lines[i - slen];
	
	return VTLine::Void();
}

int VTPage::FetchLine(int i, Gate<int, const VTLine&> consumer, int spanlimit) const
{
	LLOG("FetchLine(" << i <<")");
	
	Tuple<int, int> span = GetLineSpan(i, spanlimit);
	for(int n = span.a; n <= span.b; n++) {
		const VTLine& l = FetchLine(n);
		if(!l.IsVoid())
			consumer(n, l);
	}

	return span.b;
}

int VTPage::FetchLine(int i, VectorMap<int, VTLine>& line) const
{
	LLOG("FetchLine(" << i << ", " << &line << ") [fecthes as a line vector]");

	return FetchLine(i, [&](int ii, const VTLine& l) { line.Add(ii, clone(l)); return false; });
}

int VTPage::FetchLine(int i, VectorMap<int, WString>& line) const
{
	LLOG("FetchLine(" << i << ", " << &line << ") [fetches as a text]");
	
	return FetchLine(i, [&](int ii, const VTLine& l) { line.Add(ii, l.ToWString()); return false; });
}

bool VTPage::FetchRange(const Rect& r, Gate<int, const VTLine&, VTLine::ConstRange&> consumer, bool rect) const
{
	if(IsNull(r) || !consumer)
		return false;

	for(int i = max(0, r.top); i <= min(r.bottom, GetLineCount()); i++) {
		const VTLine& line = FetchLine(i);
		if(!line.IsVoid()) {
			int length = line.GetCount();
			int b = 0, e = length;
			if(r.top == r.bottom || rect) {
				b = max(0, r.left);
				e = clamp(r.right, b, length);
			}
			else
			if(r.top == i) {
				b = clamp(r.left, 0, length);
				e = length;
			}
			else
			if(r.bottom == i) {
				b = 0;
				e = clamp(r.right, 0, length);
			}
			auto range  = SubRange(line, b, e - b);
			if(consumer(i, line, range))
				return false;
		}
	}

	return true;
}

bool VTPage::FetchRange(int b, int e, Gate<VectorMap<int, VTLine>&> consumer) const
{
	VectorMap<int, VTLine> ln;
	for(int i = b; i < e; i++) {
		const VTLine& l = FetchLine(i);
		if(!l.IsVoid()) {
			ln.Add(i, clone(l));
			if(!l.IsWrapped() || i == e - 1) {
				if(consumer(ln))
					return true;
				ln.Clear();
			}
		}
	}
	return false;
}

bool VTPage::FetchRange(Tuple<int, int> range, Gate<VectorMap<int, VTLine>&> consumer) const
{
	return FetchRange(range.a, range.b, consumer);
}

void VTPage::LineFill(int pos, int begin, int end, const VTCell& filler, dword flags)
{
	if(lines[pos - 1].Fill(begin, end, filler, flags))
		ClearEol();
}


void VTPage::RectCopy(const Point &p, const Rect& r, const Rect& rr, dword flags)
{
	LTIMING("VTPage::RectCopy");

	Rect src(Bind(rr, r.TopLeft()), Bind(rr, r.BottomRight()));
	Rect dest(p, src.GetSize());
	dest.Set(Bind(rr, dest.TopLeft()), Bind(rr, dest.BottomRight()));

	Buffer<VTCell> temp((src.Height() + 1) * (src.Width() + 1));

	for(int pass = 0; pass < 2; pass++)
	{
		const Rect& rx = pass == 0 ? src : dest;
		for(int i = rx.top, pos = 0; i <= rx.bottom; i++)
		{
			VTLine& line = lines[i - 1];
			for(int j = rx.left; j <= rx.right; j++, pos++)
			{
				VTCell& a = line[j - 1];
				VTCell& b = *(temp + pos);
				if(pass == 0)
					b.Fill(a, flags);
				else
					a.Fill(b, flags);
			}
			if(pass == 1)
				line.Invalidate();
		}
	}
}

void VTPage::RectFill(const Rect& r, const VTCell& filler, dword flags)
{
	for(int i = r.top; i <= r.bottom; i++)
		if(lines[i - 1].Fill(r.left, r.right, filler, flags))
			ClearEol();
}

Rect VTPage::AdjustRect(const Rect& r, bool displaced)
{
	if(r.top > r.bottom || r.left > r.right)
		return Null;

	if(displaced && cursor.displaced) {
		Rect rr = r.Offseted(margins.TopLeft() - 1);
		return Rect(Bind(margins, rr.TopLeft()),
                    Bind(margins, rr.BottomRight()));
	}

	Rect view = GetView();
	return Rect(Bind(view, r.TopLeft()),
                Bind(view, r.BottomRight()));
}

VTPage& VTPage::FillRect(const Rect& r, const VTCell& filler, dword flags)
{
	Rect rr = AdjustRect(r);
	if(!IsNull(rr))
		RectFill(rr, filler, flags);
	return *this;
}

VTPage& VTPage::FillRect(const Rect& r, dword chr)
{
	VTCell filler = cellattrs;
	filler.chr = chr;
	return FillRect(r, filler, VTCell::FILL_NORMAL);
}

VTPage& VTPage::EraseRect(const Rect& r, dword flags)
{
	Rect rr = AdjustRect(r);
	if(!IsNull(rr))
		RectFill(rr, cellattrs, flags);
	return *this;
}

VTPage& VTPage::CopyRect(const Point& pt, const Rect& r, dword flags)
{
	Rect rr = AdjustRect(r);
	if(!IsNull(rr)) {
		Point pp = cursor.displaced ? margins.TopLeft() + pt - 1 : pt;
		RectCopy(pp, rr, cursor.displaced ? GetMargins() : GetView());
	}
	return *this;
}

void VTPage::GetRectArea(const Rect& r, Event<Point> consumer, bool displaced)
{
	if(r.top <= r.bottom && r.left <= r.right) {
		Rect rr = AdjustRect(r, displaced);
		for(int y = rr.top; y <= rr.bottom; y++)
		for(int x = rr.left; x <= rr.right; x++)
			consumer(Point(x, y));
	}
}

VTPage& VTPage::FillStream(const Rect& r, const VTCell& filler, dword flags)
{
	LTIMING("VTPage::FillStream");

	Rect rr = AdjustRect(r);
	if(!IsNull(rr)) {
		Rect pr = cursor.displaced ? GetMargins() : GetView();
		for(int i = rr.top; i <= rr.bottom; i++) {
			if(rr.top == rr.bottom)
				LineFill(i, rr.left, rr.right, filler, flags);
			else {
				if(i == rr.top)
					LineFill(i, rr.left, pr.right, filler, flags);
				else
				if(i == rr.bottom)
					LineFill(i, pr.left, rr.right, filler, flags);
				else
					LineFill(i, pr.left, pr.right, filler, flags);
			}
		}
	}
	return *this;
}

VTPage& VTPage::FillStream(const Rect& r, dword chr)
{
	VTCell filler = cellattrs;
	filler.chr = chr;
	return FillStream(r, filler, VTCell::FILL_NORMAL);
}

VTPage& VTPage::AddImage(Size sz, dword imageid, bool scroll, bool relpos)
{
	LTIMING("VTPage::AddImage");

	if(!scroll) sz = min(sz, size);
	for(int i = 0; i < sz.cy; i++) {
		Point pt = scroll || relpos ? cursor - 1 : Point(0, i);
		VTLine& line = lines[pt.y];
		for(int j = 0; j < sz.cx; j++) {
			VTCell& cell = line[min(pt.x + j, size.cx - 1)];
			cell.Image();
			cell.chr = imageid;
			cell.object.col = j;
			cell.object.row = i;
		}
		line.Invalidate();
		if(scroll)
			NextLine();
		else
		if(relpos)
			MoveDown();
	}

	return *this;
}

void VTPage::Serialize(Stream& s)
{
	int version = 1;
	s / version;
	if(version >= 1) {
		s % tabsize;
		s % history;
		s % historysize;
	}
	
	if(s.IsLoading()) {
		historysize = max(1, historysize);
		SetTabs(tabsize);
	}
}

void VTPage::Jsonize(JsonIO& jio)
{
	jio ("TabSize", tabsize)
		("HistoryBuffer", history)
		("HistoryBufferMaxSize", historysize);

	if(jio.IsLoading()) {
		historysize = max(1, historysize);
		SetTabs(tabsize);
	}
}

void VTPage::Xmlize(XmlIO& xio)
{
	XmlizeByJsonize(xio, *this);
}

String VTPage::Cursor::ToString() const
{
	return Format(
		"[%d:%d] - Flags: displaced: %`, EOL: %",
			x, y, displaced, eol);
}

Tuple<int, int> VTPage::GetLineSpan(int i, int limit) const
{
	LLOG("GetLineSpan(" << i << ")");

	int lo = i, hi = i, minlo = 0, maxhi = GetLineCount();

	if(limit > 0) {
		minlo =  clamp(lo - limit, 0, maxhi);
		maxhi =  clamp(hi + limit, 0, maxhi);
	}

	while(lo > minlo && FetchLine(lo - 1).IsWrapped())
		lo--;
	while(hi < maxhi && FetchLine(hi).IsWrapped())
		hi++;

	return MakeTuple(lo, hi);

}

WString AsWString(const VTPage& page, const Rect& r, bool rectsel, bool tspaces)
{
	Vector<WString> v;
	bool wrapped = false;

	auto RangeToWString = [&](int i, const VTLine& line, VTLine::ConstRange& range) -> bool
	{
		WString s = AsWString(range, tspaces);
		if(!rectsel && (v.GetCount() && wrapped))
			v.Top() << s;
		else
			v.Add() << s;
		wrapped = line.IsWrapped();
		return false;
	};

	#ifdef PLATFORM_WIN32
	constexpr const char *VT_EOL = "\r\n";
	#else
	constexpr const char *VT_EOL = "\n";
	#endif

	page.FetchRange(r, RangeToWString, rectsel);
	return pick(Join(v, VT_EOL));
}

int GetLength(const VTPage& page, int begin, int end)
{
	int length = 0;
	for(int i = max(0, begin); i < min(end, page.GetLineCount()); i++)
		length += page.FetchLine(i).GetLength();
	return length;
}

int GetOffset(const VTPage& page, int begin, int end)
{
	int offset = 0;
	for(int i = max(0, begin); i < min(end, page.GetLineCount()); i++)
		offset += page.FetchLine(i).GetOffset();
	return offset;
}

}