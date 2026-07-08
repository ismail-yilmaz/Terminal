#ifndef _VTCell_h_
#define _VTCell_h_

#include <Core/Core.h>

namespace Upp {

struct VTCell : Moveable<VTCell> {
    dword   chr; // TODO: Grapheme support.

    union {
        dword data;
        struct {
            word col; // Max. 65536
            word row; // Max. 65536
        } object;
    };

    union {
        word attrs;
        struct {
            word protect_dec : 1;
            word protect_iso : 1;
            word semantic    : 2;  // Bits 2-3 (Fits 0 to 3: None, Prompt, Input, Output)
            word reserved    : 12;
        } attr;
    };

    union {
        word sgr;
        struct {
            word bold             : 1;
            word italic           : 1;
            word underline        : 1;
            word overline         : 1;
            word strikeout        : 1;
            word blink            : 1;
            word inverted         : 1;
            word hidden           : 1;
            word faint            : 1;
            word image            : 1;
            word hyperlink        : 1;
            word annotation       : 1;
            word underlinestyle   : 2; // Bits 12-13 (Fits 0 to 3: Single, Double, Curly)
            word reserved         : 2;
        } style;
    };

    Color   ink;
    Color   paper;

    enum SemanticType {
        SEMANTIC_NONE   = 0,
        SEMANTIC_PROMPT = 1,
        SEMANTIC_INPUT  = 2,
        SEMANTIC_OUTPUT = 3
    };

    enum UnderlineStyle {
        UNDERLINE_SINGLE = 0,
        UNDERLINE_DOUBLE = 1,
        UNDERLINE_CURLY  = 2,
    };

    enum FillerFlags : dword {
        FILL_NORMAL        = 0x0000,
        FILL_DEC_SELECTIVE = 0x0001,
        FILL_ISO_SELECTIVE = 0x0002,
        FILL_CHAR          = 0x0004,
        FILL_ATTRS         = 0x0008,
        FILL_SGR           = 0x0010,
        FILL_INK           = 0x0020,
        FILL_PAPER         = 0x0040,
        FILL_DATA          = 0x0080,
        XOR_SGR            = 0x0100,
    };

    VTCell& Normal()                             { sgr = 0; return *this; }
    VTCell& Bold(bool b = true)                  { style.bold = b; return *this; }
    VTCell& Faint(bool b = true)                 { style.faint = b; return *this; }
    VTCell& Italic(bool b = true)                { style.italic = b; return *this; }
    VTCell& Overline(bool b = true)              { style.overline = b; return *this; }
    VTCell& Blink(bool b = true)                 { style.blink = b; return *this; }
    VTCell& Strikeout(bool b = true)             { style.strikeout = b; return *this; }
    VTCell& Invert(bool b = true)                { style.inverted = b; return *this; }
    VTCell& Conceal(bool b = true)               { style.hidden = b; return *this; }
    VTCell& Image(bool b = true)                 { style.image = b; return *this; }
    VTCell& Hyperlink(bool b = true)             { style.hyperlink = b; return *this; }
    VTCell& Annotation(bool b= true)             { style.annotation = b; return *this;}

    VTCell& Underline(bool b = true)             { style.underline = b; if(!b) style.underlinestyle = UNDERLINE_SINGLE; return *this; }
    VTCell& SetUnderlineStyle(UnderlineStyle st) { style.underlinestyle = st; return Underline(true); }

    VTCell& ProtectDEC(bool b = true)            { attr.protect_dec = b; return *this; }
    VTCell& ProtectISO(bool b = true)            { attr.protect_iso = b; return *this; }
    VTCell& Protect(bool b = true)               { attr.protect_dec = b; attr.protect_iso = b; return *this; }

    VTCell& SetAsPrompt(bool b = true)           { attr.semantic = b ? SEMANTIC_PROMPT : SEMANTIC_NONE; return *this; }
    VTCell& SetAsInput(bool b = true)            { attr.semantic = b ? SEMANTIC_INPUT : SEMANTIC_NONE; return *this;  }
    VTCell& SetAsOutput(bool b = true)           { attr.semantic = b ? SEMANTIC_OUTPUT : SEMANTIC_NONE; return *this; }
    VTCell& ClearSemanticInfo()                  { attr.semantic = SEMANTIC_NONE; return *this; }

    static const VTCell& Void();

    VTCell& Ink(Color c)                         { ink = c; return *this;   }
    VTCell& Paper(Color c)                       { paper = c; return *this; }

    int  GetWidth(int ambiguouswidth = 1) const;

    bool IsVoid() const                          { return this == &Void();       }
    bool IsNormal() const                        { return sgr == 0;              }
    bool IsBold() const                          { return style.bold;            }
    bool IsFaint() const                         { return style.faint;           }
    bool IsItalic() const                        { return style.italic;          }
    bool IsUnderlined() const                    { return style.underline;       }
    bool IsOverlined() const                     { return style.overline;        }
    bool IsBlinking() const                      { return style.blink;           }
    bool IsInverted() const                      { return style.inverted;        }
    bool IsStrikeout() const                     { return style.strikeout;       }
    bool IsConcealed() const                     { return style.hidden;          }
    bool IsImage() const                         { return style.image;           }
    bool IsHyperlink() const                     { return style.hyperlink;       }
    bool IsAnnotation() const                    { return style.annotation;      }
    int  GetUnderlineStyle() const               { return style.underlinestyle;  }

    bool IsWideCharTrail() const                 { return chr == 1;              }
    bool IsSpecial() const                       { return chr > 0 && chr < 32;   }
    bool IsHypertext() const                     { return IsHyperlink() || IsAnnotation(); }

    bool IsPrompt() const                        { return attr.semantic == SEMANTIC_PROMPT; }
    bool IsInput() const                         { return attr.semantic == SEMANTIC_INPUT;  }
    bool IsOutput() const                        { return attr.semantic == SEMANTIC_OUTPUT; }
    bool HasSemanticInfo() const                 { return attr.semantic != SEMANTIC_NONE;   }

    bool IsProtected() const                     { return attr.protect_dec || attr.protect_iso; }
    bool HasDECProtection() const                { return attr.protect_dec; }
    bool HasISOProtection() const                { return attr.protect_iso; }
    bool IsNullInstance() const;

    void    Fill(const VTCell& filler, dword flags);
    void    Reset();

    void    Clear()                              { Reset(); chr = 0; attrs = 0; }
    void    operator=(const Nuller&)             { Clear(); }
    void    operator=(dword c)                   { chr = c; }
    operator dword() const                       { return chr; }

    hash_t  GetHashValue() const;
    void    Serialize(Stream& s);

    VTCell()                                     { Clear(); }
};

}
#endif