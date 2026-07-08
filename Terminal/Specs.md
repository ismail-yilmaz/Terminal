## Table of Contents

 1.  [Requirements](#requirements)
 2.  [Supported Platforms](#platforms)
 3.  [Supported Emulation Levels](#emulation-levels)
 4.  [Supported I/O Modes and Parser Capabilities](#Capabilities)
 5.  [Supported Control Bytes](#controlbytes)
 6.  [Supported Terminal Modes](#modes)
 7.  [Supported Escape Sequences](#esc-sequences)
 8.  [Supported Command Sequences](@csi-sequences)
 9.  [Supported Device Control Strings](#dcs-sequences)
 10. [Supported Operating System Commands](#osc-sequences)
 11. [Supported Application Programming Commands](#apc-sequences)
 12. [Supported Graphics Rendition Opcodes](@sgr-sequences)
 13. [Supported Extended Color Sequences](#sgr-extended-sequences)
 14. [Supported Color Text Specifications](#color-text-specs)
 15. [Supported Extended Inline Image Sequences](#inline-image-protocols)
 16. [Supported Window Actions and Reports](#window-ops)
 17. [Other Supported Extensions](#other-extensions)

## [Requirements](#requirements)

* [U++](www.ultimatepp.org), cross-platform C/C++ rapid application development framework.
* A C/C++ compiler, that supports at least C++17.
 
## [Supported Platforms](#platforms)

* POSIX-compliant OSes (Linux, BSD, etc.)
* Microsoft Windows (tm)
* MacOS (tm)

## [Supported Emulation Levels](#emulation-levels)

| Conformance Level | Model Range | Status |
| --- | --- | --- |
| 0 | VT 52 | Supported |
| 1 | VT 1xx | Supported |
| 2 | VT 2xx | Supported |
| 3 | VT 3xx | Supported |
| 4 | VT 4xx (default) | Supported |
| 5 | VT 5xx | Partial |

#### Notes

* In reality, there is no such conformance level as "0". However, since the VT 52 emulation has to be treated as a special case, it is labelled as "level 0", for convenience.
* Not all terminal sequences or modes pertaining to the above listed device conformance levels or model ranges are implemented. Some of these emulations are complete while others cover only a set of selected features.
* DEC terminals are (or were) backward compatible devices. For example, a level 4 conforming device such as, say, VT 420, recognizes virtually all terminal sequences and modes that apply to lower level devices. Terminal package follows this behavior where applicable.
* Level 5 (VT 5xx) emulation is yet to be implemented. However, TerminalCtrl already recognizes and utilizes some useful sequences and modes pertaining to the conformance level 5, such as some cursor movements and caret customization commands.
* Extensions (such as xterm's window manipulation sequences, jexer inline images protocol, or hyperlinks protocol, etc.) apply to all emulation levels, with the reasonable exception of level 0.

## [Supported I/O Modes and Parser Capabilities](#Capabilities)

| Mnemonic | Description | Status | 7-Bit | 8-Bit |
| --- | --- | --- | --- | --- |
| UTF-8 | Unicode transport format | Supported | - | - |
| CTL | Control bytes | Supported | Yes | Yes |
| ESC | Escape sequences | Supported | Yes | Yes |
| CSI | Command sequences | Supported | Yes | Yes |
| DCS | Device control strings | Supported | Yes | Yes |
| OSC | Operating system commands | Supported | Yes | Yes |
| APC | Application programming commands | Supported | Yes | Yes |

#### Notes

* Both 7-bit and 8-bit I/O modes are supported by TerminalCtrl. However, it is highly recommended that 8-bit mode be only enabled to support legacy applications that require it. 8-bit I/O does not play well with UTF-8, since some control bytes in the 8-bit C1 region are also legitimate UTF-8 sequences. They will likely to confuse the parser.
* TerminalCtrl's parser also allows switching between UTF-8 and non UTF-8 modes on-the-fly, if required. This can come in handy on networked environments.
* APCs are supported for both internal and external scripting: Future versions of TerminalCtrl will add an internal scripting interface. Applications using TerminalCtrl can externally utilize the APCs to create their application specific scripting channels.

## [Supported Control Bytes](#controlbytes)

| Mnemonic | Byte | Description | Device Level |
| --- | --- | --- | --- |
| NUL | 0x00 | Ignored. | Level 0 |
| ENQ | 0x05 | Terminal status request. | Level 0 |
| BEL | 0x07 | Audio or visual bell. | Level 0 |
| BS | 0x08 | Backspace. | Level 0 |
| HT | 0x09 | Horizontal tab. Moves the cursor to next tab stop. | Level 0 |
| LF | 0x0A | Line feed. | Level 0 |
| VT | 0x0B | Vertical tab. Treated as line feed. | Level 0 |
| FF | 0x0C | Form feed. Treated as line feed. | Level 0 |
| CR | 0x0D | Carraige return. | Level 0 |
| LS0 | 0x0E | Locking shift 0. Invokes and locks the G0 charset into GL. | Level 1 |
| LS1 | 0x0F | Locking shift 1. Invokes and locks the G1 charset into GL. | Level 1 |
| XON | 0x11 | Resume data transter. | Level 0 |
| XOFF | 0x13 | Pause data transter. | Level 0 |
| DEL | 0x7F | Ignored. | Level 0 |
| IND | 0x84 | Index. | Level 1 |
| NEL | 0x85 | Next line. | Level 1 |
| HTS | 0x88 | Sets a tab at the active cursor position. | Level 1 |
| RI | 0x8D | Reverse index. | Level 1 |
| SS2 | 0x8E | Single shift 2. Temporarily invokes the G2 charset into GL.| Level 1 |
| SS3 | 0x8F | Singls shift 3. Temporarily invokes the G3 charset into GL.| Level 1 |
| SPA | 0x96 | Start of protected area. (ECMA-48) | Level 2 |
| EPA | 0x97 | End of protected area. (ECMA-48) | Level 2 |
| DECID | 0x9A | Return terminal ID. | Level 1 |
| ST | 0x9C | String terminator. | Level 1 |

#### Notes

* TerminalCtrl's responses to commands and report requests are not included in this table.

## [Supported Terminal Modes](#modes)

| Mnemonic | Number | Description | Type | Device Level | Set Sequence | Reset Sequence |
| --- | --- | --- | --- | --- | --- | --- |
| GATM | 1 | Guarded area tranfer mode. | ANSI | Level 1 | `CSI 1 h` | `CSI 1 l` |
| KAM | 2 | Keyboard action mode. | ANSI | Level 1 | `CSI 2 h` | `CSI 2 l` |
| CRM | 3 | Show/hide control characters. | ANSI | Level 1 | `CSI 3 h` | `CSI 3 l` |
| IRM | 4 | Insert/replace characters. | ANSI | Level 1 | `CSI 4 h` | `CSI 4 l` |
| SRTM | 5 | Status report transter mode. | ANSI | Level 1 | `CSI 5 h` | `CSI 5 l` |
| ERM | 6 | Erasure mode. | ANSI | Level 1 | `CSI 6 h` | `CSI 6 l` |
| VEM | 7 | Line editing mode. | ANSI | Level 1 | `CSI 7 h` | `CSI 7 l` |
| HEM | 10 | Character editing mode. | ANSI | Level 1 | `CSI 10 h` | `CSI 10 l` |
| PUM | 11 | Positioning unit mode. | ANSI | Level 1 | `CSI 11 h` | `CSI 11 l` |
| SRM | 12 | Send/receive mode. (Local echo.) | ANSI | Level 1 | `CSI 12 h` | `CSI 12 l` |
| FEAM | 13 | Format effector action mode. | ANSI | Level 1 | `CSI 13 h` | `CSI 13 l` |
| FETM | 14 | Format effector tranfer mode. | ANSI | Level 1 | `CSI 14 h` | `CSI 14 l` |
| MATM | 15 | Multiple area transfer mode. | ANSI | Level 1 | `CSI 15 h` | `CSI 15 l` |
| TTM | 16 | Transer terminaion mode. | ANSI | Level 1 | `CSI 16 h` | `CSI 16 l` |
| SATM | 17 | Selected area transfer mode. | ANSI | Level 1 | `CSI 17 h` | `CSI 17 l` |
| TSM | 18 | Tabulation stop mode. | ANSI | Level 1 | `CSI 18 h` | `CSI 18 l` |
| EBM | 19 | Editing boundary mode | ANSI | Level 1 | `CSI 19 h` | `CSI 19 l` |
| LNM | 20 | Line feed/new line mode. | ANSI | Level 1 | `CSI 20 h` | `CSI 20 l` |
| DECCKM | 1 | Cursor keys mode. | DEC private | Level 1 | `CSI ? 1 h` | `CSI ? 1 l` |
| DECANM | 2 | Leave ANSI mode. (Enter VT52 mode.) | DEC private | Level 1 | `CSI ? 2 h` | `CSI ? 2 l` |
| DECCOLM | 3 | 80/132 columns mode. | DEC private | Level 1 | `CSI ? 3 h` | `CSI ? 3 l` |
| DECSCLM | 4 | Scrolling mode. | DEC private | Level 1 | `CSI ? 4 h` | `CSI ? 4 l` |
| DECSCNM | 5 | Normal/inverse video mode. | DEC private | Level 1 | `CSI ? 5 h` | `CSI ? 5 l` |
| DECOM | 6 | Origin mode. | DEC private | Level 1 | `CSI ? 6 h` | `CSI ? 6 l` |
| DECAWM | 7 | Auto wrap mode. | DEC private | Level 1 | `CSI ? 7 h` | `CSI ? 7 l` |
| DECARM | 8 | Auto repeat mode. | DEC private | Level 1 | `CSI ? 8 h` | `CSI ? 8 l` |
| DECTCEM | 25 | Show/hide caret. | DEC private | Level 2 | `CSI ? 25 h` | `CSI ? 25 l` |
| DECCOLMA | 40 | Allow/Disallow DECCOLM (80/132) mode. | DEC private | Level 1 | `CSI ? 40 h` | `CSI ? 40 l` |
| DECBKM | 67 | Send backspace when backarrow key is pressed. | DEC private | Level 3 | `CSI ? 67 h` | `CSI ? 67 l` |
| DECLRMM | 69 | Enable/disable horizontal margins. | DEC private | Level 4 | `CSI ? 69 h` | `CSI ? 69 l` |
| DECSDM | 80 | Enable/disable sixel scrolling. | DEC private | Level 3 | `CSI ? 80 h` | `CSI ? 80 l` |
| XTX10MM | 9 | X10 mouse button tracking mode. (Compatibility) | xterm private | Level 1 | `CSI ? 9 h` | `CSI ? 9 l` |
| XTSCRLBAR | 30 | Show/hide scroll bar. (rxvt) | xterm private | Level 1 | `CSI ? 30 h` | `CSI ? 30 l` |
| XTREWRAPM | 45 | Reverse wrap mode. | xterm private | Level 1 | `CSI ? 45 h` | `CSI ? 45 l` |
| XTASBM | 47 | Alternate screen buffer mode. (Ver. 1) | xterm private | Level 1 | `CSI ? 47 h` | `CSI ? 47 l` |
| XTX11MM | 1000 | X11 mouse button tracking mode. (Normal mode.) | xterm private | Level 1 | `CSI ? 1000 h` | `CSI ? 1000 l` |
| XTDRAGM | 1002 | X11 mouse motion tracking mode. | xterm private | Level 1 | `CSI ? 1002 h` | `CSI ? 1002 l` |
| XTANYMM | 1003 | X11 mouse motion tracking mode. (Any motion event.) | xterm private | Level 1 | `CSI ? 1003 h` | `CSI ? 1003 l` |
| XTFOCUSM | 1004 | Focus in/out mode. | xterm private | Level 1 | `CSI ? 1004 h` | `CSI ? 1004 l` |
| XTUTF8MM | 1005 | Enable/disable UTF8 mouse tracking coordinates. | xterm private | Level 1 | `CSI ? 1005 h` | `CSI ? 1005 l` |
| XTSGRMM | 1006 | Enable/disable SGR mouse tracking coordinates. | xterm private | Level 1 | `CSI ? 1006 h` | `CSI ? 1006 l` |
| XTASCM | 1007 | Alternate scroll mode | xterm private | Level 1 | `CSI ? 1007 h` | `CSI ? 1007 l` |
| XTSGRPXMM | 1016 | Enable/disable SGR pixel-level mouse tracking. | xterm private | Level 1 | `CSI ? 1016 h` | `CSI ? 1016 l` |
| XTALTESCM | 1039 | Prefix the key with ESC when modified with Alt-key. | xterm private | Level 1 | `CSI ? 1039 h` | `CSI ? 1039 l` |
| XTASBM | 1047 | Alternate screen buffer mode. (Ver. 2) | xterm private | Level 1 | `CSI ? 1047 h` | `CSI ? 1047 l` |
| XTSRCM | 1048 | Save/restore cursor. | xterm private | Level 1 | `CSI ? 1048 h` | `CSI ? 1048 l` |
| XTASBM | 1049 | Alternate screen buffer mode. (Ver. 3) | xterm private | Level 1 | `CSI ? 1049 h` | `CSI ? 1049 l` |
| XTSPREG | 1070 | Use private registers for sixel color palette. | xterm private | Level 1 | `CSI ? 1070 h` | `CSI ? 1070 l` |
| XTBRPM | 2004 | Bracketed paste mode. | xterm private | Level 1 | `CSI ? 2004 h` | `CSI ? 2004 l` |
| XTSYNCOUT | 2026 | Synchronized output mode. | xterm private | Level 1 | `CSI ? 2026 h` | `CSI ? 2026 l` |
| XTGRAPHEME| 2027 | Unicode grapheme cluster processing. | xterm private | Level 1 | `CSI ? 2027 h` | `CSI ? 2027 l` |
| XTRESIZEREP| 2048 | In-band resize notification. | xterm private | Level 1 | `CSI ? 2048 h` | `CSI ? 2048 l` |

#### Notes

* GATM, VEM, HEM, PUM, FEAM, FETM, MATM, TTM, SATM, TSM, EBM, and XTGRAPHEME modes are set as "permanently reset".

## [Supported Escape Sequences](#esc-sequences)

*Note: `<SP>` denotes a space character.*

| Mnemonic | Description | Device Level | Sequence |
| --- | --- | --- | --- |
| ANSICL1 | Select ANSI conformance level 1 | Level 2 | `ESC <SP> L` |
| ANSICL2 | Select ANSI conformance level 2 | Level 2 | `ESC <SP> M` |
| ANSICL3 | Select ANSI conformance level 3 | Level 2 | `ESC <SP> N` |
| DECALN | Display alignment test. | Level 1 | `ESC # 8` |
| DECBI | Back index. | Level 4 | `ESC 6` |
| DECFI | Forward index. | Level 4 | `ESC 9` |
| DECKPAM | Keypad application mode. | Level 0 | `ESC =` |
| DECKPNM | Keypad numeric mode. | Level 0 | `ESC >` |
| DECRC | Restore cursor. | Level 1 | `ESC 8` |
| DECSC | Save cursor. | Level 1 | `ESC 7` |
| LS1R | Locking shift 1 right. Invokes/locks G1 into GR | Level 2 | `ESC ~` |
| LS2 | Locking shift 2 left. Invokes/locks G2 into GL | Level 2 | `ESC n` |
| LS2R | Locking shift 2 right. Invokes/locks G2 into GR | Level 2 | `ESC }` |
| LS3 | Locking shift 3 left Invokes/locks G3 into GL | Level 2 | `ESC o` |
| LS3R | Locking shift 3 right. Invokes/locks G3 into GR | Level 2 | `ESC \|` |
| RIS | Perform hard reset. | Level 1 | `ESC c` |
| S7C1T | Put terminal into 7-bit mode. | Level 2 | `ESC <SP> F` |
| S8C1T | Put terminal into 8-bit mode. | Level 2 | `ESC <SP> G` |
| SCS_DEFAULT | Select default charset. | Level 3 | `ESC % @` |
| SCS_G0_ASCII | Invoke US-ASCII charset into G0. | Level 1 | `ESC ( B` |
| SCS_G1_ASCII | Invoke US-ASCII charset into G1. | Level 1 | `ESC ) B` |
| SCS_G2_ASCII | Invoke US-ASCII charset into G2. | Level 2 | `ESC * B` |
| SCS_G3_ASCII | Invoke US-ASCII charset into G3. | Level 2 | `ESC + B` |
| SCS_G0_DEC_ACS | Invoke DEC standard ROM charset into G0. | Level 1 | `ESC ( C` |
| SCS_G1_DEC_ACS | Invoke DEC standard ROM charset into G1. | Level 1 | `ESC ) C` |
| SCS_G0_DEC_DCS | Invoke DEC line-drawing charset into G0. | Level 1 | `ESC ( 0` |
| SCS_G1_DEC_DCS | Invoke DEC line-drawing charset into G1. | Level 1 | `ESC ) 0` |
| SCS_G2_DEC_DCS | Invoke DEC line-drawing charset into G2. | Level 2 | `ESC * 0` |
| SCS_G3_DEC_DCS | Invoke DEC line-drawing charset into G3. | Level 2 | `ESC + 0` |
| SCS_G0_DEC_MCS | Invoke DEC supplemental charset into G0. | Level 2 | `ESC ( <` |
| SCS_G1_DEC_MCS | Invoke DEC supplemental charset into G1. | Level 2 | `ESC ) <` |
| SCS_G2_DEC_MCS | Invoke DEC supplemental charset into G2. | Level 2 | `ESC * <` |
| SCS_G3_DEC_MCS | Invoke DEC supplemental charset into G3. | Level 2 | `ESC + <` |
| SCS_G0_DEC_TCS | Invoke DEC technical charset into G0. | Level 3 | `ESC ( >` |
| SCS_G1_DEC_TCS | Invoke DEC technical charset into G1. | Level 3 | `ESC ) >` |
| SCS_G2_DEC_TCS | Invoke DEC technical charset into G2. | Level 3 | `ESC * >` |
| SCS_G3_DEC_TCS | Invoke DEC technical charset into G3. | Level 3 | `ESC + >` |
| SCS_G1_LATIN1 | Invoke DEC Latin-1 charset into G1. | Level 3 | `ESC ) A` |
| SCS_G2_LATIN1 | Invoke DEC Latin-1 charset into G2. | Level 3 | `ESC * A` |
| SCS_G3_LATIN1 | Invoke DEC Latin-1 charset into G3. | Level 3 | `ESC + A` |
| SCS_UTF8 | Selects UTF-8 charset. | Level 3 | `ESC % G` |
| VT52_CUB | *VT52 mode: Move backward.* | Level 0 | `ESC D` |
| VT52_CUD | *VT52 mode: Move downward.* | Level 0 | `ESC B` |
| VT52_CUF | *VT52 mode: Move forward.* | Level 0 | `ESC C` |
| VT52_CUP | *VT52 mode: Direct cursor addressing.* | Level 0 | `ESC Y l c` |
| VT52_CUU | *VT52 mode: Move upward.* | Level 0 | `ESC A` |
| VT52_DA | *VT52 mode: Device identify.* | Level 0 | `ESC Z` |
| VT52_DCS_ON | *VT52 mode: Drawing characters: On.* | Level 0 | `ESC F` |
| VT52_DCS_OFF | *VT52 mode: Drawing characters: Off.* | Level 0 | `ESC G` |
| VT52_DECANM | *VT52 mode: Enter ANSI mode. (Leave VT52 mode.)*| Level 0 | `ESC <` |
| VT52_HOME | *VT52 mode: Move home.* | Level 0 | `ESC H` |

#### Notes

* TerminalCtrl's responses to commands and report requests are not included in this table.

## [Supported Command Sequences](@csi-sequences)

*Note: `<SP>` denotes a space character.*

| Mnemonic | Description | Device Level | Sequence |
| --- | --- | --- | --- |
| CBT | Cursor backward tabulation. | Level 3 | `CSI Pn Z` |
| CHA | Cursor horizontal absolute. | Level 4 | `CSI Pn G` |
| CHT | Cursor horizontal tabulation. | Level 4 | `CSI Pn I` |
| CNL | Move to next line. (No scroll.) | Level 4 | `CSI Pn E` |
| CPL | Move to previous line. (No scroll.) | Level 4 | `CSI Pn F` |
| CUB | Move backward. | Level 1 | `CSI Pn D` |
| CUD | Move downward. | Level 1 | `CSI Pn B` |
| CUF | Move forward. | Level 1 | `CSI Pn C` |
| CUP | Cursor position. | Level 1 | `CSI Pn ; Pn H` |
| CUU | Move upward. | Level 1 | `CSI Pn A` |
| DA1 | Send primary device attributes. | Level 1 | `CSI Pn c` |
| DA2 | Send secondary device attributes. | Level 1 | `CSI > Pn c` |
| DA3 | Send tertiary device attributes. | Level 4 | `CSI = Pn c` |
| DCH | Delete character. | Level 1 | `CSI Pn P` |
| DECCARA | Change attributes in rectangular area | Level 4 | `CSI Pt; Pl; Pb; Pr; Ps $ r` |
| DECDC | Delete column. | Level 4 | `CSI Pn ' ~` |
| DECCRA | Copy rectangular area. | Level 4 | `CSI Pt; Pl; Pb; Pr; Ppt; Ppl; Pdest $ v`|
| DECERA | Erase rectangular area. | Level 4 | `CSI Pt; Pl; Pb; Pr $ z` |
| DECFRA | Fill rectangular area. | Level 4 | `CSI Pc; Pt; Pl; Pb; Pr $ x` |
| DECIC | Insert column. | Level 4 | `CSI Pn ' }` |
| DECLL | Set programmable LEDs. | Level 1 | `CSI Pn q` |
| DECRARA | Reverse attributes in rectangular area. | Level 4 | `CSI Pt; Pl; Pb; Pr; Pn $ t` |
| DECRQM | Request mode. | Level 2 | `CSI ? Pn $ p` |
| DECREQTPARM| Request terminal parameters. | Level 1 | `CSI Pn x` |
| DECRQCRA | Request rectangular area checksum. | Level 4 | `CSI Pi; Pp; Pt; Pl; Pb; Pr * y`|
| DECRQPSR | Request presentation state report | Level 3 | `CSI Pn $ w` |
| DECSACE | Select rectangular area attribute change extent. | Level 4 | `CSI Pn * x` |
| DECSCA | Set character protection attribute. | Level 2 | `CSI Pn " q` |
| DECSCL | Select device conformance level. | Level 1 | `CSI Pn ; Pn " p` |
| DECSCPP | Set columns per page. | Level 3 | `CSI Pn $ \|` |
| DECSCUSR | Set cursor style | Level 4 | `CSI Pn <SP> q` |
| DECSGR | Select graphics rendition. (DEC) | Level 2 | `CSI Pn m` |
| DECSED | Selectively erase in display. | Level 2 | `CSI ? Pn J` |
| DECSEL | Selectively erase in line. | Level 2 | `CSI ? Pn K` |
| DECSERA | Selectively erase rectangular area. | Level 4 | `CSI Pt; Pl; Pb; Pr $ {` |
| DECSLPP | Set lines per page. | Level 3 | `CSI Pn t` |
| DECSLRM | Set horizontal margins. | Level 4 | `CSI Pl ; Pr s` |
| DECSNLS | Set number of lines per screen. | Level 4 | `CSI Pn * \|` |
| DECST8C | Set a tab at every 8 columns. | Level 4 | `CSI ? 5 W` |
| DECSTBM | Set vertical margins | Level 1 | `CSI Pt ; Pb r` |
| DECSTR | Soft reset. | Level 2 | `CSI ! p` |
| DECTST | Device confidence tests | Level 1 | `CSI 4 ; Pn y` |
| DL | Delete line. | Level 1 | `CSI Pn M` |
| DSR | Device status report. | Level 1 | `CSI Pn n` |
| ECH | Erase character. | Level 2 | `CSI Pn X` |
| ED | Erase in display. | Level 1 | `CSI Pn J` |
| EL | Erase in line. | Level 1 | `CSI Pn K` |
| HPA | Horizontal position absolute. | Level 1 | `CSI Pn \`` |
| HPR | Horizontal position relative. | Level 1 | `CSI Pn a` |
| HVP | Cursor horizontal and vertical position. | Level 1 | `CSI Pn ; Pn f` |
| ICH | Insert character. | Level 2 | `CSI Pn @` |
| IL | Insert line. | Level 1 | `CSI Pn L` |
| REP | Repeat last character. | Level 3 | `CSI Pn b` |
| RM | Reset mode. | Level 1 | `CSI Pn l` |
| SCORC | SCO restore cursor. | Level 4 | `CSI u` |
| SCOSC | SCO save cursor. | Level 4 | `CSI s` |
| SD | Scroll down. | Level 3 | `CSI Pn T` |
| SGR | Select graphics rendition. (ANSI) | Level 1 | `CSI Pm m` |
| SL | Scroll/shift left. | Level 3 | `CSI Pn <SP> @` |
| SM | Set mode. | Level 1 | `CSI Pn h` |
| SR | Scroll/shift right. | Level 3 | `CSI Pn <SP> A` |
| SU | Scroll up. | Level 3 | `CSI Pn S` |
| TBC | Clear tabs. | Level 1 | `CSI Pn g` |
| VPA | Vertical position absolute. | Level 4 | `CSI Pn d` |
| VPR | Vertical position relative. | Level 4 | `CSI Pn e` |

#### Notes

* TerminalCtrl's responses to commands and report requests are not included in this table.

## [Supported Device Control Strings](#dcs-sequences)

| Mnemonic | Description | Device Level | Sequence |
| --- | --- | --- | --- |
| DECRQSS | Request control function strings. | Level 4 | `DCS $ q Pt ST` |
| DECRSPS | Restore presentation state. | Level 3 | `DCS Pn $ w Pt ST` |
| DECSIXEL | Parse sixel graphics format. | Level 3 | `DCS P1 ; P2 ; P3 q Pt ST`|
| DECUDK | Set user-defined keys. | Level 2 | `DCS Pc ; Pl \| Pt ST` |
| XTGETTCAP | Query terminal capabilities. | Level 1 | `DCS + q Pt ST` |

#### Notes

* TerminalCtrl's responses to commands and report requests are not included in this table.

## [Supported Operating System Commands](#osc-sequences)

| Opcode | Description | Device Level | Sequence |
| --- | --- | --- | --- |
| 0 | Change icon name and window title. | Level 1 | `OSC 0 ; Pt ST` |
| 2 | Change window title. | Level 1 | `OSC 2 ; Pt ST` |
| 4 | Change ANSI colors. | Level 1 | `OSC 4 ; c ; spec ST` |
| 7 | Change working directory | Level 1 | `OSC 7 ; Pt ST` |
| 8 | Set up hyperlinks. | Level 1 | `OSC 8 ; params ; URI ST` |
| 9 ; 2 | Show a text message. | Level 1 | `OSC 9 ; 2 ; Pt ST` |
| 9 ; 4 | Progress notification. | Level 1 | `OSC 9 ; 4 ; state ; value ST`|
| 9 ; 9 | Change working directory. (ConEmu version) | Level 1 | `OSC 9 ; 9 ; Pt ST` |
| 10 | Change ink color. | Level 1 | `OSC 10 ; spec ST` |
| 11 | Change paper color. | Level 1 | `OSC 11 ; spec ST` |
| 17 | Change selection ink color. | Level 1 | `OSC 17 ; spec ST` |
| 19 | Change selection paper color. | Level 1 | `OSC 19 ; spec ST` |
| 52 | Clipboard access and manipulation. | Level 1 | `OSC 52 ; Pc ; Pd ST` |
| 104 | Reset ANSI colors. | Level 1 | `OSC 104 ; c ST` |
| 110 | Reset ink color. | Level 1 | `OSC 110 ST` |
| 111 | Reset paper color. | Level 1 | `OSC 111 ST` |
| 119 | Reset selection paper color. | Level 1 | `OSC 119 ST` |
| 133 | Semantic information. | Level 1 | `OSC 133 ; Pt ST` |
| 444 | Display inline images. (Jexer) | Level 1 | `OSC 444 ; P1 ; P2 ; ... ST`|
| 1337 | Display inline images. (iTerm2) | Level 1 | `OSC 1337 ; args ST` |

#### Notes

* TerminalCtrl's responses to commands and report requests are not included in this table.

## [Supported Application Programming Commands](#apc-sequences)

| Opcode | Description | Device Level | Sequence |
| --- | --- | --- | --- |
| G | Display inline images. (Kitty) | Level 1 | `APC G params ; data ST` |

#### Notes

* TerminalCtrl's responses to commands and report requests are not included in this table.

## [Supported Graphics Rendition Opcodes](@sgr-sequences)

| Opcode | Description | Device Level |
| --- | --- | --- |
| 0 | Reset/Normal. | Level 1 |
| 1 | Bold | Level 1 |
| 2 | Faint. | Level 1 |
| 3 | Italic. | Level 1 |
| 4 | Underline. (Plain, Double, Curly) | Level 1 |
| 5 | Blink. | Level 1 |
| 7 | Inverse. | Level 1 |
| 8 | Hidden. | Level 1 |
| 9 | Striked out. | Level 1 |
| 21 | Double underline. (Legacy version.) | Level 1 |
| 22 | Neither bold nor faint. | Level 1 |
| 23 | Not Italic. | Level 1 |
| 24 | Not Underlined. | Level 1 |
| 25 | Steady. (Not blinking.) | Level 1 |
| 27 | Not Inversed. | Level 1 |
| 28 | Visible. (Not hidden.) | Level 1 |
| 29 | Not striked out. | Level 1 |
| 30 | Set ink color to black. | Level 1 |
| 31 | Set ink color to red. | Level 1 |
| 32 | Set ink color to green. | Level 1 |
| 33 | Set ink color to yellow. | Level 1 |
| 34 | Set ink color to blue. | Level 1 |
| 35 | Set ink color to magenta. | Level 1 |
| 36 | Set ink color to cyan. | Level 1 |
| 37 | Set ink color to white. | Level 1 |
| 38 | See Extended Color Sequences. | Level 1 |
| 39 | Default ink color. | Level 1 |
| 40 | Set paper color to black | Level 1 |
| 41 | Set paper color to red. | Level 1 |
| 42 | Set paper color to green. | Level 1 |
| 43 | Set paper color to yellow. | Level 1 |
| 44 | Set paper color to blue. | Level 1 |
| 45 | Set paper color to magenta. | Level 1 |
| 46 | Set paper color to cyan. | Level 1 |
| 47 | Set paper color to white. | Level 1 |
| 48 | See Extended Color Sequences. | Level 1 |
| 49 | Default paper color. | Level 1 |
| 53 | Overline | Level 1 |
| 55 | Not overlined. | Level 1 |
| 90 | Set ink color to gray. | Level 1 |
| 91 | Set ink color to light red. | Level 1 |
| 92 | Set ink color to ligt green. | Level 1 |
| 93 | Set ink color to light yellow. | Level 1 |
| 94 | Set ink color to light blue. | Level 1 |
| 95 | Set ink color to light magenta. | Level 1 |
| 96 | Set ink color to light cyan. | Level 1 |
| 97 | Set ink color to white. | Level 1 |
| 100 | Set paper color to gray | Level 1 |
| 101 | Set paper color to light red. | Level 1 |
| 102 | Set paper color to light green. | Level 1 |
| 103 | Set paper color to light yellow. | Level 1 |
| 104 | Set paper color to light blue. | Level 1 |
| 105 | Set paper color to light magenta. | Level 1 |
| 106 | Set paper color to light cyan. | Level 1 |
| 107 | Set paper color to white. | Level 1 |

## [Supported Extended Color Sequences](#sgr-extended-sequences)

### ISO/IEC 8613-6 Format

| Palette | CSI/SGR Sequence |
| --- | --- |
| RGB | `38 : 2 : [IGNORED] : R : G : B` |
| RGB | `48 : 2 : [IGNORED] : R : G : B` |
| RGB | `38 : 2 : R : G : B` |
| RGB | `48 : 2 : R : G : B` |
| CMY | `38 : 3 : [IGNORED] : C : M : Y` |
| CMY | `48 : 3 : [IGNORED] : C : M : Y` |
| CMY | `38 : 3 : C : M : Y` |
| CMY | `48 : 3 : C : M : Y` |
| CMYK | `38 : 4 : [IGNORED] : C : M : Y : K` |
| CMYK | `48 : 4 : [IGNORED] : C : M : Y : K` |
| CMYK | `38 : 4 : C : M : Y : K` |
| CMYK | `48 : 4 : C : M : Y : K` |
| Indexed | `38 : 5 : index` |
| Indexed | `48 : 5 : index` |

### ISO/IEC 8613-6 Mixed Format

| Palette | CSI/SGR Sequence |
| --- | --- |
| RGB | `38 ; 2 : [IGNORED] : R : G : B` |
| RGB | `48 ; 2 : [IGNORED] : R : G : B` |
| RGB | `38 ; 2 : R : G : B` |
| RGB | `48 ; 2 : R : G : B` |
| CMY | `38 ; 3 : [IGNORED] : C : M : Y` |
| CMY | `48 ; 3 : [IGNORED] : C : M : Y` |
| CMY | `38 ; 3 : C : M : Y` |
| CMY | `48 ; 3 : C : M : Y` |
| CMYK | `38 ; 4 : [IGNORED] : C : M : Y : K` |
| CMYK | `48 ; 4 : [IGNORED] : C : M : Y : K` |
| CMYK | `38 ; 4 : C : M : Y : K` |
| CMYK | `48 ; 4 : C : M : Y : K` |
| Indexed | `38 ; 5 : index` |
| Indexed | `48 ; 5 : index` |

### Legacy/Non-Standard Format

| Palette | CSI/SGR Sequence |
| --- | --- |
| RGB | `38 ; 2 ; R ; G ; B` |
| RGB | `48 ; 2 ; R ; G ; B` |
| CMY | `38 ; 3 ; C ; M ; Y` |
| CMY | `48 ; 3 ; C ; M ; Y` |
| CMYK | `38 ; 4 ; C ; M ; Y ; K` |
| CMYK | `48 ; 4 ; C ; M ; Y ; K` |
| Indexed | `38 ; 5 ; index` |
| Indexed | `48 ; 5 ; index` |

#### Notes

* 38 designates the pen.
* 48 designates the paper.
* Since TerminalCtrl is a true color virtual terminal emulator, there are no restrictions on its color palette. It can use a color palette ranging from 2 to 16 million colors.
* CMY and CMYK planes are projected onto RGB plane.
* TerminalCtrl does not keep a static palette for indexed color, or 256-color mode, if you will. It calculates the the 6x6x6 cube from the given index.
* Color space identifiers are ignored by TerminalCtrl.
* Transparent colors are not implemented (TODO).

## [Supported Color Text Specifications](#color-text-specs)

| Mnemonic | Format | Example | Device Level |
| --- | --- | --- | --- |
| RGB | `rgb : %04x / %04x / %04x` | `rgb:FFFF/A0A0/0000` | Level 1 |
| RGB | `rgb : %02x / %02x / %02x` | `rgb:FF/A0/00` | Level 1 |
| RGB | `%u , %u , %u` | `255,160,0` | Level 1 |
| RGBA | `rgba : %04x / %04x / %04x / %04x`| `rgba:FFFF/A0A0/0000/FFFF`| Level 1 |
| RGBA | `rgba : %02x / %02x / %02x / %02x`| `rgba:FF/A0/00/FF` | Level 1 |
| CMY | `cmy : %f / %f / %f` | `cmy:0.0/0.372549/1.0` | Level 1 |
| CMYK | `cmyk : %f / %f / %f / %f` | `cmyk:0.0/0.372549/1.0/0.0`| Level 1 |
| Hash3 | `# %01x %01x %01x` | `#FA0` | Level 1 |
| Hash6 | `# %02x %02x %02x` | `#FFAO00` | Level 1 |
| Hash9 | `# %03x %03x %03x` | `#FF0A00000` | Level 1 |
| Hash12 | `# %04x %04x %04x` | `#FFFFA0A00000` | Level 1 |

#### Notes

* The use of "hash hex color" text specifications 3, 9, and 12 are discouraged by X.org. They are supported by TerminalCtrl for backward compatibilty. RGB, RGBA, CMY, CMYK or Hash6 text specifications should be used wherever it is possible.
* The above listed color text specifications are utilized by xterm's dynamic colors feature.

## [Supported Extended Inline Image Sequences](#inline-image-protocols)

### Jexer Inline Images Protocol

| Image Format | OSC Sequnece | Description | Device Level |
| --- | --- | --- | --- |
| RGB bitmap | `444 ; 0 ; Pw ; Ph ; Ps ; data BEL` | Displays a raw RGB image at cursor. | Level 1 |
| RGB bitmap | `444 ; 0 ; Pw ; Ph ; Ps ; data ST` | Displays a raw RGB image at cursor. | Level 1 |
| PNG | `444 ; 1 ; Ps ; data BEL` | Displays a PNG image at cursor. | Level 1 |
| PNG | `444 ; 1 ; Ps ; data ST` | Displays a PNG image at cursor. | Level 1 |
| JPG | `444 ; 2 ; Ps ; data BEL` | Displays a JPG image at cursor. | Level 1 |
| JPG | `444 ; 2 ; Ps ; data ST` | Displays a JPG image at cursor. | Level 1 |

#### Notes

* `Pw` is the width in pixels. Valid range is 1 to 10000.
* `Ph` is the height in pixels. Valid range is 1 to 10000.
* `Ps` is the page scrolling option. Valid values are 1 (scroll) or 0 (no scroll).
* Image `data` must be Base64 encoded.
* The wire protocol reasonably separates the sequences for raw RGB, JPG and PNG image data. In practice, however, TerminalCtrl ignores this distinction, since it uses the StreamRaster interface (the raster image decoder factory) for decoding images. Hence, it can display any raster image via jexer's pre-defined sequences, if the format of the image in question is supported by the Upp::StreamRaster.
* Jexer inline images protocol is the *recommended* inline images protocol of choice for TerminalCtrl.

### iTerm2's Inline Images Protocol

| Image Format | OSC Sequnece | Description | Device Level |
| --- | --- | --- | --- |
| Any | `1337 ; File = inline = 1 ; [optional args] : data BEL`| Displays a raster image at cursor. | Level 1 |
| Any | `1337 ; File = inline = 1 ; [optional args] : data ST` | Displays a raster image at cursor. | Level 1 |

#### Notes

* iTerm2's inline images feature is a part of its file download and display protocol. TerminalCtrl currenty supports only the inline image display command of this protocol and some of its relevant arguments. These arguments should be in key=value pairs, delimited with semicolons.
* The image `data` must be base64 encoded.
* The `inline` argument is mandatory and its value must be 1.
* The `width` and `height` arguments are optional and are given as a number followed by a unit, or the word "auto".
* A value of `N` implies `N` character cells.
* A value of `Npx` implies `N` pixels (valid range is 1 to 10000).
* A value of `N%` implies `N` percent of the page width or height (valid range is 1 to 1000).
* A value of `auto` implies the image's original size will be used.
* The `preserveAspectRatio` argument is optional. If set to 0, then the image's inherent aspect ratio will not be respected; otherwise, it will fill the specified width and height as much as possible without stretching. Default value is 1.
* If the image doesn't fit into the vertical margins of the page and the sixel scrolling mode (**DECSDM**) is enabled, then the page will be scrolled at the margins. Otherwise the image will be cropped.

### Kitty's Inline Images Protocol

| Image Format | APC Sequence | Description | Device Level |
| --- | --- | --- | --- |
| Any | `G params ; data BEL` | Displays a raster image at cursor. | Level 1 |
| Any | `G params ; data ST` | Displays a raster image at cursor. | Level 1 |

#### Notes

* TerminalCtrl implements only a base subset of Kitty's graphics protocol.
* `G`: graphics payload indicator (mandatory).
* `params` are comma-separated key=value pairs.
* The parameter `f=FORMAT` can be `24` (raw RGB), `32` (raw RGBA), or `100` (raster formats such as PNG/JPG/TIFF).
* The parameter `s=WIDTH` sets image width in pixels (optional; null if missing).
* The parameter `v=HEIGHT` sets image height in pixels (optional; null if missing).
* The parameter `m=0|1` indicates if more chunks follow (`1`) or if it is the final chunk (`0`).
* The parameter `o=z` indicates the image is Z compressed.
* The parameter `a=q` is a protocol query that will return width, size, and format information.
* `data`: Base64-encoded image payload.
* TerminalCtrl *accumulates multi-chunk images* until `m=0`.
* Any image type supported by U++'s image decoding factory are supported via PNG mode.
* Width/height are optional; if missing, size is inferred from the decoded image.
* If the image doesn't fit into the vertical margins of the page and the sixel scrolling mode (**DECSDM**) is enabled, then the page will be scrolled at the margins. Otherwise the image will be cropped.

## [Supported Window Actions and Reports](#window-ops)

### Window Actions

| CSI Sequnece | Description | Device Level |
| --- | --- | --- |
| `1 t` | Unminimize window | Level 1 |
| `2 t` | Minimize window | Level 1 |
| `3 ; x ; y t` | Move window to [x, y] coordinates. | Level 1 |
| `4 ; height ; width t` | Resize window (in pixels). | Level 1 |
| `8 ; height ; width t` | Resize page (in cells). | Level 1 |
| `9 ; 0 t` | Unmaximize window. | Level 1 |
| `9 ; 1 t` | Maximize window. | Level 1 |
| `9 ; 2 t` | Maximize window vertically. | Level 1 |
| `9 ; 3 t` | Maximize window horizontally. | Level 1 |
| `10 ; 0 t` | Exit full screen mode. | Level 1 |
| `10 ; 1 t` | Enter screen mode. | Level 1 |
| `10 ; 2 t` | Toggle full screen mode. | Level 1 |

### Window Reports

| CSI Sequnece | Description | Device Level |
| --- | --- | --- |
| `11 t` | Report window state. | Level 1 |
| `13 t` | Report window position. | Level 1 |
| `13 ; 2 t` | Report page position (in pixels). | Level 1 |
| `14 t` | Report page size (in cells). | Level 1 |
| `14 ; 2 t` | Report window size (in pixels). | Level 1 |
| `15 t` | Report screen size (in pixels). | Level 1 |
| `16 t` | Report cell size (in pixels). | Level 1 |
| `18 t` | Report page size (in cells). | Level 1 |
| `19 t` | Report screen size (in cells) | Level 1 |
| `21 t` | Report window title. | Level 1 |

#### Notes

* These sequences are a part of xterm's window ops.

## [Other Supported Extensions](#other-extensions)

### Directory Change Protocol

| Sequence | Description | Device Level |
| --- | --- | --- |
| `OSC 7 ; PATH ST` | Notifies the client code that the working directory is changed. | Level 1 |
| `OSC 9 ; 9 ; PATH ST` | Notifies the client code that the working directory is changed. (ConEmu / Windows Terminal version) | Level 1 |

#### Notes

* TerminalCtrl *does not* validate the requested path. It is up to the client-code to check the path's validity.

### Hyperlinks Protocol

| Sequence | Description | Device Level |
| --- | --- | --- |
| `OSC 8 ; [parameters] ; URI ; ST text OSC 8 ; ; ST` | Displays a text with hyperlink at cursor. | Level 1 |

#### Notes

* `parameters` are optional `key=value` pairs.
* The original protocol defines only the the `id` parameter. However, *currently*, this parameter is also ignored by TerminalCtrl.
* The maximum `URI` length is 2083 characters.

### Clipboard Manipulation Protocol

| Access Type | Sequence | Description | Device Level |
| --- | --- | --- | --- |
| Write | `OSC 52 ; [clipboard] ; data ST` | Write data to the clipboard. | Level 1 |
| Read | `OSC 52 ; [clipboard] ; ? ST` | Read data from the clipboard. | Level 1 |
| Clear | `OSC 52 ; [clipboard] ; ST` | Clears selection/clipboard. | Level 1 |

#### Notes

* The `clipboard` parameter can be empty or `s0`. Currently, TerminalCtrl omits this parameter.
* When the `data` is a `?`, TerminalCtrl will reply to the host with the selection/clipboard data encoded using the same protocol.
* When the `data` is *neither a base64 string nor a `?`*, TerminalCtrl will clear the selection/clipboard.

### Progress Notification Protocol

| Sequence | Description | Device Level |
| --- | --- | --- |
| `OSC 9 ; 4 ; [state] ; [value] ST` | Notifies the client code about the progress status of an operation. | Level 1 |

#### Notes

* Implements ConEmu/Windows Terminal's progress tracking protocol.
* `state` can be one of `0`, `1`, `2`, `3`, or `4`.
* State `0`: Off state. This indicates that the any progress bar should be hidden. Use this state when the command is complete, to clear out any progress state.
* State `1`: Normal state. The `value` represents the completion percentage of the operation. It can be in between `0-100` (inclusive).
* State `2`: Error state. The `value` is optional and *can* represents an error code.
* State `3`: Busy state. The `value` should be ignored.
* State `4`: Warning state. The `value` is optional and *can* represents an warning number.

### Message Notification Protocol

| Sequence | Description | Device Level |
| --- | --- | --- |
| `OSC 9 ; 2 ; [text] ST` | Prompts a text message. | Level 1 |

#### Notes

* Implements ConEmu's message notification protocol.

### Background Image Change Protocol

| Sequence | Description | Device Level |
| --- | --- | --- |
| `OSC 1337 ; SetBackgroundImageFile=[path] ST` | Requests a background image change. | Level 1 |

#### Notes
* `SetBackgroundImageFile` argument specifies the background image change command.
* `path` should contain the path to an image file and must be base64 encoded.
* *Allowing arbitrary file paths may pose a security risk.* It is strongly recommended that client code prompts for user confirmation before applying any changes to ensure safety and prevent unauthorized modifications.

### Semantic Information Protocol

| Sequence | Description | Device Level |
| --- | --- | --- |
| `OSC 133 ; [command] ST` | Sets the semantic information, starting from the cursor position. | Level 1 |

#### Notes

* `command` can be one of the following four values (case sensitive):
* `A`: Marks the starting point of the shell prompt.
* `B`: Marks the end of the shell prompt and the start of the user input.
* `C`: Marks the end of the user input and the start of the command output.
* `D`: Marks the end of the command output.
* TerminalCtrl currently supports only a minimal—but reasonable—subset of this protocol. This may change in the future.
* TerminalCtrl does not process or display semantic information by itself. Instead, it is up to the client code to make use of the protocol, typically in combination with features like cell highlighting or search functionality.

