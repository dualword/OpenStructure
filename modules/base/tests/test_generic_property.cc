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
#include <ost/mol/mol.hh>
#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>


using namespace ost;
using namespace ost::mol;

BOOST_AUTO_TEST_SUITE( base )

BOOST_AUTO_TEST_CASE( test_generic_property )
{
  EntityHandle eh=CreateEntity();
  XCSEditor editor=eh.EditXCS();  
  ChainHandle ch=editor.InsertChain("A");
  ResidueHandle res = editor.AppendResidue(ch, "X");
  AtomHandle atom = editor.InsertAtom(res, "Y",geom::Vec3());
  AtomHandle atom2 = editor.InsertAtom(res, "Y",geom::Vec3());
  BondHandle bond = editor.Connect(atom, atom2);

  eh.SetStringProp("a","123");
  ch.SetFloatProp("b",1.5);
  res.SetIntProp("c",123);
  atom.SetBoolProp("d",true);
  bond.SetIntProp("e", 12);

  BOOST_CHECK(eh.HasProp("x")==false);
  BOOST_CHECK(eh.HasProp("a")==true);

  BOOST_CHECK(eh.GetStringProp("a")=="123");
  BOOST_CHECK(ch.GetFloatProp("b")==1.5);
  BOOST_CHECK(res.GetIntProp("c")==123);
  BOOST_CHECK(atom.GetBoolProp("d")==true);
  BOOST_CHECK(bond.GetIntProp("e")==12);

  EntityView ev = eh.CreateFullView();
  ChainView chv = ev.FindChain("A");
  ResidueView resv = ev.FindResidue(res);
  AtomView atomv = ev.FindAtom(atom);

  BOOST_CHECK(ev.GetStringProp("a")=="123");
  BOOST_CHECK(chv.GetFloatProp("b")==1.5);
  BOOST_CHECK(resv.GetIntProp("c")==123);
  BOOST_CHECK(atomv.GetBoolProp("d")==true);
}

BOOST_AUTO_TEST_SUITE_END()
