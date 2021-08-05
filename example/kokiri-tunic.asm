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

