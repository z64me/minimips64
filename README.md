# minimips64
This is my MIPS R4300i assembler and disassembler. I designed it to be super minimalistic and compact. `valgrind` reports no errors, and compiling with GCC's `-Wall -Wextra` produces no warnings.

![kokiri-tunic.asm](screenshot.jpg)

## Disassembling
```
minimips64 --disassemble ... "input.bin"
  ... refers to the following optional arguments:
    --address 0x1234    address within binary file to begin
    --number 10         number of opcodes to disassemble
    --jrra              stop parsing when jr $ra reached
    --prefix            prefix each line with address
    --hex               prefix each line with instruction hex
    --start             locate the start of the function
                        before disassembling it
    --interactive       do not load binary file; instead,
                        disassemble bytes typed by user
```

## Assembling
```
minimips64 --assemble ... "input.asm"
  ... refers to the following optional arguments:
    --Dname=0x5678       for defining symbols
    --address 0x1234     set base address 
    --cloud "out.txt"    write cloudpatch (overwrites if exists)
    --rom "rom.z64"      update rom (file must exist)
    --interactive        do not load file; instead, assemble
                         instructions typed by user
```

## Directives
In addition to assembling MIPS, there are a few directives you can specify throughout your source file. They're very simple, so here's a quick rundown:
```
define symbolName 0xAddress
  expose a symbol to the assembler so you can use names
  instead of magic values

seek 0xAddress
  seek an address within the rom

incbin "relative/path/to/file.bin"
  write data from binary file into the rom

import "relative/path/to/symbol/map.txt"
  the symbol map you import should be formatted like this:
    WowSymbolMagic    0x800014E4
    AndHereIsAnother  0x80001584
  (one per line, separated by whitespace)
```
Prefix directives with the `>` token. For example, `> import "dmaext.txt"`. I have included an example of a working file below.

## Example
```c
/* kokiri-tunic.asm
 * a simple assembly hack for OoT MQ debug using minimips64
 * this one gradually changes the color of the Kokiri Tunic
 */

/* define some addresses for later use */
> define kRed     0x80126008 /* Kokiri Tunic RGB color channels */
> define kGreen   0x80126009
> define kBlue    0x8012600A
> define GameTime 0x80223E04 /* number of gameplay frames elapsed */

/* the hack overwrites an unused skelanime function
 * https://github.com/zeldaret/oot/blob/master/src/code/z_skelanime.c
 */
> seek 0xB1C630
> define SkelAnime_CopyFrameTableFalse 0x800A5490
	lui     v0, %hi(GameTime)
	lw      v0, %lo(GameTime)(v0)   // v0 = time elapsed
	sll     t0, v0, 2               // t0 = time * 4
	sll     t1, v0, 1               // t1 = time * 2
	lui     v1, %hi(kRed)
	sb      v0, %lo(kRed)(v1)       // red = time
	sb      t0, %lo(kGreen)(v1)     // green = time * 4
	sb      t1, %lo(kBlue)(v1)      // blue = time * 2
	jr      ra
	nop

/* we will use this hook */
> seek 0xB3B8A8
	jal     SkelAnime_CopyFrameTableFalse
	nop

```

