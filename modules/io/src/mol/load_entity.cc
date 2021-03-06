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
#include <ost/log.hh>
#include <ost/conop/conop.hh>

#include "load_entity.hh"
#include <ost/mol/xcs_editor.hh>
#include <ost/io/io_manager.hh>
#include <ost/io/mol/entity_io_handler.hh>
#include <ost/profile.hh>

namespace ost { namespace io {

namespace {

void Import(mol::EntityHandle& eh, const String& filename, int flag)
{
  Profile profile_import("import");
  LOG_DEBUG("creating EntityIOHandle for " << filename);
  EntityIOHandlerP ent_io = IOManager::Instance().FindEntityImportHandler(filename);

  // TODO: proper error handling

  LOG_DEBUG("calling import on entity io handle");
  /*
    This should probably allow various parameters to be passed
    to adjust the loading behaviour for a particular filter.
    Alternatively, these settings could be done outside via
    the main IOManager interface, as global settings per import plugin
  */
  ent_io->Import(eh,filename);

  LOG_DEBUG("running conopology");

  if(ent_io->RequiresBuilder()) {
    conop::BuilderP builder = conop::Conopology::Instance().GetBuilder();
    conop::Conopology::Instance().ConnectAll(builder,eh,flag);
  }
}

} // anon ns

mol::EntityHandle LoadEntity(const String& filename, int flag)
{
  LOG_DEBUG("creating emtpy entity");
  mol::EntityHandle eh=mol::CreateEntity();
  mol::XCSEditor xcs_lock=eh.EditXCS(mol::BUFFERED_EDIT);
  Import(eh,filename,flag);
  return eh;
}

}} // ns

