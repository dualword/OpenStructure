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

#include <boost/python.hpp>
#include <ost/config.hh>
#include <ost/mol/alg/local_dist_test.hh>
#include <ost/mol/alg/superpose_frames.hh>
#include <ost/mol/alg/filter_clashes.hh>
using namespace boost::python;
using namespace ost;

void export_svdSuperPose();
void export_TrajectoryAnalysis();
void export_StructureAnalysis();
void export_Clash();
#if OST_IMG_ENABLED
void export_entity_to_density();
#endif

namespace {
  
Real (*ldt_a)(const mol::EntityView&, const mol::EntityView& ref, Real, Real, const String&)=&mol::alg::LocalDistTest;
Real (*ldt_b)(const seq::AlignmentHandle&,Real, Real, int, int)=&mol::alg::LocalDistTest;
mol::EntityView (*fc_a)(const mol::EntityView&, Real,bool)=&mol::alg::FilterClashes;
mol::EntityView (*fc_b)(const mol::EntityHandle&, Real, bool)=&mol::alg::FilterClashes;
mol::CoordGroupHandle (*superpose_frames1)(mol::CoordGroupHandle&, mol::EntityView&, int, int, int)=&mol::alg::SuperposeFrames;
mol::CoordGroupHandle (*superpose_frames2)(mol::CoordGroupHandle&,  mol::EntityView&, mol::EntityView&, int, int)=&mol::alg::SuperposeFrames;
}



BOOST_PYTHON_MODULE(_ost_mol_alg)
{
  export_svdSuperPose();
  export_TrajectoryAnalysis();
  export_StructureAnalysis();
  #if OST_IMG_ENABLED
  export_entity_to_density();
  #endif
  
  def("LocalDistTest", ldt_a, (arg("local_ldt_property_string")=""));
  def("LocalDistTest", ldt_b, (arg("ref_index")=0, arg("mdl_index")=1));
  def("FilterClashes", fc_a, (arg("ent"), arg("tolerance")=0.1, arg("always_remove_bb")=false));
  def("FilterClashes", fc_b, (arg("ent"), arg("tolerance")=0.1, arg("always_remove_bb")=false));
  def("SuperposeFrames", superpose_frames1, 
      (arg("source"), arg("sel")=ost::mol::EntityView(), arg("begin")=0, 
       arg("end")=-1, arg("ref")=-1));
  def("SuperposeFrames", superpose_frames2, 
  (arg("source"), arg("sel")=ost::mol::EntityView(), arg("ref_view")=ost::mol::EntityView(),arg("begin")=0, 
   arg("end")=-1));
}
