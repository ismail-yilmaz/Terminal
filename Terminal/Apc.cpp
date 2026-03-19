#include "Terminal.h"

#define LLOG(x)     // RLOG("TerminalCtrl (#" << this << "]: " << x)
#define LTIMING(x)	// RTIMING(x)

namespace Upp {

void TerminalCtrl::ParseApplicationProgrammingCommands(const VTInStream::Sequence& seq)
{
	if(!kittyimages || !ParseKittyGraphics(seq))
		WhenApplicationCommand(seq.payload);
}

bool TerminalCtrl::ParseKittyGraphics(const VTInStream::Sequence& seq)
{
	// See: https://sw.kovidgoyal.net/kitty/graphics-protocol/
	
	if(seq.payload[0] != 'G')
		return false;

	String params, enc;
	if(!SplitTo(seq.payload, ';', false, params, enc) || IsNull(enc))
		return false;

	int width = 0;
	int height = 0;
	bool png = false;
	bool more = false;

	for(const String& s : Split(~params + 1, ',', false)) {
		String key, val;
		if(SplitTo(ToLower(s), '=', false, key, val)) {
			if(key == "f") {
				if(ReadInt(val, 0) == 100)
					png = true;
			}
			else
			if(key == "s")
				width = ReadInt(val, 0);
			else
			if(key == "v")
				height = ReadInt(val, 0);
			else
			if(key == "m")
				more = ReadInt(val, 0) == 1;
		}
	}

	if(!png)
		return false;

	// accumulate base64
	datachunks << enc;

	if(datachunks.GetLength() >= 256 * 1024 * 1024) {
		datachunks.Clear();
		return true;
	}
	
	if(more)
		return true;

	// final chunk
	ImageString simg(pick(datachunks));
	datachunks.Clear();

	if(width > 0 && height > 0)
		simg.size = Size(width, height);
	else
		simg.size.SetNull();

	RenderImage(simg, modes[DECSDM]); // rely on sixel scrolling mode
	return true;
}

}