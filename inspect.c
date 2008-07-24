/*  Copyright (C) 2008  Anders Kaseorg <andersk@mit.edu>,
 *                      Tim Abbott <tabbott@mit.edu>,
 *                      Jeffrey Brian Arnold <jbarnold@mit.edu>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License, version 2.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street - Fifth Floor, Boston, MA
 *  02110-1301, USA.
 */

#define _GNU_SOURCE
#include "objcommon.h"
#include "kmodsrc/ksplice.h"
#include <stdio.h>

bfd *ibfd;
struct asymbolp_vec isyms;

bfd_vma read_reloc(asection *sect, void *addr, size_t size, asymbol **symp)
{
	struct supersect *ss = fetch_supersect(ibfd, sect, &isyms);
	arelent **relocp;
	bfd_vma val = bfd_get(size * 8, ibfd, addr);
	bfd_vma address = addr_offset(ss, addr);
	for (relocp = ss->relocs.data;
	     relocp < ss->relocs.data + ss->relocs.size; relocp++) {
		arelent *reloc = *relocp;
		if (reloc->address == address) {
			if (symp != NULL)
				*symp = *reloc->sym_ptr_ptr;
			else if (*reloc->sym_ptr_ptr !=
				 bfd_abs_section_ptr->symbol)
				fprintf(stderr, "warning: unexpected "
					"non-absolute relocation at %s+%lx\n",
					sect->name, (unsigned long)address);
			return get_reloc_offset(ss, reloc, 0);
		}
	}
	if (symp != NULL)
		*symp = *bfd_abs_section_ptr->symbol_ptr_ptr;
	return val;
}

#define read_num(sect, addr) ((typeof(*(addr))) \
			      read_reloc(sect, addr, sizeof(*(addr)), NULL))

char *str_pointer(asection *sect, void **addr)
{
	asymbol *sym;
	bfd_vma offset = read_reloc(sect, addr, sizeof(*addr), &sym);
	char *str;
	assert(asprintf(&str, "%s+%lx", sym->name, (unsigned long)offset) >= 0);
	return str;
}

void *read_pointer(asection *sect, void **addr, asection **sectp)
{
	asymbol *sym;
	bfd_vma offset = read_reloc(sect, addr, sizeof(*addr), &sym);
	struct supersect *ss = fetch_supersect(ibfd, sym->section, &isyms);
	if (bfd_is_abs_section(sym->section) && sym->value + offset == 0)
		return NULL;
	if (bfd_is_const_section(sym->section)) {
		fprintf(stderr, "warning: unexpected relocation to const "
			"section at %s+%lx\n", sect->name,
			(unsigned long)addr_offset(ss, addr));
		return NULL;
	}
	if (sectp != NULL)
		*sectp = sym->section;
	return ss->contents.data + sym->value + offset;
}

char *read_string(asection *sect, char **addr)
{
	return read_pointer(sect, (void **)addr, NULL);
}

char *str_ulong_vec(asection *sect, unsigned long **datap, unsigned long *sizep)
{
	asection *data_sect;
	long *data = read_pointer(sect, (void **)datap, &data_sect);
	unsigned long size = read_num(sect, sizep);

	char *buf = NULL;
	size_t bufsize = 0;
	FILE *fp = open_memstream(&buf, &bufsize);
	fprintf(fp, "[ ");
	size_t i;
	for (i = 0; i < size; ++i)
		fprintf(fp, "%lx ", read_num(data_sect, &data[i]));
	fprintf(fp, "]");
	fclose(fp);
	return buf;
}

void show_ksplice_reloc(asection *sect, struct ksplice_reloc *kreloc)
{
	printf("blank_addr: %s  blank_offset: %lx\n"
	       "sym_name: %s\n"
	       "addend: %lx\n"
	       "pcrel: %x  size: %x  dst_mask: %lx  rightshift: %x\n"
	       "sym_addrs: %s\n"
	       "\n",
	       str_pointer(sect, (void **)&kreloc->blank_addr),
	       read_num(sect, &kreloc->blank_offset),
	       read_string(sect, &kreloc->sym_name),
	       read_num(sect, &kreloc->addend),
	       read_num(sect, &kreloc->pcrel),
	       read_num(sect, &kreloc->size),
	       read_num(sect, &kreloc->dst_mask),
	       read_num(sect, &kreloc->rightshift),
	       str_ulong_vec(sect, &kreloc->sym_addrs, &kreloc->num_sym_addrs));
}

void show_ksplice_relocs(asection *kreloc_sect)
{
	printf("KSPLICE RELOCATIONS:\n\n");
	struct supersect *kreloc_ss = fetch_supersect(ibfd, kreloc_sect,
						      &isyms);
	struct ksplice_reloc *kreloc;
	for (kreloc = kreloc_ss->contents.data; (void *)kreloc <
	     kreloc_ss->contents.data + kreloc_ss->contents.size; kreloc++)
		show_ksplice_reloc(kreloc_sect, kreloc);
	printf("\n");
}

void show_ksplice_size(asection *sect, struct ksplice_size *ksize)
{
	printf("name: %s\n"
	       "thismod_addr: %s  size: %lx\n"
	       "sym_addrs: %s\n"
	       "\n",
	       read_string(sect, &ksize->name),
	       str_pointer(sect, (void **)&ksize->thismod_addr),
	       read_num(sect, &ksize->size),
	       str_ulong_vec(sect, &ksize->sym_addrs, &ksize->num_sym_addrs));
}

void show_ksplice_sizes(asection *ksize_sect)
{
	printf("KSPLICE SIZES:\n\n");
	struct supersect *ksize_ss = fetch_supersect(ibfd, ksize_sect, &isyms);
	struct ksplice_size *ksize;
	for (ksize = ksize_ss->contents.data; (void *)ksize <
	     ksize_ss->contents.data + ksize_ss->contents.size; ksize++)
		show_ksplice_size(ksize_sect, ksize);
	printf("\n");
}

void show_ksplice_patch(asection *sect, struct ksplice_patch *kpatch)
{
	printf("oldstr: %s\n"
	       "repladdr: %s\n"
	       "\n",
	       read_string(sect, &kpatch->oldstr),
	       str_pointer(sect, (void **)&kpatch->repladdr));
}

void show_ksplice_patches(asection *kpatch_sect)
{
	printf("KSPLICE PATCHES:\n\n");
	struct supersect *kpatch_ss = fetch_supersect(ibfd, kpatch_sect,
						      &isyms);
	struct ksplice_patch *kpatch;
	for (kpatch = kpatch_ss->contents.data; (void *)kpatch <
	     kpatch_ss->contents.data + kpatch_ss->contents.size; kpatch++)
		show_ksplice_patch(kpatch_sect, kpatch);
	printf("\n");
}

int main(int argc, const char *argv[])
{
	assert(argc >= 1);
	bfd_init();
	ibfd = bfd_openr(argv[1], NULL);
	assert(ibfd);

	char **matching;
	assert(bfd_check_format_matches(ibfd, bfd_object, &matching));

	get_syms(ibfd, &isyms);

	asection *kreloc_sect = bfd_get_section_by_name(ibfd,
							".ksplice_relocs");
	if (kreloc_sect != NULL)
		show_ksplice_relocs(kreloc_sect);
	else
		printf("No ksplice relocations.\n\n");

	asection *ksize_sect = bfd_get_section_by_name(ibfd, ".ksplice_sizes");
	if (ksize_sect != NULL)
		show_ksplice_sizes(ksize_sect);
	else
		printf("No ksplice sizes.\n\n");

	asection *kpatch_sect = bfd_get_section_by_name(ibfd,
							".ksplice_patches");
	if (kpatch_sect != NULL)
		show_ksplice_patches(kpatch_sect);
	else
		printf("No ksplice patches.\n\n");

	return 0;
}
