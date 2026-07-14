#ifndef _AnsiParser_h_
#define _AnsiParser_h_

#include <Core/Core.h>

// AnsiParser: A VT500 series "lexical" parser for DEC & ANSI escape sequences.
// This parser is based on the UML state diagram provided by Paul-Flo Williams.
// See: https://vt100.net/emu/dec_ansi_parser

// Deviations from the DEC STD-070:
// 1) ISO 8613-6: 0x3a ("colon") is considered as a legitimate delimiter.
// 2) The OSC sequences allow UTF-8 payload if the UTF-8 mode is enabled.

namespace Upp {

class AnsiParser {
public:
    struct Sequence {
        enum Type : byte { NUL = 0, ESC, CSI, DCS, OSC, APC, PM, SOS };
        byte            type;
        byte            opcode;
        byte            mode;
        byte            intermediate[4];
        Vector<String>  parameters;
        String          payload;
        int             GetInt(int n, int d = 1) const;
        String          GetStr(int n) const;
        String          ToString() const;
        dword           GetHashValue() const;
        void            Clear();
        Sequence()                                          { Clear(); }
    };
    
    struct State : Moveable<State> {
        enum  class Id : byte {
            Ground,
            EscEntry,
            EscIntermediate,
            CsiEntry,
            CsiIntermediate,
            CsiParameter,
            CsiIgnore,
            DcsEntry,
            DcsIntermediate,
            DcsParameter,
            DcsIgnore,
            DcsPassthrough,
            OscString,
            ApcString,
            Repeat,
            Ignore
        };

        enum class Action : byte {
            Mode,
            Collect,
            Parameter,
            Final,
            Control,
            Passthrough,
            String,
            Ignore,
            Ground,
            DispatchEsc,
            DispatchCsi,
            DispatchDcs,
            DispatchOsc,
            DispatchApc
        };
 
        byte    begin;
        byte    end;
        Action  action;
        Id      next;
        
        static const State& GetVoid();

        State(byte b, byte e, Action a, Id id)
        : begin(b)
        , end(e)
        , action(a)
        , next(id)
        {
        }
    };
    
public:
    AnsiParser& ParametrizePayload(bool b = true)               { parametrize = b; return *this; }
    AnsiParser& DontParametrizePayload()                        { return ParametrizePayload(false); }

    void        Parse(const void *data, int size, bool utf8);
    void        Parse(const String& data, bool utf8)            { Parse(~data, data.GetLength(), utf8); }
                
    int         Peek() const                                    { return IsEof() ? -1 : *ptr; }
    int         Get()                                           { return IsEof() ? -1 : *ptr++; }
                
    bool        IsEof() const                                   { return ptr >= end; }
    void        Reset();
    bool        WasChr() const                                  { return waschr; }
   
    Event<byte> WhenCtl;
    Event<const int*, const byte*, int> WhenChr;
    Event<const AnsiParser::Sequence&>  WhenEsc;
    Event<const AnsiParser::Sequence&>  WhenCsi;
    Event<const AnsiParser::Sequence&>  WhenDcs;
    Event<const AnsiParser::Sequence&>  WhenOsc;
    Event<const AnsiParser::Sequence&>  WhenApc;

    AnsiParser();
    virtual ~AnsiParser() {}
    
private:
    int             GetChr();
    void            CheckLoadData(const char *data, int size, String& err);
    void            NextState(State::Id sid);
    const State*    GetState(int c) const;
    void            Dispatch(byte type, const Event<const AnsiParser::Sequence&>& fn);
    void            Reset0(const Vector<AnsiParser::State>* st);
    
    // Collectors.
    void            CollectChr(int c);
    void            CollectIntermediate(int c);
    void            CollectParameter(byte *start, int c);
    void            CollectPayload(byte *start, int c);
    void            CollectString(byte *start, int c);

private:
    byte *ptr, *begin, *end;
    Sequence    sequence;
    bool        waschr:1;
    bool        utf8mode:1;
    bool        parametrize:1;
    String      collected, buffer;
    const Vector<AnsiParser::State>*  state;
};

// Backward compatibility
using VTInStream = AnsiParser;

}
#endif