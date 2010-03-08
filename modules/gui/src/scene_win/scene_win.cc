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
 Authors: Marco Biasini, Ansgar Philippsen, Stefan Scheuber
 */
#include <QVBoxLayout>
#include <QHeaderView>

#include <ost/gui/widget_registry.hh>
#include <ost/gui/gosty_app.hh>
#include <ost/gui/scene_selection.hh>

#include "scene_win.hh"

namespace ost { namespace gui {

class SceneWinFactory: public WidgetFactory {
public:
  SceneWinFactory() :
    WidgetFactory("ost::gui::SceneWin", "Scene Menu") {
  }

  virtual Widget* Create(QWidget* parent) {
    return GostyApp::Instance()->GetSceneWin();
  }
};

OST_REGISTER_WIDGET(SceneWin, SceneWinFactory);

SceneWin::SceneWin(QWidget* parent) :
  Widget(NULL, parent) {
  QVBoxLayout* layout = new QVBoxLayout(this);
  layout->setMargin(0);
  layout->setSpacing(0);
  model_ = new SceneWinModel(this);
  view_ = new QTreeView(this);
  context_menu_ = new ContextMenu(view_,model_);
  view_->setAttribute(Qt::WA_MacShowFocusRect, false);
  view_->header()->hide();
  view_->setContextMenuPolicy(Qt::CustomContextMenu);
  view_->setModel(model_);
  view_->setSelectionBehavior(QAbstractItemView::SelectRows);
  view_->setSelectionMode(QAbstractItemView::ExtendedSelection);
  view_->expandAll();

  layout->addWidget(view_);
  connect(view_->selectionModel(),
      SIGNAL(selectionChanged(const QItemSelection&,
              const QItemSelection&)), this,
      SLOT(OnSelectionChange(const QItemSelection&,
              const QItemSelection&)));
  connect(view_, SIGNAL(customContextMenuRequested(const QPoint&)), this,
      SLOT(ContextMenuRequested(const QPoint&)));
  connect(model_, SIGNAL(rowsInserted(const QModelIndex&, int, int)), this, SLOT(RowsInserted(const QModelIndex&, int, int)));
}

void SceneWin::ContextMenuRequested(const QPoint& pos) {
  if (view_->indexAt(pos).isValid()) {
    context_menu_->ShowMenu(view_->viewport()->mapToGlobal(pos));
  }
}

void SceneWin::OnSelectionChange(const QItemSelection& sel,
    const QItemSelection& desel) {
  QItemSelectionModel* model = view_->selectionModel();
  QModelIndexList indexes = model->selection().indexes();
  gfx::NodePtrList selected_nodes = model_->GetGfxNodes(indexes);
  mol::QueryViewWrapperList selected_views = model_->GetQueryViewsList(indexes);
  gfx::EntityP entity = model_->GetEntityOfViews(indexes);
  if(entity)
    emit this->ActiveNodesChanged(selected_nodes,entity,selected_views);
  else
    emit this->ActiveNodesChanged(selected_nodes,entity,mol::QueryViewWrapperList());

  emit this->ActiveNodesChanged();
}

SceneWinModel* SceneWin::GetModel() const{
  return model_;
}

void SceneWin::Update() {
  model_->Update();
  view_->viewport()->update();
}

void SceneWin::RowsInserted(const QModelIndex & parent, int start, int end){
  QItemSelectionModel* model = view_->selectionModel();
  QModelIndexList indexes = model->selection().indexes();
  if(indexes.isEmpty()){
    model->select(parent.child(start,0),QItemSelectionModel::ClearAndSelect|QItemSelectionModel::Rows);
  }
}


void SceneWin::AddView(gfx::EntityP entity, mol::EntityView view){

}


SceneWin::~SceneWin() {
}

}
} // ns
