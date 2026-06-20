#include "Terminal.h"

#define LLOG(x)     // RLOG("TerminalCtrl (#" << this << "]: " << x)
#define LTIMING(x)	// RTIMING(x)

namespace Upp {

struct CellPaintData : Moveable<CellPaintData> {
	Point pos   = {0, 0};
	Size  size  = {0, 0};
	Color ink   = Null;
	Color paper = Null;
	bool show:1;
	bool highlight:1;
};

class sRectRenderer {
	Draw& w;
	Rect  cr;
	Color color;
	Color background;
	bool  transparent = false;

public:
	void DrawRect(const VTCell& cell, const CellPaintData& data);
	void Flush();

	sRectRenderer(Draw& w, Color bkg, bool tr)
		: w(w), cr(Null), color(Null), background(bkg), transparent(tr) {}
	~sRectRenderer()              { Flush(); }
};

void sRectRenderer::Flush()
{
	if(IsNull(cr) || (!transparent && color == background))
		return;
	LTIMING("sRectRenderer::Flush");
	w.DrawRect(cr, color);
	cr = Null;
}

void sRectRenderer::DrawRect(const VTCell& cell, const CellPaintData& data)
{
	if(!data.show)
		return;

	Rect r(data.pos, data.size);
	if(cr.top == r.top && cr.bottom == r.bottom && cr.right == r.left && data.paper == color) {
		cr.right = r.right;
		return;
	}
	Flush();
	cr = r;
	color = data.paper;
}

class sTextRenderer {
	Draw&       w;
	Font        font;
	int         y, icount = 0, lcount = 0;

	struct Chrs : Moveable<Chrs> {
		Vector<int> x;
		Vector<int> width;
		WString     text;
	};

	VectorMap< Tuple<dword, Color>, Chrs > cache;

	// Fast-path cache for last style run
	Tuple<dword, Color> lastkey;
	Chrs* last = nullptr;

public:
	int  GetImageCount() const                     { return icount; }
	int  GetLinkCount()  const                     { return lcount; }
	void DrawChar(const VTCell& cell, const CellPaintData& data);
	void Flush();

	Color annotationcolor;
	bool canhyperlink:1;
	bool canannotate:1;

	sTextRenderer(Draw& w, Font f) : w(w), font(f) { y = Null; }
	~sTextRenderer()                               { Flush();  }
};

void sTextRenderer::Flush()
{
	if(cache.GetCount() == 0)
		return;

	LTIMING("sTextRenderer::Flush");

	const int fcx = font.GetMonoWidth();

	for(int i = 0; i < cache.GetCount(); i++) {
		Chrs& c = cache[i];
		if(c.x.GetCount()) {
			const Tuple<dword, Color>& fc = cache.GetKey(i);
			const int x = c.x[0], cx = c.x.Top() + fcx;
			font.Bold(fc.a & VTCell::SGR_BOLD)
				.Italic(fc.a & VTCell::SGR_ITALIC)
				.Strikeout(fc.a & VTCell::SGR_STRIKEOUT)
				.Underline(fc.a & VTCell::SGR_UNDERLINE);
			for(int i = 0; i < c.x.GetCount() - 1; i++)
				c.x[i] = c.x[i + 1] - c.x[i];
			c.x.Top() = c.width.Top();
			if(fc.a & VTCell::SGR_OVERLINE) {
				int h = font.GetDescent() - 2;
				w.DrawLine(x, y + h, cx, y + h, PEN_SOLID, fc.b);
			}
			if(canhyperlink && fc.a & VTCell::SGR_HYPERLINK) {
				int h = font.GetAscent() + 2;
				w.DrawLine(x, y + h, cx, y + h, PEN_DOT, fc.b);
				font.NoUnderline();
			}
			if(canannotate && fc.a & VTCell::SGR_ANNOTATION) {
				int h = font.GetAscent() + 2;
				w.DrawLine(x, y + h, cx, y + h, PEN_SOLID, annotationcolor);
				font.NoUnderline();
			}
			w.DrawText(x, y, c.text, font, fc.b, c.x);
		}
	}
	cache.Clear();

	// reset fast-path cache
	last = nullptr;
}

void sTextRenderer::DrawChar(const VTCell& cell, const CellPaintData& data)
{
	Point p = data.pos;

	if(y != p.y) {
		Flush();
		y = p.y;
	}

	Chrs *c;
	// Optimization: Bypassed standard Tuple instantiation on hot path by checking raw components directly.
	if(last && cell.sgr == lastkey.a && data.ink == lastkey.b) {
		c = last;
	}
	else {
		lastkey.a = cell.sgr;
		lastkey.b = data.ink;
		c = &cache.GetAdd(lastkey);
		last = c;
	}

	// Order is important for optimization
	if((cell.IsUnderlined() || cell.IsHypertext()) && cache.GetCount() > 1) {
		Flush();
		lastkey.a = cell.sgr;
		lastkey.b = data.ink;
		c = &cache.GetAdd(lastkey);
		last = c;
	}

	icount += (int) cell.sgr & VTCell::SGR_IMAGE;
	lcount += (int) cell.sgr & VTCell::SGR_HYPERLINK;

	bool hide = false;
	hide |= cell.chr < 0x20;
	hide |= cell.IsImage();
	hide |= cell.IsConcealed();
	hide |= (!data.show && !data.highlight && cell.IsBlinking());

	if(hide) {
		if(c->width.GetCount())
			c->width.Top() += data.size.cx;
	}
	else {
		c->text.Cat((int) cell.chr);
		c->x.Add(p.x);
		c->width.Add(data.size.cx);
	}
}

void TerminalCtrl::Paint0(Draw& w, bool print)
{
	GuiLock __;

	int  pos = GetSbPos();
	Size wsz = GetSize();
	Size psz = GetPageSize();
	Size csz = GetCellSize();

	w.Clip(wsz);

	LTIMING("TerminalCtrl::Paint");

	bool dim = (brightness < 100) && !print;
	int dimalpha = ((100 - brightness) * 255) / 100;

	Color bkg = colortable[COLOR_PAPER];

	CellPaintData cpd;
	cpd.size = csz;
	Vector<CellPaintData> linepaintdata(psz.cx, cpd);

	
	auto PaintLine = [&](const VTLine& line, int i) {
		LTIMING("TerminalCtrl::PaintLine");
		int y = i * csz.cy - (csz.cy * pos);
		{
			// Render the background rectangles.
			sRectRenderer rr(w, dim ? Blend(bkg, Black(), dimalpha) : bkg, nobackground);
			for(int j = 0, x = 0; j < psz.cx; j++, x += csz.cx) {
				const VTCell& cell = line.Get(j, GetAttrs());
				CellPaintData& data = linepaintdata[j];
				data.highlight = IsSelected(Point(j,i));
				data.show = !nobackground;
				data.show |= !IsNull(cell.paper);
				data.show |= (cell.IsHypertext() && cell.data == activehtext);
				data.show |= cell.IsInverted();
				data.show |= print;
				if(data.highlight) {
					data.ink = colortable[COLOR_INK_SELECTED];
					data.paper = colortable[COLOR_PAPER_SELECTED];
				}
				else {
					SetInkAndPaperColor(cell, data.ink, data.paper);
					if(dim) {
						data.ink = Blend(data.ink, Black(), dimalpha);
						data.paper = Blend(data.paper, Black(), dimalpha);
					}
				}
				data.pos = {x, y};
				if(j == psz.cx - 1)
					data.size.cx = wsz.cx - x;

				data.show |= data.highlight;
				rr.DrawRect(cell, data);
			}
		}
		int icount = 0;
		{
			// Render the text by combining non-contiguous chunks of chars.
			sTextRenderer tr(w, GetFont());
			tr.canhyperlink = hyperlinks;
			tr.canannotate  = annotations;
			tr.annotationcolor = colortable[COLOR_ANNOTATION_UNDERLINE];
			for(int j = 0, x = 0; j < psz.cx; j++, x += csz.cx) {
				CellPaintData& data = linepaintdata[j];
				data.pos = { x + padding.cx, y + padding.cy };
				data.show = !blinking;
				tr.DrawChar(line.Get(j, GetAttrs()), data);
			}
			icount = tr.GetImageCount();
			// lcount = tr.GetLinkCount();
		}
		{
			if(icount) {
				// Render inline images, if any.
				ImageParts ip;
				for(int j = 0, x = 0; j < psz.cx; j++, x += csz.cx) {
					if(line[j].IsImage())
						CollectImage(ip, x, y, line.Get(j, GetAttrs()), csz);
				}
				PaintImages(w, ip, csz);
			}
		}
	};

	if(!nobackground)
		w.DrawRect(wsz, bkg);

	auto range = GetPageRange();

	if(highlight) {
		LTIMING("TerminalCtrl::WhenHighlight");
		page->FetchRange(range.a, range.b, [&](VectorMap<int, VTLine>& hl) {
			WhenHighlight(hl);
			for(const auto& h : ~hl)
				PaintLine(h.value, h.key);
			return false;
		});
	}
	else {
		for(int i = range.a; i < range.b; i++) {
			int y = i * csz.cy - (csz.cy * pos);
			if(!w.IsPainting(0, y, wsz.cx, csz.cy))
				continue;
			if(const VTLine& line = page->FetchLine(i); !line.IsVoid())
				PaintLine(line, i);
		}

	}

	// Paint a steady (non-blinking) caret, if enabled.
	if(IsSelectorMode() || (modes[DECTCEM] && HasFocus() && (print || !caret.IsBlinking())))
		w.DrawRect(caretrect, InvertColor);

	// Flash the screen.
	if(flashing && IsVisible())
		w.DrawRect(wsz, InvertColor());

	// Hint new size.
	if(sizehint && hinting && IsVisible())
		PaintSizeHint(w);

	w.End();
}

void TerminalCtrl::PaintSizeHint(Draw& w)
{
	Tuple<String, Size> hint = GetSizeHint();
	Rect rr = GetViewRect().CenterRect(hint.b).Inflated(8);
	Rect rx = Rect(rr.GetSize()).CenterRect(hint.b);
	ImagePainter ip(rr.GetSize());
	ip.Begin();
	ip.Clear(RGBAZero());
	ip.RoundedRectangle(0, 0, rr.Width(), rr.Height(), 10.0)
	.Stroke(1, LtGray())
	.Fill(SColorText());
	ip.DrawText(rx.left, rx.top, hint.a, StdFont(), SColorPaper);
	ip.End();
	w.DrawImage(rr.left, rr.top, ip.GetResult());
}

void TerminalCtrl::PaintImages(Draw& w, ImageParts& parts, const Size& csz)
{
	LTIMING("TerminalCtrl::PaintImages");

	InlineImage im;
	dword lastid = -1;

	for(const ImagePart& part : parts) {
		const dword& id = part.a;
		const Point& pt = part.b;
		const Rect&  rr = part.c;
		Rect r(pt, rr.GetSize());
		if(id != lastid) { // Optimization: cached inline-image to reduce mutex locking.
			im = GetCachedImageData(id, Null, csz);
			lastid = id;
		}
		if(!IsNull(im.image)) {
			im.paintrect = rr;    // Keep it updated.
			im.fontsize  = csz;    // Keep it updated.
			imgdisplay->Paint(w, r, im, colortable[COLOR_INK], colortable[COLOR_PAPER], 0);
		}
	}
}

void TerminalCtrl::CollectImage(ImageParts& ip, int x, int y, const VTCell& cell, const Size& sz)
{
	LTIMING("TerminalCtrl::CollectImage");

	dword id = cell.chr;
	Point coords = Point(x, y);
	Rect  ir = RectC(cell.object.col * sz.cx, cell.object.row * sz.cy, sz.cx, sz.cy);
	if(!ip.IsEmpty()) {
		ImagePart& part = ip.Top();
		if(id == part.a && part.b.y == coords.y && part.c.right == ir.left) {
			part.c.right = ir.right;
			return;
		}
	}
	ip.Add(MakeTuple(id, coords, ir));
}

void TerminalCtrl::RenderImage(const ImageString& imgs, bool scroll)
{
	bool encoded = !imgs.IsSixel(); // Sixel images are not base64 encoded.

	if(WhenImage) {
		WhenImage(encoded ? Base64Decode(imgs.data) : imgs.data);
		return;
	}

	LTIMING("TerminalCtrl::RenderImage");

	Size fsz = GetCellSize();
	dword id = FoldHash(CombineHash(imgs, fsz));
	const InlineImage& imd = GetCachedImageData(id, imgs, fsz);
	if(!IsNull(imd.image)) {
		page->AddImage(imd.cellsize, id, scroll, encoded);
		RefreshDisplay();
	}
}

// Shared image data cache support.

static StaticMutex sImageCacheLock;
static LRUCache<TerminalCtrl::InlineImage> sInlineImagesCache;
static int sCachedImageMaxSize =  1024 * 1024 * 4 * 128;
static int sCachedImageMaxCount =  256000;

String TerminalCtrl::InlineImageMaker::Key() const
{
	StringBuffer h;
	RawCat(h, id);
	return String(h); // Make MSVC happy...
}

int TerminalCtrl::InlineImageMaker::Make(InlineImage& imagedata) const
{
	LTIMING("TerminalCtrl::ImageDataMaker::Make");

	auto ToCellSize = [this](Size sz) -> Size
	{
		Size fs(fontsize);

		sz.cx = (sz.cx + fs.cx - 1) / fs.cx;
		sz.cy = (sz.cy + fs.cy - 1) / fs.cy;

		return Size(max(1, sz.cx), max(1, sz.cy));
	};

	auto AdjustSize = [this](Size sr, Size sz) -> Size
	{
		if(imgs.IsKeepRatio()) {
			if(sr.cx == 0 && sr.cy > 0)
				sr.cx = max(1, sr.cy * sz.cx / sz.cy);
			else
			if(sr.cy == 0 && sr.cx > 0)
				sr.cy = max(1, sr.cx * sz.cy / sz.cx);
		}
		else {
			if(sr.cx <= 0)
				sr.cx = sz.cx;
			if(sr.cy <= 0)
				sr.cy = sz.cy;
		}

		return sr != sz ? sr : Null;
	};

	auto RawToImage = [](const String& raw, Size sz, bool rgba) -> Image
	{
		int stride = sz.cx * (rgba ? 4 : 3);
		if(raw.GetLength() < stride * sz.cy)
			return Null;

		ImageBuffer ib(sz);
		const byte *src = (const byte*) raw.Begin();
		RGBA *dst = ~ib;
		int n   = sz.cx * sz.cy;

		if(rgba) {
			for(int i = 0; i < n; i++, dst++) {
				dst->r = *src++; dst->g = *src++;
				dst->b = *src++; dst->a = *src++;
			}
		}
		else {
			for(int i = 0; i < n; i++, dst++) {
				dst->r = *src++; dst->g = *src++;
				dst->b = *src++; dst->a = 255;
			}
		}

		ib.SetHotSpot(Null);
		return ib;
	};

	Image img;

	if(imgs.IsSixel()) { // Never base64 encoded
		img = (Image) SixelStream(imgs.data, imgs.palette).Background(!imgs.IsTransparent());
	}
	else
	if(imgs.IsRaster()) { // Always base64 encoded (PNG, JPG, TIFF, etc.)
		img = StreamRaster::LoadStringAny(imgs.IsCompressed() ? ZDecompress(Base64Decode(imgs.data)) : Base64Decode(imgs.data));
	}
	else
	if(imgs.IsRaw()) { // Always base64 encoded (RGB or RGBA raw data)
		img = RawToImage(imgs.IsCompressed() ? ZDecompress(Base64Decode(imgs.data)) : Base64Decode(imgs.data), imgs.size, imgs.IsRGBA());
	}

	if(IsNull(img))
		return 0;

	if(IsNull(imgs.size)) {
		imagedata.image = img;
	}
	else {
		Size sz = AdjustSize(imgs.size, img.GetSize());
		imagedata.image = IsNull(sz) ? img : Rescale(img, sz);
	}

	imagedata.fontsize = fontsize;
	imagedata.cellsize = ToCellSize(imagedata.image.GetSize());
	return imagedata.image.GetLength() * 4;
}

const TerminalCtrl::InlineImage& TerminalCtrl::GetCachedImageData(dword id, const ImageString& imgs, const Size& csz)
{
	Mutex::Lock __(sImageCacheLock);

	LTIMING("TerminalCtrl::GetCachedImageData");

	InlineImageMaker im(id, imgs, csz);
	sInlineImagesCache.Shrink(sCachedImageMaxSize, sCachedImageMaxCount);
	return sInlineImagesCache.Get(im);
}

void TerminalCtrl::ClearImageCache()
{
	Mutex::Lock __(sImageCacheLock);
	sInlineImagesCache.Clear();
}

void TerminalCtrl::SetImageCacheMaxSize(int maxsize, int maxcount)
{
	Mutex::Lock __(sImageCacheLock);
	sCachedImageMaxSize  = max(1, maxsize);
	sCachedImageMaxCount = max(1, maxcount);
}

dword TerminalCtrl::RenderHypertext(const String& uri)
{
	dword h = FoldHash(GetHashValue(uri));
	GetCachedHypertext(h, uri);
	return h;
}

// Shared hypertext cache support.

String TerminalCtrl::HypertextMaker::Key() const
{
	StringBuffer h;
	RawCat(h, id);
	return String(h); // Make MSVC happy...
}

int TerminalCtrl::HypertextMaker::Make(Value& obj) const
{
	LTIMING("TerminalCtrl::HypertextMaker::Make");

	obj = txt;
	return txt.GetLength();
}

String TerminalCtrl::GetCachedHypertext(dword id, const String& data)
{
	LTIMING("TerminalCtrl::GetCachedHypertext");

	HypertextMaker m(id, data);
	return MakeValue(m);
}

void TerminalCtrl::ClearHyperlinkCache()
{
	// TODO: Obsolete. Remove
}

void TerminalCtrl::SetHyperlinkCacheMaxSize(int maxcount)
{
	// TODO: Obsolete. Remove
}

// Image display support.

class NormalImageCellDisplayCls : public Display {
public:
	virtual void Paint(Draw& w, const Rect& r, const Value& q, Color ink, Color paper, dword style) const
	{
		const auto& im = q.To<TerminalCtrl::InlineImage>();
		if(!IsNull(im.image))
			w.DrawImage(r.left, r.top, im.image, im.paintrect);
	}
	virtual Size GetStdSize(const Value& q) const
	{
		const auto& im = q.To<TerminalCtrl::InlineImage>();
		return im.image.GetSize();
	}
};

const Display& NormalImageCellDisplay() { return Single<NormalImageCellDisplayCls>(); }

class ScaledImageCellDisplayCls : public Display {
public:
	virtual void Paint(Draw& w, const Rect& r, const Value& q, Color ink, Color paper, dword style) const
	{
		const auto& im = q.To<TerminalCtrl::InlineImage>();
		if(!IsNull(im.image)) {
			Size csz = im.cellsize;
			Size fsz = im.fontsize;
			w.DrawImage(r, CachedRescale(im.image, csz * fsz), im.paintrect);
		}
	}
	virtual Size GetStdSize(const Value& q) const
	{
		const auto& im = q.To<TerminalCtrl::InlineImage>();
		return im.image.GetSize();
	}
};

const Display& ScaledImageCellDisplay() { return Single<ScaledImageCellDisplayCls>(); }
}
