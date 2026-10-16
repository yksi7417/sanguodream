# Unicode reference data

This folder holds public reference data from the Unicode Consortium. The
text module uses it to decode the Big5 names of the content pack.

| File | Source | Licence |
|---|---|---|
| BIG5.TXT | https://www.unicode.org/Public/MAPPINGS/OBSOLETE/EASTASIA/OTHER/BIG5.TXT (table version 2.0, header date 2015-12-02) | Unicode License v3, see LICENSE-UNICODE.txt |
| LICENSE-UNICODE.txt | https://www.unicode.org/license.txt | the licence text itself |

## Licence note
The Unicode License v3 lets you copy, change and distribute the data files.
The copyright and permission notice must stay with each copy. This folder
keeps the notice in LICENSE-UNICODE.txt, and BIG5.TXT keeps its own header.
Do not edit BIG5.TXT. If the table must change, change the generator.

## Use
misc/make_big5_table.py reads BIG5.TXT and writes code/text/big5_table.h.
Tutorial 12 (docs/tutorials/12-text-and-locales.md) gives the procedure.
The game does not read this folder at run time.
