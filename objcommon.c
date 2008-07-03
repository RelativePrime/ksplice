/*  Copyright (C) 2008  Jeffrey Brian Arnold <jbarnold@mit.edu>
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

#include "objcommon.h"

void vec_do_reserve(void **data, size_t *mem_size, size_t new_size)
{
	if (new_size > *mem_size || new_size * 2 < *mem_size) {
		if (new_size < *mem_size * 2)
			new_size = *mem_size * 2;
		*data = realloc(*data, new_size);
		assert(new_size == 0 || *data != NULL);
		*mem_size = new_size;
	}
}

long get_syms(bfd *abfd, asymbol ***syms_ptr)
{
	long storage_needed = bfd_get_symtab_upper_bound(abfd);
	if (storage_needed == 0)
		return 0;
	assert(storage_needed >= 0);

	*syms_ptr = (asymbol **)malloc(storage_needed);
	long num_syms = bfd_canonicalize_symtab(abfd, *syms_ptr);
	assert(num_syms >= 0);

	return num_syms;
}

struct supersect *fetch_supersect(bfd *abfd, asection *sect, asymbol **sympp)
{
	static struct supersect *supersects = NULL;

	struct supersect *ss;
	for (ss = supersects; ss != NULL; ss = ss->next) {
		if (strcmp(sect->name, ss->name) == 0 && ss->parent == abfd)
			return ss;
	}

	struct supersect *new = malloc(sizeof(*new));
	new->parent = abfd;
	new->name = malloc(strlen(sect->name) + 1);
	strcpy(new->name, sect->name);
	new->next = supersects;
	supersects = new;

	vec_init(&new->contents);
	vec_resize(&new->contents, bfd_get_section_size(sect));
	assert(bfd_get_section_contents
	       (abfd, sect, new->contents.data, 0, new->contents.size));
	new->alignment = 1 << bfd_get_section_alignment(abfd, sect);

	vec_init(&new->relocs);
	vec_reserve(&new->relocs, bfd_get_reloc_upper_bound(abfd, sect));
	vec_resize(&new->relocs,
		   bfd_canonicalize_reloc(abfd, sect, new->relocs.data, sympp));
	assert(new->relocs.size >= 0);

	return new;
}

int label_offset(const char *sym_name)
{
	int i;
	for (i = 0;
	     sym_name[i] != 0 && sym_name[i + 1] != 0 && sym_name[i + 2] != 0
	     && sym_name[i + 3] != 0; i++) {
		if (sym_name[i] == '_' && sym_name[i + 1] == '_'
		    && sym_name[i + 2] == '_' && sym_name[i + 3] == '_')
			return i + 4;
	}
	return -1;
}

const char *only_label(const char *sym_name)
{
	int offset = label_offset(sym_name);
	if (offset == -1)
		return NULL;
	return &sym_name[offset];
}

const char *dup_wolabel(const char *sym_name)
{
	int offset, entire_strlen, label_strlen, new_strlen;
	char *newstr;

	offset = label_offset(sym_name);
	if (offset == -1)
		label_strlen = 0;
	else
		label_strlen = strlen(&sym_name[offset]) + strlen("____");

	entire_strlen = strlen(sym_name);
	new_strlen = entire_strlen - label_strlen;
	newstr = malloc(new_strlen + 1);
	memcpy(newstr, sym_name, new_strlen);
	newstr[new_strlen] = 0;
	return newstr;
}
