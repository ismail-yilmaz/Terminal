#include "Sixel.h"

#define LLOG(x)		 // RLOG("SixelStream: " << x)
#define LTIMING(x)	 // RTIMING(x)

namespace Upp {

void SixelStream::Palette::Init()
{
	SetCount(256, RGBAZero());

	// Initialize first 16 colors
	(*this)[0]  = (RGBA) Black();
	(*this)[1]  = (RGBA) Blue();
	(*this)[2]  = (RGBA) Red();
	(*this)[3]  = (RGBA) Green();
	(*this)[4]  = (RGBA) Magenta();
	(*this)[5]  = (RGBA) Cyan();
	(*this)[6]  = (RGBA) Yellow();
	(*this)[7]  = (RGBA) White();
	(*this)[8]  = (RGBA) Gray();
	(*this)[9]  = (RGBA) LtBlue();
	(*this)[10] = (RGBA) LtRed();
	(*this)[11] = (RGBA) LtGreen();
	(*this)[12] = (RGBA) LtMagenta();
	(*this)[13] = (RGBA) LtCyan();
	(*this)[14] = (RGBA) LtYellow();
	(*this)[15] = (RGBA) White();
}

SixelStream::SixelStream(const void *data, int64 size, Palette *shared_palette)
: MemReadStream(data, size)
, background(true)
, paletteptr(shared_palette)
{
}

SixelStream::SixelStream(const String& data, Palette *shared_palette)
: MemReadStream(~data, data.GetLength())
, background(true)
, paletteptr(shared_palette)
{
}

void SixelStream::Clear()
{
	if(!paletteptr)
		paletteptr = &private_palette;

	if(paletteptr == &private_palette)
		paletteptr->Init();

	size    = Size(0, 0);
	cursor  = Point(0, 1);
	repeat  = 0;
	ink     = (*paletteptr)[0];
	ink.a   = 0xFF;
	paper   = ink;
	if(!background)
		paper.a = 0;

	Zero(params);

	buffer.Create(1024, 1024);
	Fill(buffer, buffer.GetSize(), paper);

	CalcYOffests();
}

force_inline
int SixelStream::ReadParams()
{
	LTIMING("SixelStream::ReadParams");

	Zero(params);
	int c = 0, n = 0, i = 0;
	for(;;) {
		while((c = Peek()) > 0x2F && c < 0x3A)
			n = n * 10 + (Get() - 0x30);
		params[i++ & 7] = n;
		if(c != ';')
			break;
		n = 0;
		Get(); // faster than Stream::Skip(1)
	}
	return i;
}

force_inline
void SixelStream::CalcYOffests()
{
	int w = buffer.GetWidth();
	for(int i = 0, y = cursor.y * w; i < 6; i++, y += w)
		coords[i] = y;
}

force_inline
void SixelStream::Return()
{
	cursor.x = 0;
}

force_inline
void SixelStream::LineFeed()
{
	size.cx = max(size.cx, cursor.x);
	cursor.x =  0;
	cursor.y += 6;
	size.cy = max(size.cy, cursor.y);
	CalcYOffests();
}

static Color sHSLColor(int h, int s, int l)
{
	if(s == 0)
		return Color(l, l, l);

	double h1 = fmod(h / 60.0, 6.0);
	double l1 = l * 0.01;
	double s1 = s * 0.01;
	double c = (1.0 - abs(2.0 * l1 - 1.0)) * s1;
	double x = c * (1.0 - abs(fmod(h1, 2.0) - 1.0));

	double r, g, b;
	int sector = (int) ffloor(h1);
	switch(sector) {
	case 0: r = c; g = x; b = 0; break;
	case 1: r = x; g = c; b = 0; break;
	case 2: r = 0; g = c; b = x; break;
	case 3: r = 0; g = x; b = c; break;
	case 4: r = x; g = 0; b = c; break;
	case 5: r = c; g = 0; b = x; break;
	default: r = 0; g = 0; b = 0; break;
	}

	double m = l1 - (c * 0.5);
	return Color(
		(int) clamp((r + m) * 100.0 + 0.5, 0.0, 100.0),
		(int) clamp((g + m) * 100.0 + 0.5, 0.0, 100.0),
		(int) clamp((b + m) * 100.0 + 0.5, 0.0, 100.0));
}

force_inline
void SixelStream::SetPalette()
{
	LTIMING("SixelStream::SetPalette");

	int n = ReadParams();
	if(n == 5) {
		switch(params[1]) {
		case 2: { // RGB
			RGBA& rgba = paletteptr->At(params[0]);
			rgba.r = (params[2] * 255 + 99) / 100;
			rgba.g = (params[3] * 255 + 99) / 100;
			rgba.b = (params[4] * 255 + 99) / 100;
			rgba.a = 0xFF;
			break;
		}
		case 1: { // HLS
			int h = params[2];
			int l = params[3];
			int s = params[4];
			paletteptr->At(params[0]) = sHSLColor(h, s, l);
			break;
		}
		default:
			break;
		}
	}
	else
	if(n == 1) {
		ink = paletteptr->At(params[0]);
		ink.a = 0xFF;
	}
}

force_inline
void SixelStream::GetRasterInfo()
{
	LTIMING("SixelStream::GetRasterInfo");
	ReadParams(); // We don't use the raster info.
}

force_inline
void SixelStream::GetRepeatCount()
{
	LTIMING("SixelStream::GetRepeatCount");

	ReadParams();
	repeat += max(1, params[0]); // Repeat compression.
}

void SixelStream::AdjustBufferSize()
{
	LTIMING("AdjustBufferSize");
	if((cursor.x + repeat >= 4096) || (cursor.y + 6 >= 4096))
		throw Exc("Sixel canvas size is too big > (4096 x 4096)");
	ImageBuffer ibb(buffer.GetSize() += 512);
	Copy(ibb, Point(0, 0), buffer, buffer.GetSize());
	buffer = ibb;
	CalcYOffests();
}

force_inline
void SixelStream::PaintSixel(int c)
{
	LTIMING("SixelStream::PaintSixel");

	Size sz = buffer.GetSize();
	if((sz.cx < cursor.x + repeat) || (sz.cy < cursor.y))
		AdjustBufferSize();

	if(!repeat) {
		for(int n = 0; n < 6; ++n) {
			*(buffer + ((c >> n) & 1) * (coords[n] + cursor.x)) = ink;
		}
		size.cx = max(size.cx, ++cursor.x);
	}
	else {
		for(int n = 0; n < 6; ++n) {
			if(c & (1 << n)) {
				Fill(buffer + coords[n] + cursor.x, ink, repeat); // Takes advantage of CPU-intrinsics.
			}
		}
		size.cx = max(size.cx, cursor.x += repeat);
		repeat = 0;
	}
}

SixelStream::operator Image()
{
	Clear();

	LTIMING("SixelStream::Get");

	try {
		for(;;) {
			int c = Get() & 0x7F;
			switch(c) {
			case 0x21:
				GetRepeatCount();
				break;
			case 0x22:
				GetRasterInfo();
				break;
			case 0x23:
				SetPalette();
				break;
			case 0x24:
				Return();
				break;
			case 0x2D:
				LineFeed();
				break;
			case 0x18:
			case 0x1A:
			case 0x1B:
			case 0x1C:
				goto Finalize;
			case 0x7F:
				if(IsEof())
					goto Finalize;
				break;
			default:
				if(c > 0x3E)
					PaintSixel(c - 0x3F);
				break;
			}
		}
	}
	catch(const Exc& e) {
		LLOG(e);
	}

Finalize:

	return Crop(buffer, 0, 1, size.cx, max(size.cy, 6));
}
}

