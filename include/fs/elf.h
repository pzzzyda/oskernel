#ifndef _ELF_H
#define _ELF_H

#include "types.h"

#define ELF_MAGIC 0x464C457FU /* "\x7FELF" in little endian */

struct elfhdr {
	uint32_t magic;	    /* Must equal ELF_MAGIC */
	uint8_t elf[12];    /* Other info in magic number */
	uint16_t type;	    /* Object file type */
	uint16_t machine;   /* Architecture */
	uint32_t version;   /* Object file version */
	uint64_t entry;	    /* Enter point virtual address */
	uint64_t phoff;	    /* Program header table file offset */
	uint64_t shoff;	    /* Section header table file offset */
	uint32_t flags;	    /* Processor-specific flags */
	uint16_t ehsize;    /* ELF header size in bytes */
	uint16_t phentsize; /* Program header table entry size */
	uint16_t phnum;	    /* Program header table entry count */
	uint16_t shentsize; /* Section header table entry size */
	uint16_t shnum;	    /* Section header table entry count */
	uint16_t shstrndx;  /* Section header string table index */
};

struct proghdr {
	uint32_t type;	 /* Segment type */
	uint32_t flags;	 /* Segment flags */
	uint64_t off;	 /* Segment file offset */
	uint64_t vaddr;	 /* Segment virtual address */
	uint64_t paddr;	 /* Segment physical address */
	uint64_t filesz; /* Segment size in file */
	uint64_t memsz;	 /* Segment size int memory */
	uint64_t align;	 /* Segment alignment */
};

/* Values for Proghdr type */
#define ELF_PROG_LOAD 1

/* Flag bits for Proghdr flags */
#define ELF_PROG_FLAG_EXEC 1
#define ELF_PROG_FLAG_WRITE 2
#define ELF_PROG_FLAG_READ 4

#endif
