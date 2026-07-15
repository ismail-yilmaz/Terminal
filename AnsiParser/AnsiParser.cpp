#include "AnsiParser.h"

// AnsiParser: A "lexical" parser for DEC and ANSI escape sequences in general.
// This parser is based on the UML state diagram provided by Paul-Flo Williams
// See: https://vt100.net/emu/dec_ansi_parser

// Deviations from the DEC STD-070:
// 1) ISO 8613-6: 0x3a ("colon") is considered as a legitimate delimiter.
// 2) The OSC sequences allow UTF-8 payload if the UTF-8 mode is enabled.

#define LLOG(x)	   // RLOG("AnsiParser: " << x);
#define LTIMING(x) // RTIMING(x)

namespace Upp {

#define VT_BEGIN_STATE_MAP(sname)                   \
	const static Vector<AnsiParser::State> sname =  {

#define VT_END_STATE_MAP                            \
	}

#define VT_STATE(begin, end, action, next)          \
	{                                               \
		begin,                                      \
		end,                                        \
		AnsiParser::State::Action::action,          \
		AnsiParser::State::Id::next                 \
	}

VT_BEGIN_STATE_MAP(EscEntry)
	VT_STATE(0x00, 0x17, Control,       Repeat),
	VT_STATE(0x18, 0x18, Control,       Ground),
	VT_STATE(0x19, 0x19, Control,       Repeat),
	VT_STATE(0x1a, 0x1a, Control,       Ground),
	VT_STATE(0x1b, 0x1b, Ignore,        Repeat),
	VT_STATE(0x1c, 0x1f, Control,       Repeat),
	VT_STATE(0x20, 0x2f, Collect,       EscIntermediate),
	VT_STATE(0x30, 0x4f, DispatchEsc,   Ground),
	VT_STATE(0x50, 0x50, Ignore,        DcsEntry),
	VT_STATE(0x51, 0x57, DispatchEsc,   Ground),
	VT_STATE(0x58, 0x58, Ignore,        SosString),
	VT_STATE(0x59, 0x59, DispatchEsc,   Ground),
	VT_STATE(0x5a, 0x5a, DispatchEsc,   Ground),
	VT_STATE(0x5b, 0x5b, Ignore,        CsiEntry),
	VT_STATE(0x5c, 0x5c, DispatchEsc,   Ground),
	VT_STATE(0x5d, 0x5d, Ignore,        OscString),
	VT_STATE(0x5e, 0x5e, Ignore,        PmString),
	VT_STATE(0x5f, 0x5f, Ignore,        ApcString),
	VT_STATE(0x60, 0x7e, DispatchEsc,   Ground),
	VT_STATE(0x7f, 0x7f, Control,       Repeat),
	VT_STATE(0x80, 0x8f, Control,       Ground),
	VT_STATE(0x90, 0x90, Ignore,        DcsEntry),
	VT_STATE(0x91, 0x97, Control,       Ground),
	VT_STATE(0x98, 0x98, Ignore,        SosString),
	VT_STATE(0x99, 0x9a, Control,       Ground),
	VT_STATE(0x9b, 0x9b, Ignore,        CsiEntry),
	VT_STATE(0x9c, 0x9c, Ignore,        Repeat),
	VT_STATE(0x9d, 0x9d, Ignore,        OscString),
	VT_STATE(0x9e, 0x9e, Ignore,        PmString),
	VT_STATE(0x9f, 0x9f, Ignore,        ApcString)
VT_END_STATE_MAP;

VT_BEGIN_STATE_MAP(EscIntermediate)
	VT_STATE(0x00, 0x17, Control,       Repeat),
	VT_STATE(0x18, 0x18, Control,       Ground),
	VT_STATE(0x19, 0x19, Control,       Repeat),
	VT_STATE(0x1a, 0x1a, Control,       Ground),
	VT_STATE(0x1b, 0x1b, Ignore,        EscEntry),
	VT_STATE(0x1c, 0x1f, Control,       Repeat),
	VT_STATE(0x20, 0x2f, Collect,       Repeat),
	VT_STATE(0x30, 0x7e, DispatchEsc,   Ground),
	VT_STATE(0x7f, 0x7f, Control,       Repeat),
	VT_STATE(0x80, 0x8f, Control,       Ground),
	VT_STATE(0x90, 0x90, Ignore,        DcsEntry),
	VT_STATE(0x91, 0x97, Control,       Ground),
	VT_STATE(0x98, 0x98, Ignore,        SosString),
	VT_STATE(0x99, 0x9a, Control,       Ground),
	VT_STATE(0x9b, 0x9b, Ignore,        CsiEntry),
	VT_STATE(0x9c, 0x9c, Ignore,        Repeat),
	VT_STATE(0x9d, 0x9d, Ignore,        OscString),
	VT_STATE(0x9e, 0x9e, Ignore,        PmString),
	VT_STATE(0x9f, 0x9f, Ignore,        ApcString)
VT_END_STATE_MAP;

VT_BEGIN_STATE_MAP(CsiEntry)
	VT_STATE(0x00, 0x17, Control,       Repeat),
	VT_STATE(0x18, 0x18, Control,       Ground),
	VT_STATE(0x19, 0x19, Control,       Repeat),
	VT_STATE(0x1a, 0x1a, Control,       Ground),
	VT_STATE(0x1b, 0x1b, Ignore,        EscEntry),
	VT_STATE(0x1c, 0x1f, Control,       Repeat),
	VT_STATE(0x20, 0x2f, Collect,       CsiIntermediate),
	VT_STATE(0x30, 0x3b, Parameter,     CsiParameter),
	VT_STATE(0x3c, 0x3f, Mode,          CsiParameter),
	VT_STATE(0x40, 0x7e, DispatchCsi,   Ground),
	VT_STATE(0x7f, 0x7f, Control,       Repeat),
	VT_STATE(0x80, 0x8f, Control,       Ground),
	VT_STATE(0x90, 0x90, Ignore,        DcsEntry),
	VT_STATE(0x91, 0x97, Control,       Ground),
	VT_STATE(0x98, 0x98, Ignore,        SosString),
	VT_STATE(0x99, 0x9a, Control,       Ground),
	VT_STATE(0x9b, 0x9b, Ignore,        Repeat),
	VT_STATE(0x9c, 0x9c, Ignore,        Repeat),
	VT_STATE(0x9d, 0x9d, Ignore,        OscString),
	VT_STATE(0x9e, 0x9e, Ignore,        PmString),
	VT_STATE(0x9f, 0x9f, Ignore,        ApcString)
VT_END_STATE_MAP;

VT_BEGIN_STATE_MAP(CsiParameter)
	VT_STATE(0x00, 0x17, Control,       Repeat),
	VT_STATE(0x18, 0x18, Control,       Ground),
	VT_STATE(0x19, 0x19, Control,       Repeat),
	VT_STATE(0x1a, 0x1a, Control,       Ground),
	VT_STATE(0x1b, 0x1b, Ignore,        EscEntry),
	VT_STATE(0x1c, 0x1f, Control,       Repeat),
	VT_STATE(0x20, 0x2f, Collect,       CsiIntermediate),
	VT_STATE(0x30, 0x3b, Parameter,     Repeat),
	VT_STATE(0x3c, 0x3f, Ignore,        CsiIgnore),
	VT_STATE(0x40, 0x7e, DispatchCsi,   Ground),
	VT_STATE(0x7f, 0x7f, Control,       Repeat),
	VT_STATE(0x80, 0x8f, Control,       Ground),
	VT_STATE(0x90, 0x90, Ignore,        DcsEntry),
	VT_STATE(0x91, 0x97, Control,       Ground),
	VT_STATE(0x98, 0x98, Ignore,        SosString),
	VT_STATE(0x99, 0x9a, Control,       Ground),
	VT_STATE(0x9b, 0x9b, Ignore,        CsiEntry),
	VT_STATE(0x9c, 0x9c, Ignore,        Repeat),
	VT_STATE(0x9d, 0x9d, Ignore,        OscString),
	VT_STATE(0x9e, 0x9e, Ignore,        PmString),
	VT_STATE(0x9f, 0x9f, Ignore,        ApcString)
VT_END_STATE_MAP;

VT_BEGIN_STATE_MAP(CsiIntermediate)
	VT_STATE(0x00, 0x17, Control,       Repeat),
	VT_STATE(0x18, 0x18, Control,       Ground),
	VT_STATE(0x19, 0x19, Control,       Repeat),
	VT_STATE(0x1a, 0x1a, Control,       Ground),
	VT_STATE(0x1b, 0x1b, Ignore,        EscEntry),
	VT_STATE(0x1c, 0x1f, Control,       Repeat),
	VT_STATE(0x20, 0x2f, Collect,       Repeat),
	VT_STATE(0x30, 0x3f, Ignore,        CsiIgnore),
	VT_STATE(0x40, 0x7e, DispatchCsi,   Ground),
	VT_STATE(0x7f, 0x7f, Control,       Repeat),
	VT_STATE(0x80, 0x8f, Control,       Ground),
	VT_STATE(0x90, 0x90, Ignore,        DcsEntry),
	VT_STATE(0x91, 0x97, Control,       Ground),
	VT_STATE(0x98, 0x98, Ignore,        SosString),
	VT_STATE(0x99, 0x9a, Control,       Ground),
	VT_STATE(0x9b, 0x9b, Ignore,        CsiEntry),
	VT_STATE(0x9c, 0x9c, Ignore,        Repeat),
	VT_STATE(0x9d, 0x9d, Ignore,        OscString),
	VT_STATE(0x9e, 0x9e, Ignore,        PmString),
	VT_STATE(0x9f, 0x9f, Ignore,        ApcString)
VT_END_STATE_MAP;

VT_BEGIN_STATE_MAP(CsiIgnore)
	VT_STATE(0x00, 0x17, Control,       Repeat),
	VT_STATE(0x18, 0x18, Control,       Ground),
	VT_STATE(0x19, 0x19, Control,       Repeat),
	VT_STATE(0x1a, 0x1a, Control,       Ground),
	VT_STATE(0x1b, 0x1b, Ignore,        EscEntry),
	VT_STATE(0x1c, 0x1f, Control,       Repeat),
	VT_STATE(0x20, 0x3f, Ignore,        Repeat),
	VT_STATE(0x40, 0x7e, Ignore,        Ground),
	VT_STATE(0x7f, 0x7f, Control,       Repeat),
	VT_STATE(0x80, 0x8f, Control,       Ground),
	VT_STATE(0x90, 0x90, Ignore,        DcsEntry),
	VT_STATE(0x91, 0x97, Control,       Ground),
	VT_STATE(0x98, 0x98, Ignore,        SosString),
	VT_STATE(0x99, 0x9a, Control,       Ground),
	VT_STATE(0x9b, 0x9b, Ignore,        CsiEntry),
	VT_STATE(0x9c, 0x9c, Ignore,        Repeat),
	VT_STATE(0x9d, 0x9d, Ignore,        OscString),
	VT_STATE(0x9e, 0x9e, Ignore,        PmString),
	VT_STATE(0x9f, 0x9f, Ignore,        ApcString)
VT_END_STATE_MAP;

VT_BEGIN_STATE_MAP(DcsEntry)
	VT_STATE(0x00, 0x17, Ignore,        Repeat),
	VT_STATE(0x18, 0x18, Control,       Ground),
	VT_STATE(0x19, 0x19, Ignore,        Repeat),
	VT_STATE(0x1a, 0x1a, Control,       Ground),
	VT_STATE(0x1b, 0x1b, Ignore,        EscEntry),
	VT_STATE(0x1c, 0x1f, Ignore,        Repeat),
	VT_STATE(0x20, 0x2f, Collect,       DcsIntermediate),
	VT_STATE(0x30, 0x3b, Parameter,     DcsParameter),
	VT_STATE(0x3c, 0x3f, Mode,          DcsParameter),
	VT_STATE(0x40, 0x7e, Final,         DcsPassthrough),
	VT_STATE(0x7f, 0x7f, Control,       Repeat),
	VT_STATE(0x80, 0x8f, Control,       Ground),
	VT_STATE(0x90, 0x90, Ignore,        Repeat),
	VT_STATE(0x91, 0x97, Control,       Ground),
	VT_STATE(0x98, 0x98, Ignore,        SosString),
	VT_STATE(0x99, 0x9a, Control,       Ground),
	VT_STATE(0x9b, 0x9b, Ignore,        CsiEntry),
	VT_STATE(0x9c, 0x9c, Ignore,        Repeat),
	VT_STATE(0x9d, 0x9d, Ignore,        OscString),
	VT_STATE(0x9e, 0x9e, Ignore,        PmString),
	VT_STATE(0x9f, 0x9f, Ignore,        ApcString)
VT_END_STATE_MAP;

VT_BEGIN_STATE_MAP(DcsParameter)
	VT_STATE(0x00, 0x17, Ignore,        Repeat),
	VT_STATE(0x18, 0x18, Control,       Ground),
	VT_STATE(0x19, 0x19, Ignore,        Repeat),
	VT_STATE(0x1a, 0x1a, Control,       Ground),
	VT_STATE(0x1b, 0x1b, Ignore,        EscEntry),
	VT_STATE(0x1c, 0x1f, Ignore,        Repeat),
	VT_STATE(0x20, 0x2f, Collect,       DcsIntermediate),
	VT_STATE(0x30, 0x3b, Parameter,     Repeat),
	VT_STATE(0x3c, 0x3f, Ignore,        DcsIgnore),
	VT_STATE(0x40, 0x7e, Final,         DcsPassthrough),
	VT_STATE(0x7f, 0x7f, Control,       Repeat),
	VT_STATE(0x80, 0x8f, Control,       Ground),
	VT_STATE(0x90, 0x90, Ignore,        DcsEntry),
	VT_STATE(0x91, 0x97, Control,       Ground),
	VT_STATE(0x98, 0x98, Ignore,        SosString),
	VT_STATE(0x99, 0x9a, Control,       Ground),
	VT_STATE(0x9b, 0x9b, Ignore,        CsiEntry),
	VT_STATE(0x9c, 0x9c, Ignore,        Repeat),
	VT_STATE(0x9d, 0x9d, Ignore,        OscString),
	VT_STATE(0x9e, 0x9e, Ignore,        PmString),
	VT_STATE(0x9f, 0x9f, Ignore,        ApcString)
VT_END_STATE_MAP;

VT_BEGIN_STATE_MAP(DcsIntermediate)
	VT_STATE(0x00, 0x17, Ignore,        Repeat),
	VT_STATE(0x18, 0x18, Control,       Ground),
	VT_STATE(0x19, 0x19, Ignore,        Repeat),
	VT_STATE(0x1a, 0x1a, Control,       Ground),
	VT_STATE(0x1b, 0x1b, Ignore,        EscEntry),
	VT_STATE(0x1c, 0x1f, Ignore,        Repeat),
	VT_STATE(0x20, 0x2f, Collect,       Repeat),
	VT_STATE(0x30, 0x3f, Ignore,        DcsIgnore),
	VT_STATE(0x40, 0x7e, Final,         DcsPassthrough),
	VT_STATE(0x7f, 0x7f, Control,       Repeat),
	VT_STATE(0x80, 0x8f, Control,       Ground),
	VT_STATE(0x90, 0x90, Ignore,        DcsEntry),
	VT_STATE(0x91, 0x97, Control,       Ground),
	VT_STATE(0x98, 0x98, Ignore,        SosString),
	VT_STATE(0x99, 0x9a, Control,       Ground),
	VT_STATE(0x9b, 0x9b, Ignore,        CsiEntry),
	VT_STATE(0x9c, 0x9c, Ignore,        Repeat),
	VT_STATE(0x9d, 0x9d, Ignore,        OscString),
	VT_STATE(0x9e, 0x9e, Ignore,        PmString),
	VT_STATE(0x9f, 0x9f, Ignore,        ApcString)
VT_END_STATE_MAP;

VT_BEGIN_STATE_MAP(DcsPassthrough)
	VT_STATE(0x00, 0x17, Passthrough,   Repeat),
	VT_STATE(0x18, 0x18, Control,       Ground),
	VT_STATE(0x19, 0x19, Passthrough,   Repeat),
	VT_STATE(0x1a, 0x1a, Control,       Ground),
	VT_STATE(0x1b, 0x1b, DispatchDcs,   EscEntry),
	VT_STATE(0x1c, 0x1f, Passthrough,   Repeat),
	VT_STATE(0x20, 0x7e, Passthrough,   Repeat),
	VT_STATE(0x7f, 0x7f, Control,       Repeat),
	VT_STATE(0x80, 0x8f, Control,       Ground),
	VT_STATE(0x90, 0x90, Ignore,        DcsEntry),
	VT_STATE(0x91, 0x97, Control,       Ground),
	VT_STATE(0x98, 0x98, Ignore,        SosString),
	VT_STATE(0x99, 0x9a, Control,       Ground),
	VT_STATE(0x9b, 0x9b, Ignore,        CsiEntry),
	VT_STATE(0x9c, 0x9c, DispatchDcs,   Ground),
	VT_STATE(0x9d, 0x9d, Ignore,        OscString),
	VT_STATE(0x9e, 0x9e, Ignore,        PmString),
	VT_STATE(0x9f, 0x9f, Ignore,        ApcString)
VT_END_STATE_MAP;

VT_BEGIN_STATE_MAP(DcsIgnore)
	VT_STATE(0x00, 0x17, Ignore,        Repeat),
	VT_STATE(0x18, 0x18, Control,       Ground),
	VT_STATE(0x19, 0x19, Ignore,        Repeat),
	VT_STATE(0x1a, 0x1a, Control,       Ground),
	VT_STATE(0x1b, 0x1b, Ignore,        EscEntry),
	VT_STATE(0x1c, 0x1f, Ignore,        Repeat),
	VT_STATE(0x20, 0x7f, Ignore,        Repeat),
	VT_STATE(0x80, 0x8f, Control,       Ground),
	VT_STATE(0x90, 0x90, Ignore,        DcsEntry),
	VT_STATE(0x91, 0x97, Control,       Ground),
	VT_STATE(0x98, 0x98, Ignore,        SosString),
	VT_STATE(0x99, 0x9a, Control,       Ground),
	VT_STATE(0x9b, 0x9b, Ignore,        CsiEntry),
	VT_STATE(0x9c, 0x9c, Ignore,        Ground),
	VT_STATE(0x9d, 0x9d, Ignore,        OscString),
	VT_STATE(0x9e, 0x9e, Ignore,        PmString),
	VT_STATE(0x9f, 0x9f, Ignore,        ApcString)
VT_END_STATE_MAP;

VT_BEGIN_STATE_MAP(OscString)
	VT_STATE(0x00, 0x06, Ignore,        Repeat),
	VT_STATE(0x07, 0x07, DispatchOsc,   Ground),
	VT_STATE(0x08, 0x17, Ignore,        Repeat),
	VT_STATE(0x18, 0x18, Control,       Ground),
	VT_STATE(0x19, 0x19, Ignore,        Repeat),
	VT_STATE(0x1a, 0x1a, Control,       Ground),
	VT_STATE(0x1b, 0x1b, DispatchOsc,   EscEntry),
	VT_STATE(0x1c, 0x1f, Ignore,        Repeat),
	VT_STATE(0x20, 0x7f, String,        Repeat),
	VT_STATE(0x80, 0x8f, Control,       Ground),
	VT_STATE(0x90, 0x90, Ignore,        DcsEntry),
	VT_STATE(0x91, 0x97, Control,       Ground),
	VT_STATE(0x98, 0x98, Ignore,        SosString),
	VT_STATE(0x99, 0x9a, Control,       Ground),
	VT_STATE(0x9b, 0x9b, Ignore,        CsiEntry),
	VT_STATE(0x9c, 0x9c, DispatchOsc,   Ground),
	VT_STATE(0x9d, 0x9d, Ignore,        Repeat),
	VT_STATE(0x9e, 0x9e, Ignore,        PmString),
	VT_STATE(0x9f, 0x9f, Ignore,        ApcString),
	VT_STATE(0xa0, 0xff, String,        Repeat)
VT_END_STATE_MAP;

VT_BEGIN_STATE_MAP(ApcString)
	VT_STATE(0x00, 0x06, Ignore,        Repeat),
	VT_STATE(0x07, 0x07, DispatchApc,   Ground),
	VT_STATE(0x08, 0x17, Ignore,        Repeat),
	VT_STATE(0x18, 0x18, Control,       Ground),
	VT_STATE(0x19, 0x19, Ignore,        Repeat),
	VT_STATE(0x1a, 0x1a, Control,       Ground),
	VT_STATE(0x1b, 0x1b, DispatchApc,   EscEntry),
	VT_STATE(0x1c, 0x1f, Ignore,        Repeat),
	VT_STATE(0x20, 0x7f, Passthrough,   Repeat),
	VT_STATE(0x80, 0x8f, Control,       Ground),
	VT_STATE(0x90, 0x90, Ignore,        DcsEntry),
	VT_STATE(0x91, 0x97, Control,       Ground),
	VT_STATE(0x98, 0x98, Ignore,        SosString),
	VT_STATE(0x99, 0x9a, Control,       Ground),
	VT_STATE(0x9b, 0x9b, Ignore,        CsiEntry),
	VT_STATE(0x9c, 0x9c, DispatchApc,   Ground),
	VT_STATE(0x9d, 0x9d, Ignore,        OscString),
	VT_STATE(0x9e, 0x9e, Ignore,        PmString),
	VT_STATE(0x9f, 0x9f, Ignore,        Repeat)
VT_END_STATE_MAP;

VT_BEGIN_STATE_MAP(SosString)
	VT_STATE(0x00, 0x06, Ignore,        Repeat),
	VT_STATE(0x07, 0x07, DispatchSos,   Ground),
	VT_STATE(0x08, 0x17, Ignore,        Repeat),
	VT_STATE(0x18, 0x18, Control,       Ground),
	VT_STATE(0x19, 0x19, Ignore,        Repeat),
	VT_STATE(0x1a, 0x1a, Control,       Ground),
	VT_STATE(0x1b, 0x1b, DispatchSos,   EscEntry),
	VT_STATE(0x1c, 0x1f, Ignore,        Repeat),
	VT_STATE(0x20, 0x7f, Passthrough,   Repeat),
	VT_STATE(0x80, 0x8f, Control,       Ground),
	VT_STATE(0x90, 0x90, Ignore,        DcsEntry),
	VT_STATE(0x91, 0x97, Control,       Ground),
	VT_STATE(0x98, 0x98, Ignore,        Repeat),
	VT_STATE(0x99, 0x9a, Control,       Ground),
	VT_STATE(0x9b, 0x9b, Ignore,        CsiEntry),
	VT_STATE(0x9c, 0x9c, DispatchSos,   Ground),
	VT_STATE(0x9d, 0x9d, Ignore,        OscString),
	VT_STATE(0x9e, 0x9e, Ignore,        PmString),
	VT_STATE(0x9f, 0x9f, Ignore,        ApcString)
VT_END_STATE_MAP;

VT_BEGIN_STATE_MAP(PmString)
	VT_STATE(0x00, 0x06, Ignore,        Repeat),
	VT_STATE(0x07, 0x07, DispatchPm,    Ground),
	VT_STATE(0x08, 0x17, Ignore,        Repeat),
	VT_STATE(0x18, 0x18, Control,       Ground),
	VT_STATE(0x19, 0x19, Ignore,        Repeat),
	VT_STATE(0x1a, 0x1a, Control,       Ground),
	VT_STATE(0x1b, 0x1b, DispatchPm,    EscEntry),
	VT_STATE(0x1c, 0x1f, Ignore,        Repeat),
	VT_STATE(0x20, 0x7f, Passthrough,   Repeat),
	VT_STATE(0x80, 0x8f, Control,       Ground),
	VT_STATE(0x90, 0x90, Ignore,        DcsEntry),
	VT_STATE(0x91, 0x97, Control,       Ground),
	VT_STATE(0x98, 0x98, Ignore,        SosString),
	VT_STATE(0x99, 0x9a, Control,       Ground),
	VT_STATE(0x9b, 0x9b, Ignore,        CsiEntry),
	VT_STATE(0x9c, 0x9c, DispatchPm,    Ground),
	VT_STATE(0x9d, 0x9d, Ignore,        OscString),
	VT_STATE(0x9e, 0x9e, Ignore,        Repeat),
	VT_STATE(0x9f, 0x9f, Ignore,        ApcString)
VT_END_STATE_MAP;

VT_BEGIN_STATE_MAP(Ground)
	VT_STATE(0x00, 0x17, Control,       Repeat),
	VT_STATE(0x18, 0x18, Control,       Repeat),
	VT_STATE(0x19, 0x19, Control,       Repeat),
	VT_STATE(0x1a, 0x1a, Control,       Repeat),
	VT_STATE(0x1b, 0x1b, Ignore,        EscEntry),
	VT_STATE(0x1c, 0x1f, Control,       Repeat),
	VT_STATE(0x20, 0x7e, Ground,        Repeat),
	VT_STATE(0x7f, 0x7f, Control,       Repeat),
	VT_STATE(0x80, 0x8f, Control,       Repeat),
	VT_STATE(0x90, 0x90, Ignore,        DcsEntry),
	VT_STATE(0x91, 0x97, Control,       Repeat),
	VT_STATE(0x98, 0x98, Ignore,        SosString),
	VT_STATE(0x99, 0x9a, Control,       Repeat),
	VT_STATE(0x9b, 0x9b, Ignore,        CsiEntry),
	VT_STATE(0x9c, 0x9c, Ignore,        Repeat),
	VT_STATE(0x9d, 0x9d, Ignore,        OscString),
	VT_STATE(0x9e, 0x9e, Ignore,        PmString),
	VT_STATE(0x9f, 0x9f, Ignore,        ApcString),
	VT_STATE(0xa0, 0xff, Ground,        Repeat)
VT_END_STATE_MAP;

#undef VT_STATE
#undef VT_BEGIN_STATE_MAP
#undef VT_END_STATE_MAP

#ifdef CPU_SIMD
namespace SimdAnsi {
#ifdef CPU_SSE2
force_inline
int MoveMask(i8x16 v)
{
	return _mm_movemask_epi8(v.data);
}
#elif CPU_NEON
force_inline
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

namespace {

struct ParameterPolicy {
#ifdef CPU_SIMD
	static force_inline i8x16 GetInvalidMask(i8x16 chunk)
	{
		return (chunk < i8all(0x30)) | (chunk > i8all(0x3B));
	}
#endif
	static force_inline bool ScalarCheck(int c)
	{
		return c >= 0x30 && c <= 0x3B;
	}
};

struct PayloadPolicy {
#ifdef CPU_SIMD
	static force_inline i8x16 GetInvalidMask(i8x16 chunk)
	{
		i8x16 badascii = (chunk < i8all(0x00)) | (chunk > i8all(0x7E));
		i8x16 ctlrange = (chunk > i8all(0x17)) & (chunk < i8all(0x20));
		i8x16 exempt   = (chunk == i8all(0x19)) | ((chunk > i8all(0x1B)) & (chunk < i8all(0x20)));
		return badascii | (ctlrange & ~exempt);
	}
#endif
	static force_inline bool ScalarCheck(int c)
	{
		return (c >= 0x20 && c <= 0x7E)
			|| (c >= 0x00 && c <= 0x17)
			|| (c >= 0x1C && c <= 0x1F)
			|| c == 0x19;
	}
};

struct StringPolicy {
	bool isutf8;
	StringPolicy(bool utf8) : isutf8(utf8) {}
#ifdef CPU_SIMD
	force_inline i8x16 GetInvalidMask(i8x16 chunk) const
	{
		i8x16 lo = chunk < i8all(0x20);
		i8x16 hi = chunk < i8all(0);
		i8x16 keep = isutf8 ? hi : (chunk > i8all(-97)); // -97 -> 0x9F
		return lo & ~keep;
	}
#endif
	force_inline bool ScalarCheck(int c) const
	{
		return (c >= 0x20 && c <= 0x7F) || (isutf8 ? c >= 0x80 : c > 0x9F);
	}
};

template<class Policy, class T> force_inline
void sCollectInto(T& out, const byte *start, byte*& ptr, const byte* end, const Policy& policy)
{

#ifdef CPU_SIMD
	while(ptr + 64 <= end) {
		i8x16 m0 = policy.GetInvalidMask(i8x16(ptr +  0));
		i8x16 m1 = policy.GetInvalidMask(i8x16(ptr + 16));
		i8x16 m2 = policy.GetInvalidMask(i8x16(ptr + 32));
		i8x16 m3 = policy.GetInvalidMask(i8x16(ptr + 48));
		if(AnyTrue(m0 | m1 | m2 | m3)) {
			uint64 mask = (uint64)(uint16)  SimdAnsi::MoveMask(m0)
						| ((uint64)(uint16) SimdAnsi::MoveMask(m1) << 16)
						| ((uint64)(uint16) SimdAnsi::MoveMask(m2) << 32)
						| ((uint64)(uint16) SimdAnsi::MoveMask(m3) << 48);
			ptr += CountTrailingZeroBits64(mask);
			out.Cat(start, (int)(ptr - start));
			return;
		}
		ptr += 64;
	}
	while(ptr + 16 <= end) {
		i8x16 chunk(ptr);
		if(i8x16 mask = policy.GetInvalidMask(chunk); AnyTrue(mask)) {
			ptr += CountTrailingZeroBits(SimdAnsi::MoveMask(mask));
			out.Cat(start, (int)(ptr - start));
			return;
		}
		ptr += 16;
	}
#endif
	while((ptr < end) && policy.ScalarCheck(*ptr))
		ptr++;

	out.Cat(start, (int)(ptr - start));
}

force_inline
bool sCheckRange(int c, int lo, int hi)
{
	return dword(c - lo) < (hi - lo + 1);
}

force_inline
int sCheckSplit(const char *s, int len)
{
	// Scan backwards up to 4 bytes (maximum valid UTF-8 sequence length)
	const int maxscan = len < 4 ? len : 4;
	for(int n = 1; n <= maxscan; ++n) {
		const byte c = s[len - n];
		if((c & 0x80) == 0x00)
			return 0;
		if((c & 0xC0) == 0xC0) {
			if((c & 0xE0) == 0xC0) // 2-byte sequence
				return n < 2 ? n : 0;
			if((c & 0xF0) == 0xE0) // 3-byte sequence
				return n < 3 ? n : 0;
			if((c & 0xF8) == 0xF0) // 4-byte sequence
				return n < 4 ? n : 0;
			return n; // Malformed leading byte; slice it out safely anyway
		}
	}
	return 0;
}

}

void AnsiParser::Parse(const void *data, int size, bool utf8)
{
	String iutf8;
	utf8mode = utf8;

	LTIMING("AnsiParser::Parse");

	CheckLoadData((const char*) data, size, iutf8);

	while(!IsEof()) {
		byte *start = ptr;
		const int c = GetChr();
		const State* st = GetState(c);
		switch(st->action) {
		case State::Action::Mode:
			sequence.mode = byte(c);
			break;
		case State::Action::Parameter:
			CollectParameter(start, c);
			break;
		case State::Action::Collect:
			CollectIntermediate(c);
			break;
		case State::Action::Final:
			sequence.opcode = byte(c);
			break;
		case State::Action::Control:
			WhenCtl(byte(c));
			break;
		case State::Action::Ground:
			CollectChr(c);
			break;
		case State::Action::Passthrough:
			CollectPayload(start, c);
			break;
		case State::Action::String:
			CollectString(start, c);
			break;
		case State::Action::DispatchEsc:
			sequence.opcode = byte(c);
			Dispatch(Sequence::Type::ESC, WhenEsc);
			break;
		case State::Action::DispatchCsi:
			sequence.opcode = byte(c);
			Dispatch(Sequence::Type::CSI, WhenCsi);
			break;
		case State::Action::DispatchDcs:
			Dispatch(Sequence::Type::DCS, WhenDcs);
			break;
		case State::Action::DispatchOsc:
			Dispatch(Sequence::Type::OSC, WhenOsc);
			break;
		case State::Action::DispatchApc:
			Dispatch(Sequence::Type::APC, WhenApc);
			break;
		case State::Action::DispatchSos:
			Dispatch(Sequence::Type::SOS, WhenSos);
			break;
		case State::Action::DispatchPm:
			Dispatch(Sequence::Type::PM, WhenPm);
			break;
		case State::Action::Ignore:
			break;
		default:
			NEVER();
		}
		NextState(st->next);
	}

	buffer.Clear();
	if(iutf8.GetCount())
		buffer = iutf8;
}

void AnsiParser::NextState(State::Id  sid)
{
	LTIMING("AnsiParser::NextState");

	switch(sid) {
	case State::Id::EscEntry:
		Reset0(&EscEntry);
		break;
	case State::Id::EscIntermediate:
		state = &EscIntermediate;
		break;
	case State::Id::CsiEntry:
		Reset0(&CsiEntry);
		break;
	case State::Id::CsiParameter:
		state = &CsiParameter;
		break;
	case State::Id::CsiIntermediate:
		state = &CsiIntermediate;
		break;
	case State::Id::CsiIgnore:
		state = &CsiIgnore;
		break;
	case State::Id::DcsEntry:
		Reset0(&DcsEntry);
		break;
	case State::Id::DcsParameter:
		state = &DcsParameter;
		break;
	case State::Id::DcsIntermediate:
		state = &DcsIntermediate;
		break;
	case State::Id::DcsPassthrough:
		state = &DcsPassthrough;
		break;
	case State::Id::DcsIgnore:
		state = &DcsIgnore;
		break;
	case State::Id::OscString:
		Reset0(&OscString);
		break;
	case State::Id::ApcString:
		Reset0(&ApcString);
		break;
	case State::Id::SosString:
		Reset0(&SosString);
		break;
	case State::Id::PmString:
		Reset0(&PmString);
		break;
	case State::Id::Repeat:
		break;
	default:
		state = &Ground;
		break;
	}
}

force_inline
const AnsiParser::State* AnsiParser::GetState(int c) const
{
	LTIMING("AnsiParser::GetState");

	if(c >= 0) {
		int l = 0, r = state->GetCount() - 1;
		while(l <= r) {
			int mid = (l + r) >> 1;
			const State& st = (*state)[mid];
			if(c < st.begin)
				r = mid - 1;
			else
			if(c > st.end && st.end != 0xff)	// Allow unicode code points in ground state...
				l = mid + 1;
			else
				return &st;
		}
	}

	return &State::GetVoid();
}

force_inline
int AnsiParser::GetChr()
{
	LTIMING("VtInStream::GetChr()");

	if(ptr >= end)
		return -1;

	if(*ptr < 0x80 || !utf8mode)
		return *ptr++;

	// Using read-ahead.
	byte *savedptr = ptr + 1;
	int code = *ptr++;

	// Check for invalid start byte
	if(code < 0xC2) {
		ptr = savedptr;
		return 0xFFFD;
	}

	// 2-byte sequence
	if(code < 0xE0) {
		if(ptr < end) {
			const byte *p = ptr;
			if((p[0] & 0xC0) == 0x80) {
				ptr += 1;
				return ((code & 0x1F) << 6) | (p[0] & 0x3F);
			}
		}
	}
	// 3-byte sequence
	else
	if(code < 0xF0) {
		if(ptr + 1 < end) {
			const byte *p = ptr;
			byte b0 = p[0];
			byte b1 = p[1];
			if(((b0 & 0xC0) == 0x80) & ((b1 & 0xC0) == 0x80)) {
				int c = ((code & 0x0F) << 12) | ((b0 & 0x3F) << 6) | (b1 & 0x3F);
				if((c >= 0x800) & (c < 0x10000)) {
					ptr += 2;
					return c;
				}
			}
		}
	}
	// 4-byte sequence
	else
	if(code < 0xF8) {
		if(ptr + 2 < end) {
			const byte *p = ptr;
			byte b0 = p[0];
			byte b1 = p[1];
			byte b2 = p[2];
			if(((b0 & 0xC0) == 0x80) & ((b1 & 0xC0) == 0x80) & ((b2 & 0xC0) == 0x80)) {
				int c = ((code & 0x07) << 18) | ((b0 & 0x3F) << 12) | ((b1 & 0x3F) << 6) | (b2 & 0x3F);
				if((c >= 0x10000) & (c < 0x110000)) {
					ptr += 3;
					return c;
				}
			}
		}
	}

	if(!IsEof()) {
		ptr = savedptr;
		return 0xFFFD;
	}

	ptr = savedptr;
	return -1;
}

force_inline
void AnsiParser::CollectChr(int c)
{
	byte *p = ptr;

	do {
		WhenChr(&c, nullptr, 1);
#ifdef CPU_SIMD
		const byte* start = ptr;
		const i8x16 lo = i8all(0x20);
		const i8x16 hi = i8all(0x7E);
		while(ptr + 64 <= end) {
			i8x16 c0(ptr +  0), m0 = (c0 < lo) | (c0 > hi);
			i8x16 c1(ptr + 16), m1 = (c1 < lo) | (c1 > hi);
			i8x16 c2(ptr + 32), m2 = (c2 < lo) | (c2 > hi);
			i8x16 c3(ptr + 48), m3 = (c3 < lo) | (c3 > hi);
			if(AnyTrue(m0 | m1 | m2 | m3)) {
				uint64 mask = (uint64)(uint16)  SimdAnsi::MoveMask(m0)
							| ((uint64)(uint16) SimdAnsi::MoveMask(m1) << 16)
							| ((uint64)(uint16) SimdAnsi::MoveMask(m2) << 32)
							| ((uint64)(uint16) SimdAnsi::MoveMask(m3) << 48);
				ptr += CountTrailingZeroBits64(mask);
				WhenChr(nullptr, start, (int)(ptr - start));
				goto COMPLEX_CHAR_FALLBACK;
			}
			ptr += 64;
		}
		while(ptr + 16 <= end) {
			i8x16 chunk(ptr);
			if(int m = SimdAnsi::MoveMask((chunk < lo) | (chunk > hi)); m != 0) {
				ptr += CountTrailingZeroBits(m);
				WhenChr(nullptr, start, (int)(ptr - start));
				goto COMPLEX_CHAR_FALLBACK;
			}
			ptr += 16;
		}
		if(ptr > start)
			WhenChr(nullptr, start, (int)(ptr - start));
#endif
COMPLEX_CHAR_FALLBACK:
		p = ptr;
		c = GetChr();
	}
	while(sCheckRange(c, 0x20, 0x7E) || c > 0x9F);

	if(c != -1)
		ptr = p;

	waschr = true;
}

force_inline
void AnsiParser::CollectIntermediate(int c)
{
	LTIMING("VtInStream::CollectParameter()");

	int n = 0;
	sequence.intermediate[0] = c;
	while((ptr < end) && sCheckRange(*ptr, 0x20, 0x2F) && n < 3) {
		sequence.intermediate[++n] = *ptr++;
	}
}

force_inline
void AnsiParser::CollectParameter(byte *start, int c)
{
	LTIMING("VtInStream::CollectParameter()");
	
	sCollectInto(collected, start, ptr, end, ParameterPolicy{});
}

force_inline
void AnsiParser::CollectPayload(byte *start, int c)
{
	LTIMING("VtInStream::CollectPayload()");
	
	sCollectInto(sequence.payload, start, ptr, end, PayloadPolicy{});
}

force_inline
void AnsiParser::CollectString(byte *start, int c)
{
	LTIMING("VtInStream::CollectString()");
	
	sCollectInto(sequence.payload, start, ptr, end, StringPolicy{ utf8mode });
}

force_inline
void AnsiParser::Dispatch(Sequence::Type type, const Event<const AnsiParser::Sequence&>& fn)
{
	LTIMING("VtInStream::Dispatch()");

	switch(type) {
	case Sequence::Type::CSI:
	case Sequence::Type::DCS:
		if(collected.GetCount())
			sequence.parameters = pick(Split(collected, ';', false));
		else // We can have empty parameter list, e.g. \033[m
			sequence.parameters.Add();
		break;
	case Sequence::Type::OSC:
	case Sequence::Type::APC:
	case Sequence::Type::SOS:
	case Sequence::Type::PM:
		if(parametrize && sequence.payload.GetCount())
			sequence.parameters = pick(Split(sequence.payload, ';', false));
		break;
	default:
		break;
	}
	sequence.type = type;
	fn(sequence);
	waschr = false;
}

force_inline
void AnsiParser::CheckLoadData(const char *data, int size, String& err)
{
	LTIMING("VtInStream::CheckLoadData()");

	if(!utf8mode) {
		begin = ptr = (byte*) data;
		end = begin + size;
		return;
	}
	// Check for a possibly split UTF-8 sequence at the end of the chunk.
	int n = sCheckSplit(data, size);
	if(n > 0) {
		size -= n;
		err.Cat(data + size, n);
	}
	if(!buffer.IsEmpty()) {
		buffer.Cat(data, size);
		begin = ptr = (byte*) ~buffer;
		end = begin + buffer.GetLength();
	}
	else {
		begin = ptr = (byte*) data;
		end = begin + size;
	}
}

void AnsiParser::Reset()
{
	Reset0(&Ground);
	waschr = false;
	utf8mode = false;
}

void AnsiParser::Reset0(const Vector<AnsiParser::State>* st)
{
	state = st;
	sequence.Clear();
	collected.Clear();
}

AnsiParser::AnsiParser()
: ptr(nullptr)
, begin(nullptr)
, end(nullptr)
, parametrize(false)
{
	Reset();
}

int AnsiParser::Sequence::GetInt(int n, int d) const
{
	LTIMING("VtInStream::Dequence::GetInt()");

	int c = 0, i = 0;
	const char *p = parameters.Get(n - 1, String::GetVoid());
	while(*p && dword((c = *p++) - '0') < 10)
		i = i * 10 + (c - '0');
	return !i ? d : i;
}

String AnsiParser::Sequence::GetStr(int n) const
{
	LTIMING("VtInStream::Dequence::GetStr()");

	return parameters.Get(n - 1, String::GetVoid());
}

void AnsiParser::Sequence::Clear()
{
	type = Type::NUL;
	opcode = mode = 0;
	Zero(intermediate);
	parameters.Clear();
	payload.Clear();
}

String AnsiParser::Sequence::ToString() const
{
    String txt;

    txt << decode(type,
        Type::PM,  "PM  ",
        Type::SOS, "SOS ",
        Type::APC, "APC ",
        Type::OSC, "OSC ",
        Type::DCS, "DCS ",
        Type::CSI, "CSI ",
        "ESC "
    );

    if(intermediate[0] > 0) txt << intermediate[0] << " ";
    if(intermediate[1] > 0) txt << intermediate[1] << " ";

    if(findarg(type, Type::CSI, Type::DCS) >= 0)
        txt << parameters.ToString();

    if(findarg(type, Type::ESC, Type::CSI, Type::DCS, Type::APC) >= 0)
        txt << AsString(opcode) << " ";

    if(mode)
        txt << "(private) ";

    if(!IsNull(payload))
        txt << "Payload: " << payload.ToString();

    return txt;
}

const AnsiParser::State& AnsiParser::State::GetVoid()
{
	static State s(0, 0, Action::Ignore, Id::Repeat);
	return s;
}

}