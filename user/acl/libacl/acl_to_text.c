/*
  File: acl_to_text.c

  Copyright (C) 1999, 2000
  Andreas Gruenbacher, <a.gruenbacher@computer.org>

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU Library General Public
  License as published by the Free Software Foundation; either
  version 2 of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Library General Public License for more details.

  You should have received a copy of the GNU Library General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/

#include <stdio.h>
#include <errno.h>
#include <acl/libacl.h>
#include "libacl.h"

char *
acl_to_text(acl_t acl, ssize_t *len_p)
{
	return __acl_to_any_text(acl, len_p, NULL, '\n', "\n",
				 TEXT_SOME_EFFECTIVE);
}

