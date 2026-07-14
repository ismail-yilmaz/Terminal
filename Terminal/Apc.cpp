#include "Terminal.h"

#define LLOG(x)     // RLOG("TerminalCtrl (#" << this << "]: " << x)
#define LTIMING(x)	// RTIMING(x)

namespace Upp {

void TerminalCtrl::ParseApplicationProgrammingCommands(const AnsiParser::Sequence& seq)
{
	if(!kittyimages || !ParseKittyGraphics(seq))
		WhenApplicationCommand(seq.payload);
}

bool TerminalCtrl::ParseKittyGraphics(const AnsiParser::Sequence& seq)
{
	// See: https://sw.kovidgoyal.net/kitty/graphics-protocol/
	
	LLOG(seq);
	
	if(seq.payload[0] != 'G')
		return false;

	String params, enc;
	if(!SplitTo(seq.payload.Mid(1), ';', false, params, enc) || IsNull(enc))
		return false;

	bool more = false;
	bool query = false;
	
	CParser p(params);
	p.SkipSpaces();
	
	auto SendAck = [this](const char *err) {
		PutAPC(Format("Gi=%ld;%s", chunkedimage.id, err));
	};
	
	try {
		while(!p.IsEof()) {
			if(p.Char2('i', '=')) {
				chunkedimage.id = p.ReadInt64();
			}
			if(p.Char2('m', '=')) {
				more = p.ReadInt() == 1;
			}
			if(p.Char2('s', '=')) {
				chunkedimage.size.cx = p.ReadInt(0, 4096);
			}
			if(p.Char2('v', '=')) {
				chunkedimage.size.cy = p.ReadInt(0, 4096);
			}
			if(p.Char2('f', '=')) {
				switch(p.ReadInt()) {
				case 100:
					chunkedimage.FmtRaster();
					break;
				case 32:
					chunkedimage.FmtRGBA();
					break;
				default:
					chunkedimage.FmtRGB();
					break;
				}
			}
			if(p.Char3('a', '=', 'q')) {
				query = true;
			}
			if(p.Char3('o', '=', 'z')) {
				chunkedimage.Compressed();
			}
			if(p.Char2('t', '=')) {
				if(p.GetChar() != 'd')
					p.ThrowError();
			}
			else
				p.Skip();
		}
		if(query) {
			SendAck("OK");
			return true;
		}

		chunkedimage.data << enc; // Accumulate successive chunks

		if(chunkedimage.data.GetLength() >= 256 * 1024 * 1024)
			p.ThrowError();

		if(more)
			return true;
	
		chunkedimage.Encoded();
		RenderImage(chunkedimage);
	}
	catch(CParser::Error)
	{
		LLOG("Failed to parse kitty graphics protocol");
		SendAck("EINVAL");
	}
	catch(...)
	{
		LLOG("Unknown exception");
		SendAck("EINVAL");
	}
	
	chunkedimage.Clear();
	return true;

}

}