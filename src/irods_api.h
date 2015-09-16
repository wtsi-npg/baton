/**
 * Copyright (C) 2015 Genome Research Ltd. All rights reserved.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * @file irods_api.h
 * @author Keith James <kdj@sanger.ac.uk>
 */

#ifndef _IRODS_API_H
#define _IRODS_API_H

#include "config.h"

#if HAVE_IRODS3
#include "irods_3_x_x.h"
#elif HAVE_IRODS4
#include "irods_4_1_x.h"
#endif

#endif // _IRODS_API_H
