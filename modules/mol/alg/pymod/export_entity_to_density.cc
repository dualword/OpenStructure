//------------------------------------------------------------------------------
// This file is part of the OpenStructure project <www.openstructure.org>
//
// Copyright (C) 2008-2010 by the OpenStructure authors
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

/*
 * Author Juergen Haas
 */
#include <boost/python.hpp>

#include <ost/mol/alg/entity_to_density.hh>

using namespace boost::python;
using namespace ost;
using namespace ost::mol::alg;


#if OST_IMG_ENABLED

#include <ost/mol/alg/entity_to_density.hh>

//"thin wrappers" for default parameters
int etd1(const mol::EntityView& entity_view,
         img::MapHandle& map,
         const DensityType& density_type,
         Real falloff_start,
         Real falloff_end,
         bool clear_map_flag)
{
  EntityToDensity(entity_view, map, density_type, falloff_start,
                  falloff_end, clear_map_flag);
                  return 0;
}

int etd2(const mol::EntityView& entity_view,
        img::MapHandle& map,
        const DensityType& density_type,
        Real falloff_start,
        Real falloff_end)
{
  EntityToDensity(entity_view, map, density_type, falloff_start,
                  falloff_end);
                  return 0;
}


void export_entity_to_density()
{

    def("EntityToDensity",EntityToDensity);
    def("EntityToDensity",etd1);
    def("EntityToDensity",etd2);

    enum_<DensityType>("DensityType")
    .value("SCATTERING_FACTORS", SCATTERING_FACTORS)
    .value("ROSETTA_HIGH_RESOLUTION", ROSETTA_HIGH_RESOLUTION)
    .value("ROSETTA_LOW_RESOLUTION", ROSETTA_LOW_RESOLUTION)
    .export_values()
    ;
 }
#endif
