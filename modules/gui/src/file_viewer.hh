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
#ifndef OST_GUI_FILE_VIEWER_HH
#define OST_GUI_FILE_VIEWER_HH


#include <ost/gui/module_config.hh>


#include <QWidget>

class QTextEdit;
class QSyntaxHighlighter;

namespace ost { namespace gui {

/// \brief a simplistic file viewer
class DLLEXPORT_OST_GUI FileViewer : public QWidget {
  Q_OBJECT
public:
  FileViewer(const QString& path=QString(), QWidget* parent=NULL);
  
  QTextEdit* text_viewer_;
  QSyntaxHighlighter* highlighter_;
};

}}

#endif
