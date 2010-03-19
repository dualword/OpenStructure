//------------------------------------------------------------------------------
// This file is part of the OpenStructure project <www.openstructure.org>
//
// Copyright (C) 2008-2010 by the OpenStructure authors
// Copyright (C) 2003-2010 by the IPLT authors
//
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
#ifndef DX_IO_MAP_IO_PLUGIN_OST_HH
#define DX_IO_MAP_IO_PLUGIN_OST_HH

#include "map_io_handler.hh"

namespace ost { namespace io {

class DLLEXPORT_OST_IO DX: public ImageFormatBase
{
 public:

  DX(bool normalize_on_save = false);

  bool GetNormalizeOnSave() const;
  void SetNormalizeOnSave(bool normalize_on_save);
  static String FORMAT_STRING;
  static String FORMAT_NAME_STRING;
  static String FORMAT_DESCRIPTION_STRING;

 private:

  bool normalize_on_save_;
};

class DLLEXPORT_OST_IO MapIODxHandler: public MapIOHandler
{
 public:
  /// \brief Map IO handler to read/write APBS map files (in OpenDX format)
  ///
  /// This map IO handler reads and writes OpenDX formatted map files, as
  /// generated by APBS for example. Only the information needed to construct
  /// an OpenStructure map are read from the file (i.e. grid size (in x,y,z), 
  /// grid spacing (in x,y,z) grid origin (in x,y,z) and the actual values for
  /// each grid point. The other lines are ignored. Thus, when writing the
  /// grid to a file, not exactly the same file is produced.
  virtual void Import(img::MapHandle& sh, const boost::filesystem::path& loc,
                      const ImageFormatBase& formatstruct);
  virtual void Import(img::MapHandle& sh, std::istream& loc,const ImageFormatBase& formatstruct);
  virtual void Export(const img::MapHandle& sh,
                      const boost::filesystem::path& loc,const ImageFormatBase& formatstruct) const;
  virtual void Export(const img::MapHandle& sh,
                      std::ostream& loc, const ImageFormatBase& formatstruct) const;
  static bool MatchContent(unsigned char* header);
  static bool MatchType(const ImageFormatBase& type);
  static bool MatchSuffix(const boost::filesystem::path& locx);
  static String GetFormatName() { return String("Dx"); };
  static String GetFormatDescription()  { return String("Format used by the OpenDX software package"); };

};

typedef MapIOHandlerFactory<MapIODxHandler> MapIODxHandlerFactory;

}} // ns

#endif
