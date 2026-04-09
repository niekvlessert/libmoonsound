In this directory are a bunch of other directories:

- libvgm, a library than contains emulation for among others ymf278b, OPL4 also called
- roboplay, a music player for msx that can play mwm (with mbk) and mfm files
- openmsx, an emulator for the msx computer, with an emulated opl4

Also yrw801.rom, the samples for the opl4 emulator.

And LICKIT.MWM and a MBK file with it.

I want a program in c that can convert mfm and mwm files to wav audio.

Below, at 'tracker view' is the pattern with the notes of the first page of the tracker output. So the notes and timing for W01 (bass sound), W02 (same sound), W03 (don't know?), W04 (a sample) and W23 (percussion).

Start by being able to get this data correctly out of the MWM file, so the parsing works well. Look at the roboplay implementation for that, but be careful, it's MSX, som maybe endian stuff and other problems.

After that implement the wav generation using libvgm emulator and if needed openmsx source.

Compile using cmake.

tracker view:

=== MWM Tracker Dump (First Page, Pos 0 Pattern 0) ===
     | W01  W02  W03  W04  W05  W06  W07  W08  W09  W10  W11  W12  W13  W14  W15  W16  W17  W18  W19  W20  W21  W22  W23  W24 | CMD
-----|--------------------------------------------------------------------- ------------------------------------------------------------------------------------------
00   | C2   C2   G4   ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  C#5  --- | 13
01   | ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  --- | 00
02   | C3   C3   ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  --- | 00
03   | ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  --- | 00
04   | C2   C2   ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  --- | 00
05   | ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  --- | 00
06   | C3   C3   ---  G4   ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  --- | 00
07   | ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  --- | 00
08   | C2   C2   ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  --- | 00
09   | ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  --- | 00
0A   | C3   C3   ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  --- | 00
0B   | ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  --- | 00
0C   | C2   C2   ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  --- | 00
0D   | ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  ---  --- | 00 
