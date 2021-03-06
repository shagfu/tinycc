#ifdef TARGET_DEFS_ONLY

#define EM_TCC_TARGET EM_X86_64

/* relocation type for 32 bit data relocation */
#define R_DATA_32   R_X86_64_32
#define R_DATA_PTR  R_X86_64_64
#define R_JMP_SLOT  R_X86_64_JUMP_SLOT
#define R_GLOB_DAT  R_X86_64_GLOB_DAT
#define R_COPY      R_X86_64_COPY

#define R_NUM       R_X86_64_NUM

#define ELF_START_ADDR 0x400000
#define ELF_PAGE_SIZE  0x200000

#define HAVE_SECTION_RELOC
#define PCRELATIVE_DLLPLT 1

#else /* !TARGET_DEFS_ONLY */

#include "tcc.h"

static ElfW_Rel *qrel; /* ptr to next reloc entry reused */
ST_DATA struct reloc_info relocs_info[R_NUM] = {
    INIT_RELOC_INFO (R_X86_64_64, 0, AUTO_GOTPLT_ENTRY, 0)
    INIT_RELOC_INFO (R_X86_64_32, 0, AUTO_GOTPLT_ENTRY, 0)
    INIT_RELOC_INFO (R_X86_64_32S, 0, AUTO_GOTPLT_ENTRY, 0)
    INIT_RELOC_INFO (R_X86_64_PC32, 1, AUTO_GOTPLT_ENTRY, 0)
    INIT_RELOC_INFO (R_X86_64_PLT32, 1, ALWAYS_GOTPLT_ENTRY, 1)
    INIT_RELOC_INFO (R_X86_64_GLOB_DAT, 0, NO_GOTPLT_ENTRY, 0)
    INIT_RELOC_INFO (R_X86_64_JUMP_SLOT, 1, NO_GOTPLT_ENTRY, 0)
    INIT_RELOC_INFO (R_X86_64_GOTPCREL, 0, ALWAYS_GOTPLT_ENTRY, 0)
    INIT_RELOC_INFO (R_X86_64_GOTPCRELX, 0, ALWAYS_GOTPLT_ENTRY, 0)
    INIT_RELOC_INFO (R_X86_64_REX_GOTPCRELX, 0, ALWAYS_GOTPLT_ENTRY, 0)
    INIT_RELOC_INFO (R_X86_64_GOTTPOFF, 0, BUILD_GOT_ONLY, 0)
    INIT_RELOC_INFO (R_X86_64_GOT32, 0, ALWAYS_GOTPLT_ENTRY, 0)
};

void relocate_init(Section *sr)
{
    qrel = (ElfW_Rel *) sr->data;
}

void relocate(TCCState *s1, ElfW_Rel *rel, int type, char *ptr, addr_t addr, addr_t val)
{
    int sym_index, esym_index;

    sym_index = ELFW(R_SYM)(rel->r_info);

    switch (type) {
        case R_X86_64_64:
            if (s1->output_type == TCC_OUTPUT_DLL) {
                esym_index = s1->symtab_to_dynsym[sym_index];
                qrel->r_offset = rel->r_offset;
                if (esym_index) {
                    qrel->r_info = ELFW(R_INFO)(esym_index, R_X86_64_64);
		    qrel->r_addend = rel->r_addend;
                    qrel++;
                    break;
                } else {
		    qrel->r_info = ELFW(R_INFO)(0, R_X86_64_RELATIVE);
		    qrel->r_addend = read64le(ptr) + val;
                    qrel++;
                }
            }
            add64le(ptr, val);
            break;
        case R_X86_64_32:
        case R_X86_64_32S:
            if (s1->output_type == TCC_OUTPUT_DLL) {
                /* XXX: this logic may depend on TCC's codegen
                   now TCC uses R_X86_64_32 even for a 64bit pointer */
                qrel->r_info = ELFW(R_INFO)(0, R_X86_64_RELATIVE);
                /* Use sign extension! */
                qrel->r_addend = (int)read32le(ptr) + val;
                qrel++;
            }
            add32le(ptr, val);
            break;

        case R_X86_64_PC32:
            if (s1->output_type == TCC_OUTPUT_DLL) {
                /* DLL relocation */
                esym_index = s1->symtab_to_dynsym[sym_index];
                if (esym_index) {
                    qrel->r_offset = rel->r_offset;
                    qrel->r_info = ELFW(R_INFO)(esym_index, R_X86_64_PC32);
                    /* Use sign extension! */
                    qrel->r_addend = (int)read32le(ptr) + rel->r_addend;
                    qrel++;
                    break;
                }
            }
            goto plt32pc32;

        case R_X86_64_PLT32:
	    /* fallthrough: val already holds the PLT slot address */

	plt32pc32:
	{
            long long diff;
            diff = (long long)val - addr;
            if (diff < -2147483648LL || diff > 2147483647LL) {
                tcc_error("internal error: relocation failed");
            }
            add32le(ptr, diff);
        }
            break;
        case R_X86_64_GLOB_DAT:
        case R_X86_64_JUMP_SLOT:
            /* They don't need addend */
            write64le(ptr, val - rel->r_addend);
            break;
        case R_X86_64_GOTPCREL:
	case R_X86_64_GOTPCRELX:
	case R_X86_64_REX_GOTPCRELX:
            add32le(ptr, s1->got->sh_addr - addr +
                         s1->sym_attrs[sym_index].got_offset - 4);
            break;
        case R_X86_64_GOTTPOFF:
            add32le(ptr, val - s1->got->sh_addr);
            break;
        case R_X86_64_GOT32:
            /* we load the got offset */
            add32le(ptr, s1->sym_attrs[sym_index].got_offset);
            break;
    }
}

#endif /* !TARGET_DEFS_ONLY */
