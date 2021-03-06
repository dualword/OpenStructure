//------------------------------------------------------------------------------
// This file is part of the OpenStructure project <www.openstructure.org>
//
// Copyright (C) 2008-2011 by the OpenStructure authors
//
// This library is free software; you can redistribute it and/or modify it under
// the terms of the GNU Lesser General Public License as published by the Free
// Software Foundation; either version 3.0 of the License, or (at your option)
// any later version.
// This library is distributed in the hope that it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for more
// details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this library; if not, write to the Free Software Foundation, Inc.,
// 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
//------------------------------------------------------------------------------
#ifndef OST_IMG_ALG_MODULE_CONFIG_HH
#define OST_IMG_ALG_MODULE_CONFIG_HH

#include <ost/dllexport.hh>

#if defined(OST_MODULE_OST_IMG_ALG)
#  define DLLEXPORT_IMG_ALG DLLEXPORT 
#  if defined(_MSC_VER)
#    define OST_IMG_ALG_EXPLICIT_INST_DECL(c, t)
#  else
#    define OST_IMG_ALG_EXPLICIT_INST_DECL(c, t) extern template TEMPLATE_EXPORT c ost::img::image_state::t;
#  endif
#else
#  define DLLEXPORT_IMG_ALG DLLIMPORT
#  define OST_IMG_ALG_EXPLICIT_INST_DECL(c, t) extern template c DLLIMPORT ost::img::image_state::t;
#endif
#endif
