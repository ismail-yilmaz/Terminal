topic "AnsiParser";
[i448;a25;kKO9;2 $$1,0#37138531426314131252341829483380:class]
[l288;2 $$2,2#27521748481378242620020725143825:desc]
[0 $$3,0#96390100711032703541132217272105:end]
[H6;0 $$4,0#05600065144404261032431302351956:begin]
[i448;a25;kKO9;2 $$5,0#37138531426314131252341829483370:item]
[l288;a4;*@5;1 $$6,6#70004532496200323422659154056402:requirement]
[l288;i1121;b17;O9;~~~.1408;2 $$7,0#10431211400427159095818037425705:param]
[i448;b42;O9;2 $$8,8#61672508125594000341940100500538:tparam]
[b42;2 $$9,9#13035079074754324216151401829390:normal]
[b42;a42;2 $$10,10#45413000475342174754091244180557:text]
[2 $$0,0#00000000000000000000000000000000:Default]
[{_}%EN-US 
[ {{10000@(113.42.0) [s0; [*@7;4 AnsiParser]]}}&]
[s0;%- &]
[s1;:Upp`:`:AnsiParser`:`:class:%- [@(0.0.255)3 class][3 _][*3 AnsiParser]&]
[s2; This class encapsulates a reliable and high`-performance VT500 
series “lexical” parser designed specifically for handling 
DEC `& ANSI escape sequences. It is implemented as a finite state 
machine based on [^https`:`/`/vt100`.net`/emu`/dec`_ansi`_parser^ the 
UML state diagram publicized by Paul`-Flo Williams]. &]
[s2; &]
[s2; AnsiParser can handle UTF`-8 and non UTF`-8 characters, C0 and 
C1 control bytes, and ESC, CSI, DCS, OSC, APC, SOS, and PM sequences 
in both 7`-bits and 8`-bits forms, allows switching between UTF`-8 
and non UTF`-8 modes on`-the`-fly, and can be used as both a 
parser and a filter.&]
[s2; &]
[s0;l288; Note that there are two deviations from the original scheme 
in order to accommodate modern usage patterns:&]
[s0;l288; &]
[s2;l544;i150;O9;~~~512~576; 1) ISO 8613`-6: Byte 0x3a (`"colon`", 
`':`') is considered as a legitimate delimiter.&]
[s2;l544;i150;O9;~~~512~576; 2) The OSC sequences allow UTF`-8 payload 
if the UTF`-8 mode is enabled.&]
[s3;%- &]
[ {{10000F(128)G(128)@1 [s0; [* Public Method List]]}}&]
[s3;%- &]
[s5;:Upp`:`:AnsiParser`:`:Parse`(const Upp`:`:String`&`,bool`):%- [@(0.0.255) void]_[* Pa
rse]([@(0.0.255) const]_[_^Upp`:`:String^ String][@(0.0.255) `&]_[*@3 data], 
[@(0.0.255) bool]_[*@3 utf8])&]
[s5;:Upp`:`:AnsiParser`:`:Parse`(const void`*`,int`,bool`):%- [@(0.0.255) void]_[* Parse](
[@(0.0.255) const]_[@(0.0.255) void]_`*[*@3 data], [@(0.0.255) int]_[*@3 size], 
[@(0.0.255) bool]_[*@3 utf8])&]
[s2; Parses a [%-*@3 data] string, or a [%-*@3 data].buffer with given 
[%-*@3 size]. The parser will switch to UTF`-8 mode when the [%-*@3 utf8] 
flag is enabled and this flag can be enabled or disabled run`-time. 
Important note: AnsiParser has no explicit error state. It will 
ignore the remaining bytes of the erroneous sequence if it encounters 
any. Still, it is possible to reset the parser to its initial 
state at any moment, using the [^topic`:`/`/Terminal`/src`/Upp`_AnsiParser`_en`-us`#Upp`:`:AnsiParser`:`:Reset`(`)^ R
eset()] method.&]
[s3; &]
[s4;%- &]
[s5;:Upp`:`:AnsiParser`:`:Reset`(`):%- [@(0.0.255) void]_[* Reset]()&]
[s2; Resets the parser to its initial state.&]
[s3;%- &]
[s4;%- &]
[s5;:Upp`:`:AnsiParser`:`:WasChr`(`)const:%- [@(0.0.255) bool]_[* WasChr]()_[@(0.0.255) con
st]&]
[s2; Returns true if the last dispatched item was a character or 
a chunk of characters and not a sequence or control bytes.&]
[s3;%- &]
[s4; &]
[s5;:Upp`:`:AnsiParser`:`:ParametrizePayload`(bool`):%- AnsiParser[@(0.0.255) `&] 
[* ParametrizePayload]([@(0.0.255) bool] [*@3 b] [@(0.0.255) `=] [@(0.0.255) true])&]
[s2; Enables or disables automatic splitting and parametrizing of 
the OSC, APC, SOS and PM payloads into [^topic`:`/`/Terminal`/src`/Upp`_VTInStream`_en`-us`#Upp`:`:AnsiParser`:`:Sequence`:`:parameters^ S
equence`::parameters] vector, according to semicolon delimiter 
rule. Returns `*this for method chaining. Disabled by default.&]
[s3; &]
[s4;%- &]
[s5;:Upp`:`:AnsiParser`:`:DontParametrizePayload`(`):%- AnsiParser[@(0.0.255) `&] 
[* DontParametrizePayload]()&]
[s2; Disables automatic splitting and parametrizing of the OSC, APC, 
SOS and PM payloads. Return `*this for method chaining.&]
[s3;%- &]
[s4; &]
[s5;:Upp`:`:AnsiParser`:`:WhenChr:%- [_^Upp`:`:Event^ Event]<const 
[@(0.0.255) int`*, const] byte [@(0.0.255) `*], [@(0.0.255) int]>_[* WhenChr]&]
[s2; This event serves as the unified data sink for the parser. It 
delivers character data exclusively via either the unicode codepoint 
array (first parameter) or the ASCII byte buffer (second parameter). 
The count (last parameter) specifies the number of available 
elements. This is for performance reasons. Clients should check 
which buffer is valid to select the appropriate processing path. 
This dual channel approach allows optimized handling of huge 
ASCII chunks.&]
[s3;%- &]
[s4;%- &]
[s5;:Upp`:`:AnsiParser`:`:WhenCtl:%- [_^Upp`:`:Event^ Event]<[_^Upp`:`:byte^ byte]>_[* When
Ctl]&]
[s2; This event is dispatched when a control byte (either in 0x00`-0x7F 
range or in 0x80`-0x9F range) is received. &]
[s3;%- &]
[s4;%- &]
[s5;:Upp`:`:AnsiParser`:`:WhenEsc:%- [_^Upp`:`:Event^ Event]<[@(0.0.255) const]_AnsiParse
r`::Sequence[@(0.0.255) `&]>_[* WhenEsc]&]
[s2; This event is dispatched when an escape sequence is received.&]
[s3;%- &]
[s4;%- &]
[s5;:Upp`:`:AnsiParser`:`:WhenCsi:%- [_^Upp`:`:Event^ Event]<[@(0.0.255) const]_AnsiParse
r`::Sequence[@(0.0.255) `&]>_[* WhenCsi]&]
[s2; This event is dispatched when a control function is received.&]
[s3;%- &]
[s4;%- &]
[s5;:Upp`:`:AnsiParser`:`:WhenDcs:%- [_^Upp`:`:Event^ Event]<[@(0.0.255) const]_AnsiParse
r`::Sequence[@(0.0.255) `&]>_[* WhenDcs]&]
[s2; This event is dispatched when a device control string is received.&]
[s3;%- &]
[s4;%- &]
[s5;:Upp`:`:AnsiParser`:`:WhenOsc:%- [_^Upp`:`:Event^ Event]<[@(0.0.255) const]_AnsiParse
r`::Sequence[@(0.0.255) `&]>_[* WhenOsc]&]
[s2; This event is dispatched when an operating system command is 
received.&]
[s3; &]
[s4;%- &]
[s5;:Upp`:`:AnsiParser`:`:WhenApc:%- [_^Upp`:`:Event^ Event]<[@(0.0.255) const]_AnsiParse
r`::Sequence[@(0.0.255) `&]>_[* WhenApc]&]
[s2; This event is dispatched when an application programming command 
is received.&]
[s3;%- &]
[s4;%- &]
[s5;:Upp`:`:AnsiParser`:`:WhenSos:%- Event<[@(0.0.255) const] Sequence[@(0.0.255) `&]> 
[* WhenSos]&]
[s2; This event is dispatched when a start of sequence command is 
received.&]
[s3;%- &]
[s4;%- &]
[s5;:Upp`:`:AnsiParser`:`:WhenPm:%- Event<[@(0.0.255) const] Sequence[@(0.0.255) `&]> 
[* WhenPm]&]
[s2; This event is dispatched when an application privacy message 
is received.&]
[s3;%- &]
[s3;%- &]
[ {{10000F(128)G(128)@1 [s0; [* Constructor detail]]}}&]
[s3;%- &]
[s5;:Upp`:`:AnsiParser`:`:AnsiParser`(`):%- [* AnsiParser]()&]
[s2; Default constructor.&]
[s3;%- &]
[s0;%- &]
[ {{10000@(113.42.0) [s0; [*@7;4 AnsiParser`::Sequence]]}}&]
[s0; &]
[s1;:Upp`:`:AnsiParser`:`:Sequence`:`:struct:%- [@(0.0.255)3 struct][3 _][*3 Sequence]&]
[s2; This structure represents a ESC, CSI, DCS, OSC, APC, SOS, or 
PM sequence, depending on the AnsiParser context.&]
[s3;%- &]
[s4;%- &]
[s5;:Upp`:`:AnsiParser`:`:Sequence`:`:type:%- [_ enum class Type]_[* type]&]
[s2; Represents the type of the sequence. Currently the valid values 
are ESC, CSI, DCS, OSC, APC, SOS, or PM. Underlying type of this 
enum is [@(0.128.128) byte].&]
[s3;%- &]
[s4;%- &]
[s5;:Upp`:`:AnsiParser`:`:Sequence`:`:opcode:%- [%%@(0.128.128) byte]_[* opcode]&]
[s2; Also known as the `"final`" or `"terminator`" byte. Usually 
represents the opcode of received function.&]
[s3;%- &]
[s4;%- &]
[s5;:Upp`:`:AnsiParser`:`:Sequence`:`:mode:%- [%%@(0.128.128) byte]_[* mode]&]
[s2; Contains the context`-dependent mode information of the given 
sequence. E.g. for private (DEC/xterm) functions or modes this 
is usually, but not always, a 0x3F (question mark).&]
[s3;%- &]
[s4;%- &]
[s5;:Upp`:`:AnsiParser`:`:Sequence`:`:intermediate:%- [%%@(0.128.128) byte]_[* intermedia
te][@(0.0.255) `[][@3 4][@(0.0.255) `]]&]
[s2; Contains the collected intermediate bytes specific to the given 
sequence. Allows up to 4 intermediary bytes. (Historically, max 
2 bytes are used.)&]
[s3;%- &]
[s4;%- &]
[s5;:Upp`:`:AnsiParser`:`:Sequence`:`:parameters:%- [_^Upp`:`:Vector^ Vector]<[_^Upp`:`:String^ S
tring]>_[* parameters]&]
[s2; Contains the function parameters specific to CSI or DCS sequences, 
in a vectorized form. Note that this vector can contain empty 
strings that usually mean an implicit default value, depending 
on the sequence`'s context.&]
[s2; &]
[s2; Note that [^topic`:`/`/Terminal`/src`/Upp`_VTInStream`_en`-us`#Upp`:`:AnsiParser`:`:class^ A
nsiParser] is able to [^topic`:`/`/Terminal`/src`/Upp`_VTInStream`_en`-us`#Upp`:`:AnsiParser`:`:ParametrizePayload`(bool`)^ a
utomatically parametrize] the payload for OSC, APC, SOS and PM 
sequences, but this feature is disabled by default.&]
[s3; &]
[s4; &]
[s5;:Upp`:`:AnsiParser`:`:Sequence`:`:payload:%- [_^Upp`:`:String^ String]_[* payload]&]
[s2; Contains any additional data to be parsed by client code. For 
example, OSC, APC, SOS, PM and the string component of the device 
control strings are handed to client code via this member variable.&]
[s2; &]
[s2; Note that [^topic`:`/`/Terminal`/src`/Upp`_VTInStream`_en`-us`#Upp`:`:AnsiParser`:`:class^ A
nsiParser] is able to [^topic`:`/`/Terminal`/src`/Upp`_VTInStream`_en`-us`#Upp`:`:AnsiParser`:`:ParametrizePayload`(bool`)^ a
utomatically parametrize] the payload for OSC, APC, SOS and PM 
sequences, but this feature is disabled by default.&]
[s3;%- &]
[s4;%- &]
[s5;:Upp`:`:AnsiParser`:`:Sequence`:`:GetInt`(int`,int`)const:%- [@(0.0.255) int]_[* GetI
nt]([@(0.0.255) int]_[*@3 n], [@(0.0.255) int]_[*@3 d]_`=_[@3 1])_[@(0.0.255) const]&]
[s2; A convenience method for parsing integer parameters. [%-*@3 n] 
is the index of parameter. The index is 1`-based. Namely, the 
index of the first parameter would be 1, and the index of the 
fifth parmeter would be 5, etc. A default value to be used instead 
of an erroneous, out`-of`-bounds, or empty (defaulted) sequence 
parameter can be provided with [%-*@3 d].&]
[s3; &]
[s4;%- &]
[s5;:Upp`:`:AnsiParser`:`:Sequence`:`:GetStr`(int`)const:%- [_^Upp`:`:String^ String]_[* G
etStr]([@(0.0.255) int]_[*@3 n])_[@(0.0.255) const]&]
[s2; The same as above but for string parameters. [%-*@3 n ] is the 
index of the sequence parameter. Returns a void string for erroneous, 
out`-of`-bounds, or empty (defaulted) sequence parameters.&]
[s3; &]
[s4;%- &]
[s5;:Upp`:`:AnsiParser`:`:Sequence`:`:ToString`(`)const:%- [_^Upp`:`:String^ String]_[* T
oString]()_[@(0.0.255) const]&]
[s2; Returns the sequence as a String. Useful for diagnostics and 
logging.&]
[s3;%- &]
[s4;%- &]
[s5;:Upp`:`:AnsiParser`:`:Sequence`:`:Clear`(`):%- [@(0.0.255) void]_[* Clear]()&]
[s0; -|Clears the structure.]]