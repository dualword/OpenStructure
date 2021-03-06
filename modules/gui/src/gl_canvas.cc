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
#ifndef _MSC_VER
#include <sys/time.h>
#endif

#include <ost/gui/python_shell/python_interpreter.hh>
#include "gl_canvas.hh"
#include "gl_win.hh"

#include <ost/log.hh>
#include <ost/dyn_cast.hh>
#include <ost/mol/view_op.hh>

#include <ost/seq/alignment_handle.hh>



#include <ost/gfx/scene.hh>
#include <ost/gfx/entity.hh>
#include <QTimer>
#include <QStatusBar>
#include <QApplication>
#include <QClipboard>
#include <QTime>
#include <QBasicTimer>
#include <QMouseEvent>
#include <QMenu>

#if QT_VERSION >= 0x040600
# include <QGesture>
#endif
#include "tools/tool_manager.hh"

namespace ost { namespace gui {

using gfx::Scene;

GLCanvas::GLCanvas(GLWin* gl_win,  QWidget* parent, const QGLFormat& f):
  QGLWidget(f,parent),
  glwin_(gl_win),
  mouse_key_mask_(),
  refresh_(true),
  master_timer_(),
  bench_flag_(false),
  last_pos_(),
  scene_menu_(NULL),
  show_beacon_(false),
  angular_speed_(0.0)
{
  if(!isValid()) return;
  master_timer_.start(10,this);
  setFocusPolicy(Qt::StrongFocus);
  setMouseTracking(true);
  scene_menu_=new SceneMenu();
  this->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(this, SIGNAL(customContextMenuRequested(const QPoint&)), this,
          SLOT(RequestContextMenu(const QPoint&)));
#if QT_VERSION >= 0x040600
  this->grabGesture(Qt::PinchGesture);
#endif
}

bool GLCanvas::event(QEvent* event)
{
#if QT_VERSION >= 0x040600
  if (event->type()==QEvent::Gesture) {
    return this->GestureEvent(static_cast<QGestureEvent*>(event));
  }
#endif
  return QGLWidget::event(event);
}


#if QT_VERSION >= 0x040600

bool GLCanvas::GestureEvent(QGestureEvent* event)
{
  if (QGesture* pinch=event->gesture(Qt::PinchGesture)) {
    QPinchGesture* pinch_gesture=static_cast<QPinchGesture*>(pinch);
    QPinchGesture::ChangeFlags changeFlags = pinch_gesture->changeFlags();
    if (changeFlags & QPinchGesture::RotationAngleChanged) {
      qreal value=pinch_gesture->rotationAngle();
      qreal lastValue=pinch_gesture->lastRotationAngle();
      this->OnTransform(gfx::INPUT_COMMAND_ROTZ, 0, gfx::TRANSFORM_VIEW, 
                  static_cast<Real>(value - lastValue));
      this->update();
      event->accept();
      if (pinch_gesture->state()==Qt::GestureFinished) {
        angular_speed_=value-lastValue;
        if (!gesture_timer_.isActive())
          gesture_timer_.start(10, this);
      }
      return true;
    }
  }
  return false;
}

#endif




void GLCanvas::MakeActive()
{
  makeCurrent();
}

void GLCanvas::DoRefresh()
{
  refresh_=true;
}

void GLCanvas::StatusMessage(const String& m)
{
  glwin_->StatusMessage(m);
}

void GLCanvas::OnTransform(gfx::InputCommand com, int indx, 
                           gfx::TransformTarget trg, Real val)
{
  // does not request a redraw on purpose, in order to chain OnTransform calls
  Scene::Instance().Apply(gfx::InputEvent(gfx::INPUT_DEVICE_MOUSE,com,indx,
                                          trg,val*0.5),false);
}

void GLCanvas::initializeGL()
{
  static bool init=false;
  if(!init) {
    init=true;
  }

  LOG_DEBUG("GLCanvas::initializeGL()");
  Scene::Instance().InitGL();
  LOG_DEBUG("GLCanvas::registering with scene");
  Scene::Instance().Register(this);
}

void GLCanvas::resizeGL(int w, int h)
{
  LOG_DEBUG("GLCanvas::resizeGL("<<w<<","<<h<<")");
  Scene::Instance().Resize(w,h);
}

void GLCanvas::paintGL()
{
  Scene::Instance().RenderGL();
  Tool* tool=ToolManager::Instance().GetActiveTool();
  if (tool) {
    tool->RenderGL();
  }
}

void GLCanvas::wheelEvent(QWheelEvent* event)
{
  OnTransform(gfx::INPUT_COMMAND_TRANSZ,0,gfx::TRANSFORM_VIEW,
              0.2*static_cast<Real>(-event->delta()));
}
bool GLCanvas::IsToolEvent(QInputEvent* event) const
{
  return event->modifiers() & Qt::ControlModifier;
}

MouseEvent::Buttons GLCanvas::TranslateButtons(Qt::MouseButtons buttons) const
{
  return MouseEvent::Buttons(buttons);
}
  
void GLCanvas::mouseMoveEvent(QMouseEvent* event)
{
  if (!(show_beacon_ || event->buttons())) {
    return;
  }
  if (this->IsToolEvent(event)) {
    if (ToolManager::Instance().GetActiveTool()) {
      MouseEvent mouse_event(this->TranslateButtons(event->buttons()), 
                             last_pos_, QPoint(event->x(), event->y()),
                             event->modifiers());
      ToolManager::Instance().GetActiveTool()->MouseMove(mouse_event);
      event->accept();      
    }
  } else {
    this->HandleMouseMoveEvent(event);
  }
  last_pos_=QPoint(event->x(), event->y());
  this->DoRefresh();
}

void GLCanvas::mousePressEvent(QMouseEvent* event)
{
  if (this->IsToolEvent(event)) {
    if (ToolManager::Instance().GetActiveTool()) {
      MouseEvent mouse_event(this->TranslateButtons(event->button()), 
                             QPoint(event->x(), event->y()),
                             QPoint(event->x(), event->y()),
                             event->modifiers());
      ToolManager::Instance().GetActiveTool()->MousePress(mouse_event);
    }
  } else {
    this->HandleMousePressEvent(event);
  }
  last_pos_=QPoint(event->x(), event->y());
  this->DoRefresh();
}

void GLCanvas::mouseReleaseEvent(QMouseEvent* event)
{
  if (this->IsToolEvent(event)) {
    if (ToolManager::Instance().GetActiveTool()) {
      MouseEvent mouse_event(this->TranslateButtons(event->button()), 
                             QPoint(event->x(), event->y()),
                             QPoint(event->x(), event->y()),
                             event->modifiers());
      ToolManager::Instance().GetActiveTool()->Click(mouse_event);
      event->accept();      
    } else {
      this->HandleMouseReleaseEvent(event);
    }
  }
  last_pos_=QPoint(event->x(), event->y());
  this->DoRefresh();
}

void GLCanvas::mouseDoubleClickEvent(QMouseEvent* event)
{
  if (this->IsToolEvent(event)) {
    if (ToolManager::Instance().GetActiveTool()) {
      MouseEvent mouse_event(this->TranslateButtons(event->button()), 
                             QPoint(event->x(), event->y()),
                             QPoint(event->x(), event->y()),
                             event->modifiers());
      ToolManager::Instance().GetActiveTool()->DoubleClick(mouse_event);
      event->accept();
    }
  } else {
    gfx::Scene& scene=gfx::Scene::Instance();
    mol::AtomHandle atom=scene.PickAtom(event->x(), 
                                        size().height()-event->y()).second;
    if (atom.IsValid()) {
      scene.SetCenter(atom.GetPos());
    }
  }
  if (show_beacon_) {
    this->DoRefresh();
  }
}

void GLCanvas::RequestContextMenu(const QPoint& pos)
{
  QPoint pick_pos(pos.x(),this->height()-pos.y());
  scene_menu_->SetPickPoint(pick_pos);
  scene_menu_->ShowMenu(this->mapToGlobal(pos));
}

void GLCanvas::HandleMousePressEvent(QMouseEvent* event)
{
  gfx::Scene& scene=gfx::Scene::Instance();
  scene.Pick(event->x(), scene.GetViewport().height-event->y(), 0);
  event->accept();
}

void GLCanvas::HandleMouseReleaseEvent(QMouseEvent* event)
{
  event->accept();  
}

void GLCanvas::HandleMouseMoveEvent(QMouseEvent* event)
{
  int indx=0;
  gfx::TransformTarget trg=gfx::TRANSFORM_VIEW;  

  if(show_beacon_) {
    Scene::Instance().SetBeacon(event->x(),size().height()-event->y());
  }

  QPoint delta=QPoint(event->x(), event->y())-last_pos_;
  if (event->buttons() & Qt::LeftButton) {
    if (event->buttons() & Qt::MidButton) {
      if (event->modifiers() & Qt::ShiftModifier) {
        OnTransform(gfx::INPUT_COMMAND_SLABN,indx,
                    trg,0.5*static_cast<Real>(-delta.x()));
        OnTransform(gfx::INPUT_COMMAND_SLABF,indx,
                    trg,0.5*static_cast<Real>(delta.x()));

      } else {
        OnTransform(gfx::INPUT_COMMAND_ROTZ, indx, trg, 
                    static_cast<Real>(delta.x()+delta.y()));
      }

    } else {
      if (event->modifiers() & Qt::ShiftModifier) {
        OnTransform(gfx::INPUT_COMMAND_TRANSX,indx,
                    trg,static_cast<Real>(delta.x())); // translate along x
        OnTransform(gfx::INPUT_COMMAND_TRANSY,indx,
                    trg,static_cast<Real>(-delta.y())); // translate along y
      } else {
        OnTransform(gfx::INPUT_COMMAND_ROTY,indx,trg,
                    static_cast<Real>(delta.x())); // rotation around y
        OnTransform(gfx::INPUT_COMMAND_ROTX,indx,trg,
                    static_cast<Real>(delta.y())); // rotation around x
      }

    }
  } else if (event->buttons() & Qt::MidButton) {
      if (event->modifiers() & Qt::ShiftModifier) {
        OnTransform(gfx::INPUT_COMMAND_SLABN,indx,trg,
                    0.5*static_cast<Real>(-delta.x()));
        OnTransform(gfx::INPUT_COMMAND_SLABF,indx,trg,
                    0.5*static_cast<Real>(-delta.x()));
      } else {
        OnTransform(gfx::INPUT_COMMAND_TRANSZ,indx,
                    trg,static_cast<Real>(delta.y())); // translate along z
      }
  }
  event->accept();
}


void GLCanvas::CopySelectionToClipboard()
{
  gfx::EntityP ent;

  // TODO: this needs a re-write based on the new GfxNodeVisitor
#if 0
  // find first entity connect with non-empty selection
  gfx::GfxObjList& obj=gfx::Scene::Instance().SceneObjects();
  for (gfx::GfxObjList::iterator i=obj.begin(), e=obj.end(); i!=e; ++i) {
    if (ent=dyn_cast<gfx::Entity>(*i)) {
      if (ent->GetView().GetAtomCount()>0) {
        break;        
      } else {
        ent=gfx::EntityP();
      }
    }
  }
#endif

  if (!ent)
    return;
    
  mol::EntityView sel=ent->GetSelection();
  mol::EntityView view=ent->GetView();
  mol::ChainViewList chains=view.GetChainList();
  QString pretty;
  for (mol::ChainViewList::iterator c=chains.begin(), ec=chains.end(); c!=ec; ++c) {
    mol::ResidueViewList res=c->GetResidueList();
    std::stringstream seq1;
    std::stringstream seq2;    
    for (mol::ResidueViewList::iterator r=res.begin(), er=res.end(); r!=er; ++r) {
      mol::ResidueView rv=*r;      
      if (!rv.IsPeptideLinking() || rv.GetOneLetterCode()=='?')
        continue;      
      seq1 << rv.GetOneLetterCode();
      if (sel.FindResidue(rv.GetHandle()).IsValid()) {
        seq2 << rv.GetOneLetterCode();
      } else {
        seq2 << '-';
      }        
    }
    seq::AlignmentHandle a=seq::CreateAlignment();
    a.AddSequence(seq::CreateSequence(ent->GetName(), seq1.str()));
    a.AddSequence(seq::CreateSequence(ent->GetName()+" SEL", seq2.str()));
    pretty+=QString(a.ToString(80).c_str());
  }
  QClipboard *clipboard = QApplication::clipboard();
  clipboard->setText(pretty);
}

void GLCanvas::keyPressEvent(QKeyEvent* event)
{
  if(event->key()==Qt::Key_Space) {
    show_beacon_=true;
    Scene::Instance().SetBeacon(last_pos_.x(),size().height()-last_pos_.y());
    DoRefresh();
    setCursor(Qt::BlankCursor);
  }
  if((event->modifiers() & Qt::ControlModifier)) {
    // Ctrl pressed
    if(event->key()==Qt::Key_A) {
      OnTransform(gfx::INPUT_COMMAND_AUTOSLAB,0,gfx::TRANSFORM_VIEW,0.0);
      return;
    } else if(event->key()==Qt::Key_F) {
      OnTransform(gfx::INPUT_COMMAND_TOGGLE_FOG,0,gfx::TRANSFORM_VIEW,0.0);
      return;
    } else if(event->key()==Qt::Key_B) {
      bench_flag_=!bench_flag_;
      return;
    } else if (event->key()==Qt::Key_C) {
      this->CopySelectionToClipboard();
      return;
    } else if(event->key()==Qt::Key_1) {
      gfx::Scene::Instance().SetShadingMode("fallback");
      DoRefresh();
      return;
    } else if(event->key()==Qt::Key_2) {
      gfx::Scene::Instance().SetShadingMode("basic");
      DoRefresh();
      return;
    } else if(event->key()==Qt::Key_3) {
      gfx::Scene::Instance().SetShadingMode("default");
      DoRefresh();
      return;
    } else if(event->key()==Qt::Key_4) {
      gfx::Scene::Instance().SetShadingMode("hf");
      DoRefresh();
      return;
    } else if(event->key()==Qt::Key_5) {
      gfx::Scene::Instance().SetShadingMode("toon1");
      DoRefresh();
      return;
    } else if(event->key()==Qt::Key_6) {
      gfx::Scene::Instance().SetShadingMode("toon2");
      DoRefresh();
      return;
    } else if(event->key()==Qt::Key_0) {
      gfx::Scene::Instance().SetShadow(!gfx::Scene::Instance().GetShadow());
      DoRefresh();
      return;
    } else if(event->key()==Qt::Key_9) {
      gfx::Scene::Instance().SetAmbientOcclusion(!gfx::Scene::Instance().GetAmbientOcclusion());
      DoRefresh();
      return;
    } else if(event->key()==Qt::Key_Equal) {
      if(gfx::Scene::Instance().GetStereoMode()>0) {
        gfx::Scene::Instance().SetStereoMode(0);
      } else {
        gfx::Scene::Instance().SetStereoMode(1);
      }
      DoRefresh();
      return;
    }    
  }
  event->ignore();
}

void GLCanvas::keyReleaseEvent(QKeyEvent* event)
{
  if(event->key()==Qt::Key_Space) {
    show_beacon_=false;
    Scene::Instance().SetBeaconOff();
    DoRefresh();
    setCursor(Qt::ArrowCursor);
    return;
  }
  if(event->key()==Qt::Key_Alt){
    emit ReleaseFocus();
    return;
  }
  QGLWidget::keyReleaseEvent(event);
}

#ifndef _MSC_VER
Real delta_time(const timeval& t1, const timeval& t2)
{
  return 1e-6*static_cast<Real>((t2.tv_sec*1e6+t2.tv_usec)-(t1.tv_sec*1e6+t1.tv_usec));
}
#endif

void GLCanvas::timerEvent(QTimerEvent * event)
{

#if QT_VERSION>= 0x040600
  // gesture support
  if (gesture_timer_.timerId()==event->timerId()) {
    if (angular_speed_!=0.0) {
      angular_speed_*=0.95;
      this->OnTransform(gfx::INPUT_COMMAND_ROTZ, 0, gfx::TRANSFORM_VIEW, 
                        static_cast<Real>(angular_speed_));
      if (std::abs(angular_speed_)<0.001) {
        angular_speed_=0.0;
        gesture_timer_.stop();
      }
      this->update();
    }
    return;
  }
#endif
#ifndef _MSC_VER
  static struct timeval time0,time1;
  static int count=0;
#endif
  if(refresh_ || bench_flag_) {
    refresh_=false;
    master_timer_.stop();
    this->updateGL();
    master_timer_.start(10,this);
  }

#ifndef _MSC_VER
  if(bench_flag_) {
    ++count;
    if(count==20) {
      count=0;
      gettimeofday(&time1,NULL);
      LOG_ERROR(20.0/delta_time(time0,time1) << " fps");
      gettimeofday(&time0,NULL);
    }
  }
#endif
}

void GLCanvas::SetTestMode(bool f)
{
  Scene::Instance().SetTestMode(f);
}

}} // ns
