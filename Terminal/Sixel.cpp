#include "Sixel.h"

#define LLOG(x)		 // RLOG("SixelStream: " << x)
#define LTIMING(x)	 // RTIMING(x)

namespace Upp {

#ifdef CPU_SIMD
namespace SimdSixel {
#ifdef CPU_SSE2
static force_inline
int MoveMask(i8x16 v)
{
	return _mm_movemask_epi8(v.data);
}
#elif CPU_NEON
static force_inline
int MoveMask(i8x16 v)
{
	constexpr uint8x16_t bitmask = {
		0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80,
		0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80
	};
	uint8x16_t masked = vandq_u8(vreinterpretq_u8_s8(v.data), bitmask);
	uint8x8_t sum = vpadd_u8(vget_low_u8(masked), vget_high_u8(masked));
	sum           = vpadd_u8(sum, sum);
	sum           = vpadd_u8(sum, sum);
	return vget_lane_u16(vreinterpret_u16_u8(sum), 0);
}
#endif
}
#endif

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
, paletteptr(shared_palette)
, background(true)
{
}

SixelStream::SixelStream(const String& data, Palette *shared_palette)
: MemReadStream(~data, data.GetLength())
, paletteptr(shared_palette)
, background(true)
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
		while((c = *ptr) > 0x2F && c < 0x3A)
			n = n * 10 + (*ptr++ - 0x30);
		params[i++ & 7] = n;
		if(c != ';')
			break;
		n = 0;
		ptr++;
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
#ifdef CPU_SIMD
			i8x16 lo = i8all(0x3F);
			i8x16 del = i8all(0x7F);
			while(ptr + 64 <= rdlim && !repeat) {
				i8x16 c0(ptr +  0), m0 = ((c0 & del) < lo) | (c0 == del);
				i8x16 c1(ptr + 16), m1 = ((c1 & del) < lo) | (c1 == del);
				i8x16 c2(ptr + 32), m2 = ((c2 & del) < lo) | (c2 == del);
				i8x16 c3(ptr + 48), m3 = ((c3 & del) < lo) | (c3 == del);
				if(AnyTrue(m0 | m1 | m2 | m3)) {
					uint64 mask = (uint64)(uint16) SimdSixel::MoveMask(m0)
								| ((uint64)(uint16) SimdSixel::MoveMask(m1) << 16)
								| ((uint64)(uint16) SimdSixel::MoveMask(m2) << 32)
								| ((uint64)(uint16) SimdSixel::MoveMask(m3) << 48);
					for(int i = 0, n = CountTrailingZeroBits64(mask); i < n; i++)
						PaintSixel(*ptr++ - 0x03F);
					goto SCALAR_FALLBACK;
				}
                for(int i = 0; i < 64; i++)
                    PaintSixel(*ptr++ - 0x3F);
                size.cx = max(size.cx, cursor.x);
			}
			while(ptr + 16 <= rdlim && !repeat) {
	            i8x16 chunk(ptr);
	            if(int mask = SimdSixel::MoveMask(((chunk & del) < lo) | (chunk == del)); mask != 0) {
                    for(int i = 0, n = CountTrailingZeroBits(mask); i < n; i++)
	                    PaintSixel(*ptr++ - 0x3F);
	                goto SCALAR_FALLBACK;
	            }
                for(int i = 0; i < 16; i++)
                    PaintSixel(*ptr++ - 0x3F);
                size.cx = max(size.cx, cursor.x);
	        }
#endif
SCALAR_FALLBACK:
			if(ptr >= rdlim)
				break;
			
			byte c = *ptr++ & 0x7F;
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

