# AnsiParser

A very fast, reliable parser for DEC and ANSI terminal escape sequences. `AnsiParser` is a VT500-series state machine, based on Paul Williams' [well-known parser diagram](https://vt100.net/emu/dec_ansi_parser). It handles `ESC`, `CSI`, `DCS`, `OSC`, `APC`, `SOS`, and `PM` sequences in both 7-bit and 8-bit form, UTF-8 and raw 8-bit text (switchable at runtime), and C0/C1 control bytes anywhere in the stream. It can run as a dispatcher or as a transparent filter that reproduces its input byte-for-byte.

## Features

- Full sequence coverage: `ESC`, `CSI`, `DCS`, `OSC`, `APC`, `SOS`, `PM`, in both 7-bit and 8-bit forms.
- UTF-8 aware, switchable on the fly.
- Usable as a parser or a filter.
- Separate dispatch hook per sequence family.
- SIMD-accelerated (SSE2 / NEON) where available, scalar fallback otherwise.
  Throughput only (results are identical either way).

`APC`, `SOS`, and `PM` have no standard-defined payload; ECMA-48 reserves them but leaves the contents undefined, and most terminals discard them. AnsiParser still parses and terminates all three correctly, each with its own dispatch hook, so a host app can give one a private meaning without touching the others.

## Deviations from the Reference Scheme

- **`0x3a` ('`:`') is a valid parameter delimiter**, per ISO 8613-6 sub-parameters (e.g. `CSI 38:2:r:g:b m`). The original scheme rejects it.
- **`OSC` payloads may carry raw UTF-8** when UTF-8 mode is on, since `OSC`  carries real text (titles, hyperlinks, clipboard data). `APC`, `SOS`, and `PM` stay 7-bit-safe: A stray high byte ends the sequence instead.

## Supported Sequence Families

| Family | 7-bit | 8-bit | Terminator | Payload |
|---|---|---|---|---|
| `ESC` | `ESC` | — | (sequence-defined) | Single dispatched action |
| `CSI` | `ESC [` | `0x9b` | final byte `0x40–0x7e` | Parameters + intermediates |
| `DCS` | `ESC P` | `0x90` | `ST` | Parameters + passthrough |
| `OSC` | `ESC ]` | `0x9d` | `ST` / `BEL` | Text (UTF-8 if enabled) |
| `APC` | `ESC _` | `0x9f` | `ST` / `BEL` | App-defined, 7-bit-safe |
| `SOS` | `ESC X` | `0x98` | `ST` / `BEL` | Undefined / private use |
| `PM`  | `ESC ^` | `0x9e` | `ST` / `BEL` | Undefined / private use |

``ST`` = ``ESC \`` (7-bit) or `0x9c` (8-bit).

## License

```
Copyright (c) 2019-2026, İsmail Yılmaz
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

* Redistributions of source code must retain the above copyright notice, this
  list of conditions and the following disclaimer.

* Redistributions in binary form must reproduce the above copyright notice,
  this list of conditions and the following disclaimer in the documentation
  and/or other materials provided with the distribution.

* Neither the name of the copyright holder nor the names of its
  contributors may be used to endorse or promote products derived from
  this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
```
