/*
 * (C) Copyright 2003
 * Wolfgang Denk, DENX Software Engineering, wd@denx.de.
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <common.h>
#include <command.h>
#include <image.h>
#include <zlib.h>
#include <asm/byteorder.h>
#include <asm/addrspace.h>

/* cu570m start */
#ifdef CONFIG_AR7240
#include <ar7240_soc.h>
#endif
/* cu570m end */

DECLARE_GLOBAL_DATA_PTR;

#define	LINUX_MAX_ENVS		256
#define	LINUX_MAX_ARGS		256

static int	linux_argc;
static char **	linux_argv;

static char **	linux_env;
static char *	linux_env_p;
static int	linux_env_idx;

static void linux_params_init (ulong start, char * commandline);
static void linux_env_set (char * env_name, char * env_val);

/* cu570m start */
#ifdef CONFIG_WASP_SUPPORT
void wasp_set_cca(void)
{
	/* set cache coherency attribute */
	asm(	"mfc0	$t0,	$16\n"		/* CP0_CONFIG == 16 */
		"li	$t1,	~7\n"
		"and	$t0,	$t0,	$t1\n"
		"ori	$t0,	3\n"		/* CONF_CM_CACHABLE_NONCOHERENT */
		"mtc0	$t0,	$16\n"		/* CP0_CONFIG == 16 */
		"nop\n": : );
}
#endif
/* cu570m end */

void do_bootm_linux (cmd_tbl_t * cmdtp, int flag, int argc, char *argv[],
		     image_header_t *hdr, int verify)
{
	ulong initrd_start, initrd_end;

/* cu570m start */
	ulong mem_size_to_kernel;
#if defined(CONFIG_AR7100) || defined(CONFIG_AR7240) || defined(CONFIG_ATHEROS)
	int flash_size_mbytes;
	void (*theKernel) (int, char **, char **, int);
#else /* old default branch */
	void (*theKernel) (int, char **, char **, int *);
#endif /* cu570m */
	char *commandline = getenv ("bootargs");
	char env_buf[12];
/* cu570m condition branch */
#if defined(CONFIG_AR7100) || defined(CONFIG_AR7240) || defined(CONFIG_ATHEROS)
	theKernel =
		(void (*)(int, char **, char **, int)) image_get_ep (hdr);
#else /* old default branch */
	theKernel =
		(void (*)(int, char **, char **, int *)) image_get_ep (hdr);
#endif /* cu570m */

	get_ramdisk (cmdtp, flag, argc, argv, hdr, verify,
			IH_ARCH_MIPS, &initrd_start, &initrd_end);

	show_boot_progress (15);

#ifdef DEBUG
	printf ("## Transferring control to Linux (at address %08lx) ...\n",
		(ulong) theKernel);
#endif

	linux_params_init (UNCACHED_SDRAM (gd->bd->bi_boot_params), commandline);

#ifdef CONFIG_MEMSIZE_IN_BYTES
	sprintf (env_buf, "%lu", gd->ram_size);
#ifdef DEBUG
	printf ("## Giving linux memsize in bytes, %lu\n", gd->ram_size);
#endif
#else
	sprintf (env_buf, "%lu", gd->ram_size >> 20);
#ifdef DEBUG
	printf ("## Giving linux memsize in MB, %lu\n", gd->ram_size >> 20);
#endif
#endif /* CONFIG_MEMSIZE_IN_BYTES */

	linux_env_set ("memsize", env_buf);

	sprintf (env_buf, "0x%08X", (uint) UNCACHED_SDRAM (initrd_start));
	linux_env_set ("initrd_start", env_buf);

	sprintf (env_buf, "0x%X", (uint) (initrd_end - initrd_start));
	linux_env_set ("initrd_size", env_buf);

	sprintf (env_buf, "0x%08X", (uint) (gd->bd->bi_flashstart));
	linux_env_set ("flash_start", env_buf);

	sprintf (env_buf, "0x%X", (uint) (gd->bd->bi_flashsize));
	linux_env_set ("flash_size", env_buf);

	/* we assume that the kernel is in place */
	printf ("\nStarting kernel ...\n\n");

/* cu570m condition branch */
#if defined(CONFIG_AR7100) || defined(CONFIG_AR7240) || defined(CONFIG_ATHEROS)
#ifdef CONFIG_WASP_SUPPORT
	wasp_set_cca();
#endif
	/* Pass the flash size as expected by current Linux kernel for AR7100 */
	flash_size_mbytes = gd->bd->bi_flashsize/(1024 * 1024);
    /* as the same, we pass the Memsize to kernel
     * uboot can auto search mem to set memsize,so this memsize is right
     * ZJin, 2011.07.18
     */
    mem_size_to_kernel = gd->ram_size;
	theKernel (linux_argc, linux_argv, mem_size_to_kernel, flash_size_mbytes);
	/* theKernel (linux_argc, linux_argv, linux_env, flash_size_mbytes); */
#else /* old default branch */
	theKernel (linux_argc, linux_argv, linux_env, 0);
#endif /* cu570m */
}

static void linux_params_init (ulong start, char *line)
{
	char *next, *quote, *argp;
	char memstr[32]; /* cu570m */

	linux_argc = 1;
	linux_argv = (char **) start;
	linux_argv[0] = 0;
	argp = (char *) (linux_argv + LINUX_MAX_ARGS);

	next = line;

/* cu570m start */
	if (strstr(line, "mem=")) {
		memstr[0] = 0;
	} else {
		memstr[0] = 1;
	}
/* cu570m end */

	while (line && *line && linux_argc < LINUX_MAX_ARGS) {
		quote = strchr (line, '"');
		next = strchr (line, ' ');

		while (next != NULL && quote != NULL && quote < next) {
			/* we found a left quote before the next blank
			 * now we have to find the matching right quote
			 */
			next = strchr (quote + 1, '"');
			if (next != NULL) {
				quote = strchr (next + 1, '"');
				next = strchr (next + 1, ' ');
			}
		}

		if (next == NULL) {
			next = line + strlen (line);
		}

		linux_argv[linux_argc] = argp;
		memcpy (argp, line, next - line);
		argp[next - line] = 0;
/* cu570m start */
#if defined(CONFIG_AR7240)
#define REVSTR	"REVISIONID"
#define PYTHON	"python"
#define VIRIAN	"virian"
		if (strcmp(argp, REVSTR) == 0) {
			if (is_ar7241() || is_ar7242()) {
				strcpy(argp, VIRIAN);
			} else {
				strcpy(argp, PYTHON);
			}
		}
#endif
/* cu570m end */

		argp += next - line + 1;
		linux_argc++;

		if (*next)
			next++;

		line = next;
	}

/* cu570m start */
#if defined(CONFIG_AR9100) || defined(CONFIG_AR7240) || defined(CONFIG_ATHEROS)
	/* Add mem size to command line */
	if (memstr[0]) {
		sprintf(memstr, "mem=%luM", gd->ram_size >> 20);
		memcpy (argp, memstr, strlen(memstr)+1);
		linux_argv[linux_argc] = argp;
		linux_argc++;
		argp += strlen(memstr) + 1;
	}
#endif
/* cu570m end */

	linux_env = (char **) (((ulong) argp + 15) & ~15);
	linux_env[0] = 0;
	linux_env_p = (char *) (linux_env + LINUX_MAX_ENVS);
	linux_env_idx = 0;
}

static void linux_env_set (char *env_name, char *env_val)
{
	if (linux_env_idx < LINUX_MAX_ENVS - 1) {
		linux_env[linux_env_idx] = linux_env_p;

		strcpy (linux_env_p, env_name);
		linux_env_p += strlen (env_name);

		strcpy (linux_env_p, "=");
		linux_env_p += 1;

		strcpy (linux_env_p, env_val);
		linux_env_p += strlen (env_val);

		linux_env_p++;
		linux_env[++linux_env_idx] = 0;
	}
}
