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
	
	LLOG(seq);
	
	if(seq.payload[0] != 'G')
		return false;

	String params, enc;
	if(!SplitTo(seq.payload, ';', false, params, enc) || IsNull(enc))
		return false;

	int id = 0;
	int width = 0;
	int height = 0;
	bool png = false;
	bool more = false;
	bool query = false;
	bool compressed = false;
	
	CParser p(~params + 1);
	p.SkipSpaces();
	
	try {
		while(!p.IsEof()) {
			if(p.Char2('i', '=')) {
				id = p.ReadInt();
			}
			if(p.Char2('m', '=')) {
				more = p.ReadInt() == 1;
			}
			if(p.Char2('s', '=')) {
				width = p.ReadInt();
			}
			if(p.Char2('v', '=')) {
				height = p.ReadInt();
			}
			if(p.Char2('f', '=')) {
				png = p.ReadInt() == 100;
			}
			if(p.Char3('a', '=', 'q')) {
				query = true;
			}
			if(p.Char3('o', '=', 'z')) {
				compressed = true;
			}
			if(p.Char2('t', '=')) {
				if(p.GetChar() != 'd')
					p.ThrowError();
			}
			else
				p.Skip();
		}
		if(query) {
			Size sz = GetSize();
			PutAPC(String()
				<< "Gi=" << id
				<< ",t=d"
				<< ",f=100"
				<< ",s=" << sz.cx
				<< ",v=" << sz.cy
				<< ";OK");
			return true;
		}

		datachunks << enc;

		if(datachunks.GetLength() >= 256 * 1024 * 1024) {
			datachunks.Clear();
			p.ThrowError();
		}

		if(more)
			return true;
	
		ImageString simg(pick(datachunks));
		datachunks.Clear();
		
		simg.compressed = compressed;
	
		if(width > 0 && height > 0)
			simg.size = Size(width, height);
		else
			simg.size.SetNull();
		
		RenderImage(simg, !modes[DECSDM]); // rely on sixel scrolling mode

	}
	catch(CParser::Error)
	{
		LLOG("Failed to parse kitty graphics protocol");
	}
	
	return true;

}

}