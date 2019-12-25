/*
 * (C) Copyright 2008 Semihalf
 *
 * (C) Copyright 2000-2006
 * Wolfgang Denk, DENX Software Engineering, wd@denx.de.
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#define DEBUG

#include <common.h>
#include <watchdog.h>
#include <command.h>
#include <image.h>
#include <malloc.h>
#include <zlib.h>
#include <bzlib.h>
#include <environment.h>
#include <asm/byteorder.h>

#if defined(CONFIG_OF_LIBFDT)
#include <fdt.h>
#include <libfdt.h>
#include <fdt_support.h>

static void fdt_error (const char *msg);
#endif

#ifdef CONFIG_LOGBUFFER
#include <logbuff.h>
#endif

#ifdef CFG_INIT_RAM_LOCK
#include <asm/cache.h>
#endif

DECLARE_GLOBAL_DATA_PTR;

extern int do_reset (cmd_tbl_t *cmdtp, int flag, int argc, char *argv[]);

#if defined(CONFIG_CMD_BDI)
extern int do_bdinfo(cmd_tbl_t *cmdtp, int flag, int argc, char *argv[]);
#endif

void  __attribute__((noinline))
do_bootm_linux(cmd_tbl_t *cmdtp, int flag,
		int	argc, char *argv[],
		image_header_t *hdr,
		int	verify)
{
	ulong	initrd_high;
	int	initrd_copy_to_ram = 1;
	ulong	initrd_start, initrd_end;
	ulong	rd_data_start, rd_data_end, rd_len;

	ulong	cmd_start, cmd_end;
	char    *cmdline;

	ulong	sp;
	char	*s;
	bd_t	*kbd;
	void	(*kernel)(bd_t *, ulong, ulong, ulong, ulong);

#if defined(CONFIG_OF_LIBFDT)
	image_header_t *fdt_hdr;
	char	*of_flat_tree = NULL;
	ulong	of_data = 0;
#endif

	if ((s = getenv ("initrd_high")) != NULL) {
		/* a value of "no" or a similar string will act like 0,
		 * turning the "load high" feature off. This is intentional.
		 */
		initrd_high = simple_strtoul(s, NULL, 16);
		if (initrd_high == ~0)
			initrd_copy_to_ram = 0;
	} else {	/* not set, no restrictions to load high */
		initrd_high = ~0;
	}

#ifdef CONFIG_LOGBUFFER
	kbd=gd->bd;
	/* Prevent initrd from overwriting logbuffer */
	if (initrd_high < (kbd->bi_memsize-LOGBUFF_LEN-LOGBUFF_OVERHEAD))
		initrd_high = kbd->bi_memsize-LOGBUFF_LEN-LOGBUFF_OVERHEAD;
	debug ("## Logbuffer at 0x%08lX ", kbd->bi_memsize-LOGBUFF_LEN);
#endif

	/*
	 * Booting a (Linux) kernel image
	 *
	 * Allocate space for command line and board info - the
	 * address should be as high as possible within the reach of
	 * the kernel (see CFG_BOOTMAPSZ settings), but in unused
	 * memory, which means far enough below the current stack
	 * pointer.
	 */

	asm( "mr %0,1": "=r"(sp) : );

	debug ("## Current stack ends at 0x%08lX ", sp);

	sp -= 2048;		/* just to be sure */
	if (sp > CFG_BOOTMAPSZ)
		sp = CFG_BOOTMAPSZ;
	sp &= ~0xF;

	debug ("=> set upper limit to 0x%08lX\n", sp);

	cmdline = (char *)((sp - CFG_BARGSIZE) & ~0xF);
	kbd = (bd_t *)(((ulong)cmdline - sizeof(bd_t)) & ~0xF);

	if ((s = getenv("bootargs")) == NULL)
		s = "";

	strcpy (cmdline, s);

	cmd_start    = (ulong)&cmdline[0];
	cmd_end      = cmd_start + strlen(cmdline);

	*kbd = *(gd->bd);

#ifdef	DEBUG
	printf ("## cmdline at 0x%08lX ... 0x%08lX\n", cmd_start, cmd_end);

#if defined(CONFIG_CMD_BDI)
	do_bdinfo (NULL, 0, 0, NULL);
#endif
#endif

	if ((s = getenv ("clocks_in_mhz")) != NULL) {
		/* convert all clock information to MHz */
		kbd->bi_intfreq /= 1000000L;
		kbd->bi_busfreq /= 1000000L;
#if defined(CONFIG_MPC8220)
	kbd->bi_inpfreq /= 1000000L;
	kbd->bi_pcifreq /= 1000000L;
	kbd->bi_pevfreq /= 1000000L;
	kbd->bi_flbfreq /= 1000000L;
	kbd->bi_vcofreq /= 1000000L;
#endif
#if defined(CONFIG_CPM2)
		kbd->bi_cpmfreq /= 1000000L;
		kbd->bi_brgfreq /= 1000000L;
		kbd->bi_sccfreq /= 1000000L;
		kbd->bi_vco     /= 1000000L;
#endif
#if defined(CONFIG_MPC5xxx)
		kbd->bi_ipbfreq /= 1000000L;
		kbd->bi_pcifreq /= 1000000L;
#endif /* CONFIG_MPC5xxx */
	}

	kernel = (void (*)(bd_t *, ulong, ulong, ulong, ulong))image_get_ep (hdr);

	get_ramdisk (cmdtp, flag, argc, argv, hdr, verify,
			IH_ARCH_PPC, &rd_data_start, &rd_data_end);
	rd_len = rd_data_end - rd_data_start;

#if defined(CONFIG_OF_LIBFDT)
	if(argc > 3) {
		of_flat_tree = (char *) simple_strtoul(argv[3], NULL, 16);
		fdt_hdr = (image_header_t *)of_flat_tree;

		if (fdt_check_header (of_flat_tree) == 0) {
#ifndef CFG_NO_FLASH
			if (addr2info((ulong)of_flat_tree) != NULL)
				of_data = (ulong)of_flat_tree;
#endif
		} else if (image_check_magic (fdt_hdr)) {
			ulong image_start, image_end;
			ulong load_start, load_end;

			printf ("## Flat Device Tree at %08lX\n", fdt_hdr);
			print_image_hdr (fdt_hdr);

			image_start = (ulong)fdt_hdr;
			image_end = image_get_image_end (fdt_hdr);

			load_start = image_get_load (fdt_hdr);
			load_end = load_start + image_get_data_size (fdt_hdr);

			if ((load_start < image_end) && (load_end > image_start)) {
				fdt_error ("fdt overwritten");
				do_reset (cmdtp, flag, argc, argv);
			}

			puts ("   Verifying Checksum ... ");
			if (!image_check_hcrc (fdt_hdr)) {
				fdt_error ("fdt header checksum invalid");
				do_reset (cmdtp, flag, argc, argv);
			}

			if (!image_check_dcrc (fdt_hdr)) {
				fdt_error ("fdt checksum invalid");
				do_reset (cmdtp, flag, argc, argv);
			}
			puts ("OK\n");

			if (!image_check_type (fdt_hdr, IH_TYPE_FLATDT)) {
				fdt_error ("uImage is not a fdt");
				do_reset (cmdtp, flag, argc, argv);
			}
			if (image_get_comp (fdt_hdr) != IH_COMP_NONE) {
				fdt_error ("uImage is compressed");
				do_reset (cmdtp, flag, argc, argv);
			}
			if (fdt_check_header (of_flat_tree + image_get_header_size ()) != 0) {
				fdt_error ("uImage data is not a fdt");
				do_reset (cmdtp, flag, argc, argv);
			}

			memmove ((void *)image_get_load (fdt_hdr),
				(void *)(of_flat_tree + image_get_header_size ()),
				image_get_data_size (fdt_hdr));

			of_flat_tree = (char *)image_get_load (fdt_hdr);
		} else {
			fdt_error ("Did not find a Flattened Device Tree");
			do_reset (cmdtp, flag, argc, argv);
		}
		printf ("   Booting using the fdt at 0x%x\n",
				of_flat_tree);
	} else if (image_check_type (hdr, IH_TYPE_MULTI)) {
		ulong fdt_data, fdt_len;

		image_multi_getimg (hdr, 2, &fdt_data, &fdt_len);
		if (fdt_len) {

			of_flat_tree = (char *)fdt_data;

#ifndef CFG_NO_FLASH
			/* move the blob if it is in flash (set of_data to !null) */
			if (addr2info ((ulong)of_flat_tree) != NULL)
				of_data = (ulong)of_flat_tree;
#endif

			if (fdt_check_header (of_flat_tree) != 0) {
				fdt_error ("image is not a fdt");
				do_reset (cmdtp, flag, argc, argv);
			}

			if (be32_to_cpu (fdt_totalsize (of_flat_tree)) != fdt_len) {
				fdt_error ("fdt size != image size");
				do_reset (cmdtp, flag, argc, argv);
			}
		}
	}
#endif

	if (rd_data_start) {
	    if (!initrd_copy_to_ram) {	/* zero-copy ramdisk support */
		initrd_start = rd_data_start;
		initrd_end = rd_data_end;
	    } else {
		initrd_start  = (ulong)kbd - rd_len;
		initrd_start &= ~(4096 - 1);	/* align on page */

		if (initrd_high) {
			ulong nsp;

			/*
			 * the inital ramdisk does not need to be within
			 * CFG_BOOTMAPSZ as it is not accessed until after
			 * the mm system is initialised.
			 *
			 * do the stack bottom calculation again and see if
			 * the initrd will fit just below the monitor stack
			 * bottom without overwriting the area allocated
			 * above for command line args and board info.
			 */
			asm( "mr %0,1": "=r"(nsp) : );
			nsp -= 2048;		/* just to be sure */
			nsp &= ~0xF;
			if (nsp > initrd_high)	/* limit as specified */
				nsp = initrd_high;
			nsp -= rd_len;
			nsp &= ~(4096 - 1);	/* align on page */
			if (nsp >= sp)
				initrd_start = nsp;
		}

		show_boot_progress (12);

		debug ("## initrd at 0x%08lX ... 0x%08lX (len=%ld=0x%lX)\n",
			rd_data_start, rd_data_end - 1, rd_len, rd_len);

		initrd_end    = initrd_start + rd_len;
		printf ("   Loading Ramdisk to %08lx, end %08lx ... ",
			initrd_start, initrd_end);

		memmove_wd((void *)initrd_start,
			   (void *)rd_data_start, rd_len, CHUNKSZ);

		puts ("OK\n");
	    }
	} else {
		initrd_start = 0;
		initrd_end = 0;
	}

#if defined(CONFIG_OF_LIBFDT)

#ifdef CFG_BOOTMAPSZ
	/*
	 * The blob must be within CFG_BOOTMAPSZ,
	 * so we flag it to be copied if it is not.
	 */
	if (of_flat_tree >= (char *)CFG_BOOTMAPSZ)
		of_data = (ulong)of_flat_tree;
#endif

	/* move of_flat_tree if needed */
	if (of_data) {
		int err;
		ulong of_start, of_len;

		of_len = be32_to_cpu(fdt_totalsize(of_data));

		/* position on a 4K boundary before the kbd */
		of_start  = (ulong)kbd - of_len;
		of_start &= ~(4096 - 1);	/* align on page */
		debug ("## device tree at 0x%08lX ... 0x%08lX (len=%ld=0x%lX)\n",
			of_data, of_data + of_len - 1, of_len, of_len);

		of_flat_tree = (char *)of_start;
		printf ("   Loading Device Tree to %08lx, end %08lx ... ",
			of_start, of_start + of_len - 1);
		err = fdt_open_into((void *)of_data, (void *)of_start, of_len);
		if (err != 0) {
			fdt_error ("fdt move failed");
			do_reset (cmdtp, flag, argc, argv);
		}
		puts ("OK\n");
	}
	/*
	 * Add the chosen node if it doesn't exist, add the env and bd_t
	 * if the user wants it (the logic is in the subroutines).
	 */
	if (of_flat_tree) {
		if (fdt_chosen(of_flat_tree, initrd_start, initrd_end, 0) < 0) {
			fdt_error ("/chosen node create failed");
			do_reset (cmdtp, flag, argc, argv);
		}
#ifdef CONFIG_OF_HAS_UBOOT_ENV
		if (fdt_env(of_flat_tree) < 0) {
			fdt_error ("/u-boot-env node create failed");
			do_reset (cmdtp, flag, argc, argv);
		}
#endif
#ifdef CONFIG_OF_HAS_BD_T
		if (fdt_bd_t(of_flat_tree) < 0) {
			fdt_error ("/bd_t node create failed");
			do_reset (cmdtp, flag, argc, argv);
		}
#endif
#ifdef CONFIG_OF_BOARD_SETUP
		/* Call the board-specific fixup routine */
		ft_board_setup(of_flat_tree, gd->bd);
#endif
	}
#endif	/* CONFIG_OF_LIBFDT */

	debug ("## Transferring control to Linux (at address %08lx) ...\n",
		(ulong)kernel);

	show_boot_progress (15);

#if defined(CFG_INIT_RAM_LOCK) && !defined(CONFIG_E500)
	unlock_ram_in_cache();
#endif

#if defined(CONFIG_OF_LIBFDT)
	if (of_flat_tree) {	/* device tree; boot new style */
		/*
		 * Linux Kernel Parameters (passing device tree):
		 *   r3: pointer to the fdt, followed by the board info data
		 *   r4: physical pointer to the kernel itself
		 *   r5: NULL
		 *   r6: NULL
		 *   r7: NULL
		 */
		(*kernel) ((bd_t *)of_flat_tree, (ulong)kernel, 0, 0, 0);
		/* does not return */
	}
#endif
	/*
	 * Linux Kernel Parameters (passing board info data):
	 *   r3: ptr to board info data
	 *   r4: initrd_start or 0 if no initrd
	 *   r5: initrd_end - unused if r4 is 0
	 *   r6: Start of command line string
	 *   r7: End   of command line string
	 */
	(*kernel) (kbd, initrd_start, initrd_end, cmd_start, cmd_end);
	/* does not return */
}

#if defined(CONFIG_OF_LIBFDT)
static void fdt_error (const char *msg)
{
	puts ("ERROR: ");
	puts (msg);
	puts (" - must RESET the board to recover.\n");
}
#endif
