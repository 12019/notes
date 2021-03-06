/***************************************************************************
main.c - core dump debugger
-------------------
begin : Sun Aug 1 20:46:02 EDT 2003
copyright : (C) 2003 by Atanas Dimitrov
email : atanas_dimitrov@bobcat.gcsu.edu
***************************************************************************/
/****************************************************************************
 *
* This program is free software; you can redistribute it and/or modify *
* it under the terms of the GNU General Public License as published by *
* the Free Software Foundation; either version 2 of the License, or *
* (at your option) any later version. *
* ****************************************************************************/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <elf.h>
#include <sys/procfs.h>
#include <sys/time.h>
#include <sys/user.h>

#define TRUE 1
#define FALSE 0
#define EMPTY 0
#define MAX_NOTES 10

/* to avoid implicit declaration warnings generated by -Wall */
extern char *ctime(const time_t *timep);

typedef struct
{
	unsigned long r15,r14,r13,r12,rbp,rbx,r11,r10;
	unsigned long r9,r8,rax,rcx,rdx,rsi,rdi,orig_rax;
	unsigned long rip,cs,eflags;
	unsigned long rsp,ss;
  	unsigned long fs_base, gs_base;
	unsigned long ds,es,fs,gs; 
} user_regs_struct;

/* need to replace this with include/elf.h: Elf32_Nhdr ... wish I saw it earlier*/
/* this does work out pretty good for the 32 bit arch and accounts for the Note structure. */
/* definition is next to processing section */
typedef struct {
	Elf32_Word namesz;
	Elf32_Word descsz;
	Elf32_Word type;
}Elf32_Note_Part;

int main(int argc, char *argv[])
{
	int fdin;
	char *acctime, *modtime, *chtime;

	char *longarray;
	struct stat statbuf;
	int os_abi_unknown;

	Elf32_Ehdr *elfhdr, *elfhdr1;Elf32_Phdr *proghdr;
	int noteoffsets[MAX_NOTES];
	int chk_phoff=0, chk_phentsize=0, chk_phnum=0;
	int count, next_off=0;int note_sect_counter=0; /* number of note sections */
	int prstatus_bool=0, prpsinfo_bool=0;/* check for necessary number of command line arguments */

	if(argc !=2) {
		printf("Usage: %s <binary_file>\n", argv[0]);
		exit(1);
	}

	/* check for successful binary file opening */
	if ((fdin = open(argv[1], O_RDONLY)) == -1) {
		perror("open");
		exit(2);
	}

	/* get the attributes of the file but first check for errors */
	if ((fstat(fdin, &statbuf)) < 0) {
		perror("fstat");
		exit(3);
	}

	/* display some attributes */
	printf("\n.: FILE STATUS INFORMATION :.\n");
	printf("hard links: %d\n", statbuf.st_nlink);
	printf("uid: %d\n", statbuf.st_uid);
	printf("gid: %d\n", statbuf.st_gid);
	printf("size: %d\n", statbuf.st_size);
	printf("block size: %d\n", statbuf.st_blksize);
	printf("block count: %d\n", statbuf.st_blocks);
	acctime = (char *) ctime(&statbuf.st_atime);
	printf("last access: %s", acctime);
	modtime = (char *) ctime(&statbuf.st_mtime);
	printf("last mod: %s", modtime);
	chtime = (char *) ctime(&statbuf.st_ctime);
	printf("last change: %s", chtime);

	//printf("\nstarting to modify output...\n");
	fflush(stdout);
	//sleep(1);
	longarray = mmap(0, statbuf.st_size, PROT_READ, MAP_FILE | MAP_SHARED, fdin, 0);
	//perror("\nstatus of memory mapping");

	/* use memcmp to compare the first 4 bytes from file to
	   first 4 bytes of standard ELF header's first entry
	   - Elf32_Ehdr.e_ident[EI_IDENT] basically do byte
	   comparison at already defined indexes and/or offsets.
	   (see include/elf.h for definitions)
	   all this to verify the ELF type */
	if (memcmp(longarray, ELFMAG, SELFMAG) == 0) {
		//printf("ELF file type accepted\n");
		fflush(stdout);
	} else {
		printf("error: unrecognized file type\n");
		fflush(stdout);
		exit(5);
	}

	/* initialize the elf header structure as defined in include/elf.h */
	elfhdr = (Elf32_Ehdr *) ((unsigned char *) &longarray[0x0]);
	printf("\n.: CORE ELF HEADER INFORMATION :.\n");

	/* determine file class */
	if (elfhdr->e_ident[EI_CLASS] == ELFCLASS32)
		printf("capacity: 32 bit (ELFCLASS32) [%d]\n", elfhdr->e_ident[EI_CLASS]);
	else if (elfhdr->e_ident[EI_CLASS] == ELFCLASS64)
		printf("capacity: 64 bit (ELFCLASS64) [%d]\n", elfhdr->e_ident[EI_CLASS]);
	else if (elfhdr->e_ident[EI_CLASS] == ELFCLASSNONE)
		printf("capacity: none (ELFCLASSNONE) [%d]\n", elfhdr->e_ident[EI_CLASS]);
	else
		printf("error: capacity not defined within ELF header [%d]\n", elfhdr->e_ident[EI_CLASS]);

	/* determine data encoding type */
	if (elfhdr->e_ident[EI_DATA] == ELFDATA2LSB)
		printf("data encoding type: 2's comp, little endian (ELFDATA2LSB) [%d]\n",
			elfhdr->e_ident[EI_DATA]);
	else if (elfhdr->e_ident[EI_DATA] == ELFDATA2MSB)
		printf("data encoding type: 2's comp, big endian (ELFDATA2MSB) [%d]\n", 
			elfhdr->e_ident[EI_DATA]);
	else if (elfhdr->e_ident[EI_DATA] == ELFDATANONE)
		printf("data encoding type: none (ELFCLASSNONE) [%d]\n", 
			elfhdr->e_ident[EI_DATA]);
	else
		printf("data encoding type: not defined within ELF header [%d]\n",
			elfhdr->e_ident[EI_DATA]);

	/* determine version number (1 is current) and this is a must */
	if (elfhdr->e_ident[EI_VERSION] == EV_CURRENT)
		printf("file version: current (EV_CURRENT) [%d]\n", elfhdr->e_ident[EI_VERSION]);
	else
		printf("file version: illegal version [%d]\n", elfhdr->e_ident[EI_VERSION]);

	/* determine OS ABI identification (application binary interface) */
	if (elfhdr->e_ident[EI_OSABI] == ELFOSABI_NONE ||
	    elfhdr->e_ident[EI_OSABI] == ELFOSABI_SYSV)
		printf("OS ABI: Unix System V ABI [%d]\n", elfhdr->e_ident[EI_OSABI]);
	else if (elfhdr->e_ident[EI_OSABI] == ELFOSABI_HPUX)
		printf("OS ABI: HP-UX(ELFOSABI_HPUX) [%d]\n", elfhdr->e_ident[EI_OSABI]);
	else if (elfhdr->e_ident[EI_OSABI] == ELFOSABI_NETBSD)
		printf("OS ABI: NetBSD(ELFOSABI_NETBSD) [%d]\n", elfhdr->e_ident[EI_OSABI]);
	else if (elfhdr->e_ident[EI_OSABI] == ELFOSABI_LINUX)
		printf("OS ABI: Linux(ELFOSABI_LINUX) [%d]\n", elfhdr->e_ident[EI_OSABI]);
	else if (elfhdr->e_ident[EI_OSABI] == ELFOSABI_SOLARIS)
		printf("OS ABI: Solaris(ELFOSABI_SOLARIS) [%d]\n", elfhdr->e_ident[EI_OSABI]);
	else if (elfhdr->e_ident[EI_OSABI] == ELFOSABI_AIX)
		printf("OS ABI: AIX(ELFOSABI_AIX) [%d]\n", elfhdr->e_ident[EI_OSABI]);
	else if (elfhdr->e_ident[EI_OSABI] == ELFOSABI_IRIX)
		printf("OS ABI: Irix(ELFOSABI_IRIX) [%d]\n", elfhdr->e_ident[EI_OSABI]);
	else if (elfhdr->e_ident[EI_OSABI] == ELFOSABI_FREEBSD)
		printf("OS ABI: FreeBSD(ELFOSABI_FREEBSD) [%d]\n", elfhdr->e_ident[EI_OSABI]);
	else if (elfhdr->e_ident[EI_OSABI] == ELFOSABI_TRU64)
		printf("OS ABI: Compaq TRU64 Unix (ELFOSABI_HPUX) [%d]\n", elfhdr->e_ident[EI_OSABI]);
	else if (elfhdr->e_ident[EI_OSABI] == ELFOSABI_MODESTO)
		printf("OS ABI: Novell Modesto (ELFOSABI_MODESTO) [%d]\n", elfhdr->e_ident[EI_OSABI]);
	else if (elfhdr->e_ident[EI_OSABI] == ELFOSABI_OPENBSD)
		printf("OS ABI: OpenBSD(ELFOSABI_OPENBSD) [%d]\n", elfhdr->e_ident[EI_OSABI]);
	else if (elfhdr->e_ident[EI_OSABI] == ELFOSABI_ARM)
		printf("OS ABI: ARM(ELFOSABI_ARM) [%d]\n", elfhdr->e_ident[EI_OSABI]);
	else if (elfhdr->e_ident[EI_OSABI] == ELFOSABI_STANDALONE)
		printf("OS ABI: Standalone (Embedded) Application(ELFOSABI_STANDALONE) [%d]\n", elfhdr->e_ident[EI_OSABI]);
	else {
		printf("OS ABI: unknown [%d]\n", elfhdr->e_ident[EI_OSABI]);
		os_abi_unknown = TRUE;
	}

	/* determine ABI version if aplicable */
	if (!os_abi_unknown)
		printf("ABI version: %d\n", elfhdr->e_ident[EI_ABIVERSION]);
	else
		printf("ABI version: unknown [%d]\n", elfhdr->e_ident[EI_ABIVERSION]);

	/* display number of padding bytes */
		printf("padding bytes: %d\n", elfhdr->e_ident[EI_PAD]);

	/* determine object file type */
	if (elfhdr->e_type == ET_NONE)
		printf("object file type: no file type(ET_NONE) [%d]\n", elfhdr->e_type);
	else if (elfhdr->e_type == ET_REL)
		printf("object file type: relocatable file (ET_REL) [%d]\n", elfhdr->e_type);
	else if (elfhdr->e_type == ET_EXEC)
		printf("object file type: executable file (ET_EXEC) [%d]\n", elfhdr->e_type);
	else if (elfhdr->e_type == ET_DYN)
		printf("object file type: shared object file (ET_DYN) [%d]\n", elfhdr->e_type);
	else if (elfhdr->e_type == ET_CORE)
		printf("object file type: core file (ET_CORE) [%d]\n", elfhdr->e_type);
	else if (elfhdr->e_type == ET_NUM)
		printf("object file type: number of defined types (ET_NUM) [%d]\n", elfhdr->e_type);
	else if (elfhdr->e_type == ET_LOOS)
		printf("object file type: OS-specific range start (ET_LOOS) [%4x]\n", elfhdr->e_type);
	else if (elfhdr->e_type == ET_HIOS)
		printf("object file type: OS-specific range end (ET_HIOS) [%4x]\n", elfhdr->e_type);
	else if (elfhdr->e_type == ET_LOPROC)
		printf("object file type: Processor-specific range start (ET_LOPROC) [%4x]\n", elfhdr->e_type);
	else if (elfhdr->e_type == ET_HIPROC)
		printf("object file type: Processor-specific range end (ET_HIPROC) [%4x]\n", elfhdr->e_type);
	else
		printf("object file type: undefined [%4x]\n", elfhdr->e_type);

	/* determine architecture */
	/* 100 definitions enter here... just 3 for right now*/
	if (elfhdr->e_machine == EM_NONE)
		printf("architecture: none(EM_NONE) [%d]\n", elfhdr->e_machine);
	else if (elfhdr->e_machine == EM_386)
		printf("architecture: Intel 80386 (EM_NONE) [%d]\n", elfhdr->e_machine);
	else if (elfhdr->e_machine == EM_SPARC)
		printf("architecture: Sun SPARC (EM_SPARC) [%d]\n", elfhdr->e_machine);
	else
		printf("architecture: unknown[%d]\n", elfhdr->e_machine);

	/* determine object file version */
	if (elfhdr->e_version == EV_NONE)
		printf("object file version: invalid ELF version (EV_NONE) [%d]\n", elfhdr->e_version);
	else if (elfhdr->e_version == EV_CURRENT)
		printf("object file version: current (EV_CURRENT) [%d]\n", elfhdr->e_version);
	else
		printf("object file version: unknown [%d]\n", elfhdr->e_version);

	/* display the entry point virtual address */
	printf("entry point virtual address: %#x\n", elfhdr->e_entry);

	/* display the program header table file offset */
	printf("program header table file offset: %#x\n", elfhdr->e_phoff);

	if (elfhdr->e_phoff > 0)
		chk_phoff=1;

	/* display section header table file offset */
	printf("section header table file offset: %#x\n", elfhdr->e_shoff);
	/* display processor-specific flags */
	printf("processor speciffic flags: %#x\n", elfhdr->e_flags);
	/* display ELF header size in bytes */
	printf("ELF header size: %d bytes\n", elfhdr->e_ehsize);
	/* display program header table entry size */
	printf("program header table entry size: %d bytes\n", elfhdr->e_phentsize);

	if (elfhdr->e_phentsize > 0)
		chk_phentsize=1;

	/* display program header table entry count */
	printf("program header table entry count: %d\n", elfhdr->e_phnum);

	if (elfhdr->e_phnum > 0)
		chk_phnum=1;

	/* display section header table entry size */
	printf("section header table entry size: %d bytes\n", elfhdr->e_shentsize);
	/* display section header table entry count */
	printf("section header table entry count: %d\n", elfhdr->e_shnum);
	/* display section header string table index */
	printf("section header string table index: %d\n", elfhdr->e_shstrndx);

	/* check if we can work with program headers */
	/* for CORE rest will be empty */
	/* add other checks here if intentions to work with ELF in general */
	/* if value is zero this means that it doesn't hold an entry */
	/* unless ... pretty self explanatory */
	if (chk_phoff && chk_phentsize && chk_phnum) {
		/* table headers */
		printf("\n.: SEGMENT INFORMATION :.\n");
		printf("type offset vaddress paddress ");
		printf("size(file) size(mem) RWX align\n");
		printf("===================================");
		printf("=========================================\n");

		/* loop through all segments */
		for(count=0; count < elfhdr->e_phnum; ++count) {
			/* set the offset appropriately */
			proghdr = (Elf32_Phdr *) ((unsigned char *) \
				  &longarray[(unsigned int) \
				  (next_off + elfhdr->e_phoff)]);

			/* display all members as specified by table columns */

			/* determine segment type */
			if (proghdr->p_type == PT_NULL)
				printf("NULL");
			else if (proghdr->p_type == PT_LOAD)
				printf("LOAD");
			else if (proghdr->p_type == PT_DYNAMIC)
				printf("DYNAMIC");
			else if (proghdr->p_type == PT_INTERP)
				printf("INTERP");
			else if (proghdr->p_type == PT_NOTE) {
				/* if a note then store the offset into arraylater
				   this array will be processed*/
				printf("NOTE");
				noteoffsets[note_sect_counter]=proghdr->p_offset;
				note_sect_counter++;
			}
			else if (proghdr->p_type == PT_SHLIB)
				printf("SHLIB");
			else if (proghdr->p_type == PT_PHDR)
				printf("PHDR");
			else if (proghdr->p_type == PT_TLS)
				printf("TLS");
			else if (proghdr->p_type == PT_NUM)
				printf("NUM");
			else if (proghdr->p_type == PT_LOOS)
				printf("LOOS");
			else if (proghdr->p_type == PT_GNU_EH_FRAME)
				printf("GNU_EH_FRAME");
			else if (proghdr->p_type == PT_LOSUNW)
				printf("LOSUNW");
			else if (proghdr->p_type == PT_SUNWBSS)
				printf("SUNWBSS");
			else if (proghdr->p_type == PT_SUNWSTACK)
				printf("SUNWSTACK");
			else if (proghdr->p_type == PT_HISUNW)
				printf("HISUNW");
			else if (proghdr->p_type == PT_HIOS)
				printf("HIOS");
			else if (proghdr->p_type == PT_LOPROC)
				printf("LOPROC");
			else if (proghdr->p_type == PT_HIPROC)
				printf("HIPROC");
			else
				printf("UNKNOWN");

			/* print the rest */
			printf(" %#7x ", proghdr->p_offset);
			printf("%#10x ", proghdr->p_vaddr);
			printf("%#9x ", proghdr->p_paddr);
			printf("%11d ", proghdr->p_filesz);
			printf("%12d ", proghdr->p_memsz);
			printf("%#7x ", proghdr->p_flags);
			printf("%#9x ", proghdr->p_align);

			/* next row */
			printf("\n");

			/* increase offset to next program header table entry */
			next_off+=elfhdr->e_phentsize;
		} /* all segments */

		/* will try to make this with pointers */
		noteoffsets[note_sect_counter]='\0';
	} /* chk_phoff */

	/* need to get rid of this declaration and use more dynamic way of obtaining it */
	#define NOTE_PADDING 4

	/* determine if there are any note sections recorded contained in the program header */
	if (note_sect_counter != 0){
		/* note sections counter */
		int noteprcnt;printf("\n.: NOTES INFORMATION :.\n");
		unsigned int note_next_note_off=0; 

		//next note offset initially 0
		/* go through each note section and display all the notes in it */
		for (noteprcnt=0; noteprcnt< note_sect_counter; ++noteprcnt) {
			unsigned int note_sect_off = noteoffsets[noteprcnt];
			printf("\n### found note section at offset: %#x ###\n", note_sect_off);
			int i=0;
			int note_counter=0;

			/* stupid loop this should contain a logical end of the notes int he section */
			while ( i < 3 ) {
				int j=0;
				char *note_name;/* note name storage */
				unsigned int note_begin_off=0;/* beginning of the next note inthe note section */
				unsigned int note_name_begin_off=0;/* location of the note namemember */
				unsigned int note_desc_begin_off=0;/* location of description member*/
				unsigned int note_part_total=0;/* size of note header */
				unsigned int note_name_total=0;/* size of name */
				unsigned int note_desc_total=0;/* size of desc */

				/* advance to the next note within the section */
				//printf("%#x\n", note_next_note_off);
				note_begin_off = (unsigned int) note_sect_off + note_next_note_off;
				printf("\n--- note %d at offset %#x ---\n", note_counter, note_begin_off);

				/*the structure of a note in an ELF
				  0   1   2   3  -th byte
				a |-----------|
				s |  namesz   |
				c |-----------| appropriate padding is used for first 3 fields
				e |  descsz   | rest are limited by the upper
				n |-----------| for 64 bit arch i would assume we will
				d |   type    | have 8 byte padding instead of 4
				  |-----------|
				| |   name    |
				| |-----------|
				| |   desc    |
				V |-----------|
				 */ 

				/* find the note */
				Elf32_Note_Part *elfnote = (Elf32_Note_Part *) ((unsigned char *)&longarray[note_begin_off]);

				/* includes terminating character */
				printf("padding: %d bytes\n", NOTE_PADDING);
				printf("note name size: %#x bytes\n", elfnote->namesz);
				printf("note description size: %#x bytes\n", elfnote->descsz);

				/* total bytes of the partial elf note */
				note_part_total = (unsigned int) (sizeof(elfnote->namesz) + sizeof(elfnote->descsz) + sizeof(elfnote->type));

				/* calculate the name offset */
				note_name_begin_off = note_begin_off + note_part_total;

				/* get note name and display it */
				note_name = &longarray[note_name_begin_off];
				printf ("note name: %s\n", note_name);

				/* padding must be accounted for */
				note_name_total = (unsigned int) ( elfnote->namesz + (NOTE_PADDING - ((elfnote->namesz) %NOTE_PADDING)));

				/* padding must be accounted for */
				/*note_desc_begin_off = note_name_begin_off + (unsigned int) (strlen(note_name) + 1 + \(NOTE_PADDING - ((strlen(note_name) +1) %NOTE_PADDING)));*/
				note_desc_begin_off = note_name_begin_off + note_name_total;
				switch (elfnote->type) {
				case NT_PRSTATUS:
				printf("note type: PRSTATUS [%d]\n", elfnote->type);
				prstatus_bool = TRUE;
				/* now initializethe prstatus structure */
				struct elf_prstatus *prstat = (struct elf_prstatus *) ((unsigned char *) &longarray[note_desc_begin_off]);
				/* display members as defined in sys/procfs.h */

				/* signal info */
				printf("signal number: %d\n", prstat->pr_info.si_signo);
				printf("extra code: %d\n", prstat->pr_info.si_code);
				printf("errno: %d\n", prstat->pr_info.si_errno);

				/* continue with prstatus */
				printf("current signal: %d\n", prstat->pr_cursig);

				printf("set of pending signals: %#lx\n", prstat->pr_sigpend);
				printf("set of held signals: %#lx\n", prstat->pr_sigpend);
				printf("pid: %d\n", prstat->pr_pid);
				printf("ppid: %d\n", prstat->pr_pid);
				printf("pgrp: %d\n", prstat->pr_pgrp);
				printf("sid: %d\n", prstat->pr_sid);
				printf("user time: %d.%d sec\n", prstat->pr_utime.tv_sec, prstat->pr_utime.tv_usec);
				printf("system time: %d.%d sec\n", prstat->pr_stime.tv_sec, prstat->pr_stime.tv_usec);
				printf("cumulative user time: %d.%d sec\n", prstat->pr_cutime.tv_sec, prstat->pr_cutime.tv_usec);
				printf("cumulative system time: %d.%d sec\n", prstat->pr_cstime.tv_sec, prstat->pr_cstime.tv_usec);

				/* print out boolean that indicates TRUE if math coprobeing used */
				printf("bool pr_fpvalid: %d\n", prstat->pr_fpvalid);
				/* display GP (general purpose) registers' values */
				/* the user_regs struct has been converted as an array oflong int for some reason */
				/* size of this array is stored in ELF_NGREG ...why allthis??? */
				/* let's change it */
				user_regs_struct *u_regs = (user_regs_struct *) (prstat->pr_reg);
				/* print the values */
				printf("\ngeneral purpose registers:\n");
				printf("r15: %#10lx\t\t", u_regs->r15);
				printf("r14: %#10lx\t\t", u_regs->r14);
				printf("r13: %#10lx\t\t", u_regs->r13);
				printf("r12: %#10lx\t\t", u_regs->r12);
				printf("rbp: %#10lx\t\t", u_regs->rbp);
				printf("rbx: %#10lx\t\t", u_regs->rbx);
				printf("r11: %#10lx\t\t", u_regs->r11);
				printf("r10: %#10lx\t\t", u_regs->r10);
				printf("r9: %#10lx\t\t", u_regs->r9);
				printf("r8: %#10lx\t\t", u_regs->r8);

#if 0

				printf("ecx: %#10lx\t\t", u_regs->ecx);
				printf("edx: %#10lx\n", u_regs->edx);
				printf("esi: %#10lx\t\t", u_regs->esi);
				printf("edi: %#10lx\t\t", u_regs->edi);
				printf("ebp: %#10lx\n", u_regs->ebp);
				printf("eax: %#10lx\t\t", u_regs->eax);
				printf("xds: %#10lx\t\t", u_regs->xds);
				printf("xes: %#10lx\n", u_regs->xes);
				printf("xfs: %#10lx\t\t", u_regs->xfs);
				printf("xgs: %#10lx\t\t", u_regs->xgs);
				printf("orig_eax: %#10lx\n", u_regs->orig_eax);
				printf("eip: %#10lx\t\t", u_regs->eip);
				printf("xcs: %#10lx\t\t", u_regs->xcs);
				printf("eflags: %#12lx\n", u_regs->eflags);
				printf("esp: %#10lx\t\t", u_regs->esp);
				printf("xss: %#10lx\t\t\n", u_regs->xss);
#endif
				note_desc_total = (unsigned int) sizeof(struct elf_prstatus);
				note_next_note_off = (unsigned int) note_next_note_off +(note_part_total + note_name_total + note_desc_total);
				/*end of prstatus note handling section */
				break;

				case NT_FPREGSET:
				printf("note type: FPREGSET [%d]\n", elfnote->type);
				break;

				case NT_PRPSINFO:
				printf("note type: PRPSINFO [%d]\n", elfnote->type);
				prpsinfo_bool = TRUE;
				struct elf_prpsinfo *prpsinfo = (struct elf_prpsinfo *) ((unsigned char *) &longarray[note_desc_begin_off]);
				printf("numeric process state: %c\n", prpsinfo->pr_state);
				printf("char for pr_state: %c\n", prpsinfo->pr_sname);
				printf("zombie: %c\n", prpsinfo->pr_zomb);
				printf("nice val: %c\n", prpsinfo->pr_nice);
				printf("flag 1: %#lx\n", prpsinfo->pr_state);
				printf("uid: %d\n", prpsinfo->pr_uid);
				printf("gid: %d\n", prpsinfo->pr_gid);
				printf("pid: %d\n", prpsinfo->pr_pid);
				printf("ppid: %d\n", prpsinfo->pr_ppid);
				printf("pgrp: %d\n", prpsinfo->pr_pgrp);
				printf("sid: %d\n", prpsinfo->pr_sid);
				printf("filename or executable: %s\n", prpsinfo->pr_fname);

				//for (j=0; j < ELF_PRARGSZ; ++j)
				//{
				//printf("argument list : %c\n", prpsinfo->pr_psargs[i]);
				//}

				note_desc_total = (unsigned int) sizeof(struct elf_prpsinfo);
				note_next_note_off = (unsigned int) note_next_note_off +(note_part_total + note_name_total + note_desc_total);
				break;

				case NT_PRXREG:
				printf("note type: PRXREG [%d]\n", elfnote->type);
				note_desc_total = elfnote->descsz;
				//next note offset for now
				note_next_note_off = (unsigned int) note_next_note_off +note_part_total + note_name_total + note_desc_total;
				printf("next note at %#x\n", note_next_note_off);
				break;

				//for completeness
				/*case NT_TASKSTRUCT:
				printf("note type: TASKSTRUCT [%d]\n",elfnote->type);
				break;*/

				case NT_PLATFORM:
				printf("note type: PLATFORM [%d]\n", elfnote->type);
				break;

				case NT_AUXV:
				printf("note type: AUXV [%d]\n", elfnote->type);
				break;

				case NT_GWINDOWS:
				printf("note type: GWINDOWS [%d]\n", elfnote->type);
				break;

				case NT_ASRS:
				printf("note type: ASRS [%d]\n", elfnote->type);
				break;

				case NT_PSTATUS:
				printf("note type: PSTATUS [%d]\n", elfnote->type);
				break;

				case NT_PSINFO:
				printf("note type: PSINFO [%d]\n", elfnote->type);
				break;

				case NT_PRCRED:
				printf("note type: PRCRED [%d]\n", elfnote->type);
				break;

				case NT_UTSNAME:
				printf("note type: UTSNAME [%d]\n", elfnote->type);
				break;

				case NT_LWPSTATUS:
				printf("note type: LWPSTATUS [%d]\n", elfnote->type);
				break;

				case NT_LWPSINFO:
				printf("note type: LWPSINFO [%d]\n", elfnote->type);
				break;

				case NT_PRFPXREG:
				printf("note type: PRFPXREG [%d]\n", elfnote->type);
				break;

				default:
				printf("note type: not recognized [%d]\n",elfnote->type);
				break;
				} /* switch (elfnote->type) */

				++i;
				++note_counter;

			} /* while */
		} /* noteprcnt */

		/*dump file contents onto stdout remove comment if necessary ... just fordebugging
		write(fileno(stdout), longarray, statbuf.st_size);*/
		/* Section header processing */
	} /* note_sect_counter */

	/* Section header processing */
	elfhdr1 = (Elf32_Ehdr *) ((unsigned char *) &longarray[0x1000]);
	printf("\n.: ENCLOSED ELF HEADER INFORMATION at 0x1000 :.\n");

	/* determine file class */
	if (elfhdr1->e_ident[EI_CLASS] == ELFCLASS32)
		printf("capacity: 32 bit (ELFCLASS32) [%d]\n", elfhdr1->e_ident[EI_CLASS]);
	else if (elfhdr1->e_ident[EI_CLASS] == ELFCLASS64)
		printf("capacity: 64 bit (ELFCLASS64) [%d]\n", elfhdr1->e_ident[EI_CLASS]);
	else if (elfhdr1->e_ident[EI_CLASS] == ELFCLASSNONE)
		printf("capacity: none (ELFCLASSNONE) [%d]\n", elfhdr1->e_ident[EI_CLASS]);
	else
		printf("error: capacity not defined within ELF header [%d]\n", elfhdr1->e_ident[EI_CLASS]);

	/* determine data encoding type */
	if (elfhdr1->e_ident[EI_DATA] == ELFDATA2LSB)
		printf("data encoding type: 2's comp, little endian (ELFDATA2LSB) [%d]\n",elfhdr1->e_ident[EI_DATA]);
	else if (elfhdr1->e_ident[EI_DATA] == ELFDATA2MSB)
		printf("data encoding type: 2's comp, big endian (ELFDATA2MSB) [%d]\n", elfhdr1->e_ident[EI_DATA]);
	else if (elfhdr1->e_ident[EI_DATA] == ELFDATANONE)
		printf("data encoding type: none (ELFCLASSNONE) [%d]\n", elfhdr1->e_ident[EI_DATA]);
	else
		printf("data encoding type: not defined within ELF header [%d]\n",elfhdr1->e_ident[EI_DATA]);

	/* determine version number (1 is current) and this is a must */
	if (elfhdr1->e_ident[EI_VERSION] == EV_CURRENT)
		printf("file version: current (EV_CURRENT) [%d]\n", elfhdr1->e_ident[EI_VERSION]);
	else
		printf("file version: illegal version [%d]\n", elfhdr1->e_ident[EI_VERSION]);

	/* determine OS ABI identification (application binary interface) */
	if (elfhdr1->e_ident[EI_OSABI] == ELFOSABI_NONE || elfhdr1->e_ident[EI_OSABI] == ELFOSABI_SYSV)
		printf("OS ABI: Unix System V ABI [%d]\n", elfhdr1->e_ident[EI_OSABI]);
	else if (elfhdr1->e_ident[EI_OSABI] == ELFOSABI_HPUX)
		printf("OS ABI: HP-UX(ELFOSABI_HPUX) [%d]\n", elfhdr1->e_ident[EI_OSABI]);
	else if (elfhdr1->e_ident[EI_OSABI] == ELFOSABI_NETBSD)
		printf("OS ABI: NetBSD(ELFOSABI_NETBSD) [%d]\n", elfhdr1->e_ident[EI_OSABI]);
	else if (elfhdr1->e_ident[EI_OSABI] == ELFOSABI_LINUX)
		printf("OS ABI: Linux(ELFOSABI_LINUX) [%d]\n", elfhdr1->e_ident[EI_OSABI]);
	else if (elfhdr1->e_ident[EI_OSABI] == ELFOSABI_SOLARIS)
		printf("OS ABI: Solaris(ELFOSABI_SOLARIS) [%d]\n", elfhdr1->e_ident[EI_OSABI]);
	else if (elfhdr1->e_ident[EI_OSABI] == ELFOSABI_AIX)
		printf("OS ABI: AIX(ELFOSABI_AIX) [%d]\n", elfhdr1->e_ident[EI_OSABI]);
	else if (elfhdr1->e_ident[EI_OSABI] == ELFOSABI_IRIX)
		printf("OS ABI: Irix(ELFOSABI_IRIX) [%d]\n", elfhdr1->e_ident[EI_OSABI]);
	else if (elfhdr1->e_ident[EI_OSABI] == ELFOSABI_FREEBSD)
		printf("OS ABI: FreeBSD(ELFOSABI_FREEBSD) [%d]\n", elfhdr1->e_ident[EI_OSABI]);
	else if (elfhdr1->e_ident[EI_OSABI] == ELFOSABI_TRU64)
		printf("OS ABI: Compaq TRU64 Unix (ELFOSABI_HPUX) [%d]\n", elfhdr1->e_ident[EI_OSABI]);
	else if (elfhdr1->e_ident[EI_OSABI] == ELFOSABI_MODESTO)
		printf("OS ABI: Novell Modesto (ELFOSABI_MODESTO) [%d]\n", elfhdr1->e_ident[EI_OSABI]);
	else if (elfhdr1->e_ident[EI_OSABI] == ELFOSABI_OPENBSD)
		printf("OS ABI: OpenBSD(ELFOSABI_OPENBSD) [%d]\n", elfhdr1->e_ident[EI_OSABI]);
	else if (elfhdr1->e_ident[EI_OSABI] == ELFOSABI_ARM)
		printf("OS ABI: ARM(ELFOSABI_ARM) [%d]\n", elfhdr1->e_ident[EI_OSABI]);
	else if (elfhdr1->e_ident[EI_OSABI] == ELFOSABI_STANDALONE)
		printf("OS ABI: Standalone (Embedded) Application(ELFOSABI_STANDALONE) [%d]\n", elfhdr1->e_ident[EI_OSABI]);
	else {
		printf("OS ABI: unknown [%d]\n", elfhdr1->e_ident[EI_OSABI]);
		os_abi_unknown = TRUE;
	}

	/* determine ABI version if aplicable */
	if (!os_abi_unknown)
		printf("ABI version: %d\n", elfhdr1->e_ident[EI_ABIVERSION]);
	else
		printf("ABI version: unknown [%d]\n", elfhdr1->e_ident[EI_ABIVERSION]);

	/* display number of padding bytes */
	printf("padding bytes: %d\n", elfhdr1->e_ident[EI_PAD]);
	/* determine object file type */
	if (elfhdr1->e_type == ET_NONE)
		printf("object file type: no file type(ET_NONE) [%d]\n", elfhdr1->e_type);
	else if (elfhdr1->e_type == ET_REL)
		printf("object file type: relocatable file (ET_REL) [%d]\n", elfhdr1->e_type);
	else if (elfhdr1->e_type == ET_EXEC)
		printf("object file type: executable file (ET_EXEC) [%d]\n", elfhdr1->e_type);
	else if (elfhdr1->e_type == ET_DYN)
		printf("object file type: shared object file (ET_DYN) [%d]\n", elfhdr1->e_type);
	else if (elfhdr1->e_type == ET_CORE)
		printf("object file type: core file (ET_CORE) [%d]\n", elfhdr1->e_type);
	else if (elfhdr1->e_type == ET_NUM)
		printf("object file type: number of defined types (ET_NUM) [%d]\n", elfhdr1->e_type);
	else if (elfhdr1->e_type == ET_LOOS)
		printf("object file type: OS-specific range start (ET_LOOS) [%4x]\n", elfhdr1->e_type);
	else if (elfhdr1->e_type == ET_HIOS)
		printf("object file type: OS-specific range end (ET_HIOS) [%4x]\n", elfhdr1->e_type);
	else if (elfhdr1->e_type == ET_LOPROC)
		printf("object file type: Processor-specific range start (ET_LOPROC) [%4x]\n", elfhdr1->e_type);
	else if (elfhdr1->e_type == ET_HIPROC)
		printf("object file type: Processor-specific range end (ET_HIPROC) [%4x]\n", elfhdr1->e_type);
	else
		printf("object file type: undefined [%4x]\n", elfhdr1->e_type);

	/* determine architecture */
	/* 100 definitions enter here... just 3 for right now*/
	if (elfhdr1->e_machine == EM_NONE)
		printf("architecture: none(EM_NONE) [%d]\n", elfhdr1->e_machine);
	else if (elfhdr1->e_machine == EM_386)
		printf("architecture: Intel 80386 (EM_NONE) [%d]\n", elfhdr1->e_machine);
	else if (elfhdr1->e_machine == EM_SPARC)
		printf("architecture: Sun SPARC (EM_SPARC) [%d]\n", elfhdr1->e_machine);
	else
		printf("architecture: unknown[%d]\n", elfhdr1->e_machine);

	/* determine object file version */
	if (elfhdr1->e_version == EV_NONE)
		printf("object file version: invalid ELF version (EV_NONE) [%d]\n", elfhdr1->e_version);
	else if (elfhdr1->e_version == EV_CURRENT)
		printf("object file version: current (EV_CURRENT) [%d]\n", elfhdr1->e_version);
	else printf("object file version: unknown [%d]\n", elfhdr1->e_version);
		/* display the entry point virtual address */
		printf("entry point virtual address: %#x\n", elfhdr1->e_entry);

	/* display the program header table file offset */
	printf("program header table file offset: %#x\n", elfhdr1->e_phoff);

	if (elfhdr1->e_phoff > 0)
		chk_phoff=1;

	/* display section header table file offset */
	printf("section header table file offset: %#x\n", elfhdr1->e_shoff);
	/* display processor-specific flags */
	printf("processor speciffic flags: %#x\n", elfhdr1->e_flags);
	/* display ELF header size in bytes */
	printf("ELF header size: %d bytes\n", elfhdr1->e_ehsize);
	/* display program header table entry size */
	printf("program header table entry size: %d bytes\n", elfhdr1->e_phentsize);

	if (elfhdr1->e_phentsize > 0)
		chk_phentsize=1;

	/* display program header table entry count */
	printf("program header table entry count: %d\n", elfhdr1->e_phnum);

	if (elfhdr1->e_phnum > 0)
		chk_phnum=1;

	/* display section header table entry size */
	printf("section header table entry size: %d bytes\n", elfhdr1->e_shentsize);
	/* display section header table entry count */
	printf("section header table entry count: %d\n", elfhdr1->e_shnum);
	/* display section header string table index */
	printf("section header string table index: %d\n", elfhdr1->e_shstrndx);

	/* unmap file; free the memory */
	munmap(&longarray, statbuf.st_size);
	return(1);
}



