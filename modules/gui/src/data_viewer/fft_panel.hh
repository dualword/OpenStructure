//------------------------------------------------------------------------------
// This file is part of the OpenStructure project <www.openstructure.org>
//
// Copyright (C) 2008-2011 by the OpenStructure authors
// Copyright (C) 2003-2010 by the IPLT authors
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

#ifndef OST_FFT_PANEL_HH_
#define OST_FFT_PANEL_HH_
/*
  Author: Andreas Schenk
*/

#include <ost/gui/module_config.hh>
#include "data_viewer_panel_base.hh"

namespace ost { namespace img { namespace gui {

class DLLEXPORT_OST_GUI FFTPanel: public DataViewerPanelBase
{
Q_OBJECT

public:
  FFTPanel(const Data& parent_data, QWidget* parent);
  virtual ~FFTPanel();
  void SetFFTSize(unsigned int size);
  unsigned int GetFFTSize();
  virtual void ObserverUpdate();

public slots:
  void SetPosition(const QPointF& p);
  void ShowSizeDialog();

protected:
  void update_fft();
  unsigned int size_;
  Point parent_position_;
  const Data& parent_data_;
  ImageHandle fft_data_;
};

}}}  //ns

#endif /*FFT_PANEL_HH_*/
