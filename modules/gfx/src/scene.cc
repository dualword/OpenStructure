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
/*
  Author: Ansgar Philippsen
*/

#include <ost/message.hh>
#include <sstream>
#include <limits>
#include <algorithm>
#include <cstring>

#ifndef _MSC_VER
#include <sys/time.h>
#endif

#include <boost/bind.hpp>
#include <boost/shared_array.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/fstream.hpp>
#include <ost/platform.hh>
#include <ost/log.hh>
#include <ost/dyn_cast.hh>
#include <ost/geom/geom.hh>

#include "gl_helper.hh"

#include <ost/config.hh>

#include "scene.hh"
#include "input.hh"
#include "gfx_node.hh"
#include "gfx_object.hh"
#include "gfx_test_object.hh"
#include "bitmap_io.hh"
#include "entity.hh"
#include "exporter.hh"
#include "povray.hh"
#include "offscreen_buffer.hh"

#if OST_SHADER_SUPPORT_ENABLED
# include "shader.hh"
# include "impl/scene_fx.hh"
#endif

using boost::bind;

namespace ost {

using namespace geom;

namespace gfx {

namespace bf = boost::filesystem;

#ifndef uchar
#define uchar unsigned char
#endif

namespace {

Mat3 calc_light_rot(const Vec3& ldir)
{
  Mat3 nrvo=AxisRotation(Cross(-ldir,Vec3(0,0,-1)),
                                     -acos(Dot(ldir,Vec3(0,0,-1))));
  return nrvo;
}

}

// meyers singleton
Scene& Scene::Instance()
{
  static Scene inst;
  return inst;
}

Scene::Scene():
  win_(0),
  root_node_(new GfxNode("Scene")),
  observers_(),
  transform_(),
  gl_init_(false),
  fov_(30.0),
  znear_(1.0),zfar_(1000.0),
  fnear_(0.0), ffar_(0.0),
  vp_width_(1000),vp_height_(1000),
  scene_view_stack_(),
  aspect_ratio_(1.0),
  background_(Color(0.0,0.0,0.0,0.0)),
  light_dir_(Normalize(Vec3(-2,-1,-5))),
  light_rot_(calc_light_rot(light_dir_)),
  light_amb_(0.1,0.1,0.1),
  light_diff_(0.9,0.9,0.9),
  light_spec_(0.5,0.5,0.5),
  cor_flag_(false),
  fix_cor_flag_(true),
  fog_flag_(true),
  fog_color_(0.0,0.0,0.0,0.0),
  auto_autoslab_(true),
  do_autoslab_(true),
  do_autoslab_fast_(true),
  offscreen_flag_(false),
  main_offscreen_buffer_(0),
  old_vp_(),
  def_shading_mode_("default"),
  selection_mode_(1),
  test_flag_(false),
  tmp_tex_(),
  glyph_tex_id_(0),
  glyph_map_(),
  def_text_size_(32.0),
  blur_count_(0),
  blur_buffer_(),
  stereo_mode_(0),
  stereo_alg_(0),
  stereo_inverted_(false),
  stereo_eye_(0),
  stereo_iod_(4.0),
  stereo_distance_(0.0),
  scene_left_tex_(),
  scene_right_tex_()
{
  transform_.SetTrans(Vec3(0,0,-100));
}

void Scene::SetFog(bool f)
{
  fog_flag_=f;
  if(!gl_init_) return;
  if(f) {
    glEnable(GL_FOG);
  } else {
    glDisable(GL_FOG);
  }
  RequestRedraw();
}

bool Scene::GetFog() const
{
  return fog_flag_;
}

void Scene::SetFogColor(const Color& c)
{
  GLfloat fogc[]={c.Red(),c.Green(),c.Blue(),1.0};
  fog_color_=c;
  if(!gl_init_) return;
  glFogfv(GL_FOG_COLOR,fogc);
  RequestRedraw();
}

Color Scene::GetFogColor() const
{
  return fog_color_;
}

void Scene::SetShadow(bool f)
{
#if OST_SHADER_SUPPORT_ENABLED
  impl::SceneFX::Instance().shadow_flag=f;
  // the redraw routine will deal with the Shader
  RequestRedraw();
#endif
}

bool Scene::GetShadow() const
{
#if OST_SHADER_SUPPORT_ENABLED
  return impl::SceneFX::Instance().shadow_flag;
#else
  return false;
#endif
}

void Scene::SetShadowQuality(int q)
{
#if OST_SHADER_SUPPORT_ENABLED
  impl::SceneFX::Instance().shadow_quality=std::min(8,std::max(0,q));
  RequestRedraw();
#endif
}

int Scene::GetShadowQuality() const
{
#if OST_SHADER_SUPPORT_ENABLED
  return impl::SceneFX::Instance().shadow_quality;
#else
  return 0;
#endif
}

void Scene::SetShadowWeight(float w)
{
#if OST_SHADER_SUPPORT_ENABLED
  impl::SceneFX::Instance().shadow_weight=w;
  RequestRedraw();
#endif
}

float Scene::GetShadowWeight() const
{
#if OST_SHADER_SUPPORT_ENABLED
  return impl::SceneFX::Instance().shadow_weight;
#else
  return 0.0;
#endif
}

void Scene::SetDepthDarkening(bool f)
{
#if OST_SHADER_SUPPORT_ENABLED
  impl::SceneFX::Instance().depth_dark_flag=f;
  // the redraw routine will deal with the Shader
  RequestRedraw();
#endif
}

void Scene::SetDepthDarkeningWeight(float f)
{
#if OST_SHADER_SUPPORT_ENABLED
  impl::SceneFX::Instance().depth_dark_factor=f;
  // the redraw routine will deal with the Shader
  RequestRedraw();
#endif
}

void Scene::SetAmbientOcclusion(bool f)
{
#if OST_SHADER_SUPPORT_ENABLED
  impl::SceneFX::Instance().amb_occl_flag=f;
  // the redraw routine will deal with the Shader
  RequestRedraw();
#endif
} 

bool Scene::GetAmbientOcclusion() const
{
#if OST_SHADER_SUPPORT_ENABLED
  return impl::SceneFX::Instance().amb_occl_flag;
#else
  return false;
#endif
} 

void Scene::SetAmbientOcclusionWeight(float f)
{
#if OST_SHADER_SUPPORT_ENABLED
  impl::SceneFX::Instance().amb_occl_factor=f;
  // the redraw routine will deal with the Shader
  RequestRedraw();
#endif
}

float Scene::GetAmbientOcclusionWeight() const
{
#if OST_SHADER_SUPPORT_ENABLED
  return impl::SceneFX::Instance().amb_occl_factor;
#else
  return 1.0;
#endif
}

void Scene::SetAmbientOcclusionMode(uint m)
{
#if OST_SHADER_SUPPORT_ENABLED
  impl::SceneFX::Instance().amb_occl_mode=m;
  // the redraw routine will deal with the Shader
  RequestRedraw();
#endif
}

uint Scene::GetAmbientOcclusionMode() const
{
#if OST_SHADER_SUPPORT_ENABLED
  return impl::SceneFX::Instance().amb_occl_mode;
#else
  return 0;
#endif
}

void Scene::SetAmbientOcclusionQuality(uint m)
{
#if OST_SHADER_SUPPORT_ENABLED
  impl::SceneFX::Instance().amb_occl_quality=m;
  // the redraw routine will deal with the Shader
  RequestRedraw();
#endif
}

uint Scene::GetAmbientOcclusionQuality() const
{
#if OST_SHADER_SUPPORT_ENABLED
  return impl::SceneFX::Instance().amb_occl_quality;
#else
  return 0;
#endif
}

void Scene::SetShadingMode(const std::string& smode)
{
#if OST_SHADER_SUPPORT_ENABLED
  // this here is required - in case SetShadingMode is called
  // before GL is initialized (e.g. during batch mode rendering)
  def_shading_mode_=smode;
  if (!gl_init_) return;
  if(smode=="fallback") {
    Shader::Instance().Activate("");
  } else if(smode=="basic") {
    Shader::Instance().Activate("basic");
  } else if(smode=="hf" || smode=="hemilight") {
    Shader::Instance().Activate("hemilight");
  } else if(smode=="toon1") {
    Shader::Instance().Activate("toon1");
  } else if(smode=="toon2") {
    Shader::Instance().Activate("toon2");
  } else if(smode=="test_tex") {
    Shader::Instance().Activate("test_tex");
  } else {
    Shader::Instance().Activate("fraglight");
  }
  RequestRedraw();
#endif
}

void Scene::SetBeacon(int wx, int wy)
{
#if OST_SHADER_SUPPORT_ENABLED
  impl::SceneFX::Instance().use_beacon=true;
  impl::SceneFX::Instance().beacon.wx=static_cast<float>(wx);
  impl::SceneFX::Instance().beacon.wy=static_cast<float>(wy);
#endif  
}

void Scene::SetBeaconOff()
{
#if OST_SHADER_SUPPORT_ENABLED
  impl::SceneFX::Instance().use_beacon=false;
#endif  
}

namespace {

  void set_light_dir(Vec3 ld)
  {
    GLfloat l_pos[]={0.0, 0.0, 0.0, 0.0};
    l_pos[0]=-ld[0]; l_pos[1]=-ld[1]; l_pos[2]=-ld[2];
    glLightfv(GL_LIGHT0, GL_POSITION, l_pos);
  }

  struct GfxObjInitGL: public GfxNodeVisitor {
    virtual void VisitObject(GfxObj* o, const Stack& st) {
      o->InitGL();
    }
  };

}

void Scene::InitGL(bool full)
{
  LOG_VERBOSE("Scene: initializing GL state");

  if(full) {
    LOG_INFO(glGetString(GL_RENDERER) << ", openGL version " << glGetString(GL_VERSION)); 

#if OST_SHADER_SUPPORT_ENABLED
    LOG_DEBUG("Scene: shader pre-gl");
    Shader::Instance().PreGLInit();
#endif
  }

  // TODO: add more lights
  glLightfv(GL_LIGHT0, GL_AMBIENT, light_amb_);
  glLightfv(GL_LIGHT0, GL_DIFFUSE, light_diff_);
  glLightfv(GL_LIGHT0, GL_SPECULAR, light_spec_);
  set_light_dir(light_dir_);
  glEnable(GL_LIGHT0);
  glLightModeli(GL_LIGHT_MODEL_LOCAL_VIEWER,GL_FALSE);
  glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, GL_FALSE);
  GLfloat l_amb[]={0.0,0.0,0.0,0.0};
  glLightModelfv(GL_LIGHT_MODEL_AMBIENT, l_amb);
#ifndef _MSC_VER
  glLightModeli(GL_LIGHT_MODEL_COLOR_CONTROL, GL_SEPARATE_SPECULAR_COLOR);
#endif
  glEnable(GL_LIGHTING);   
  // set diffuse component to current color
  glColorMaterial(GL_FRONT_AND_BACK, GL_DIFFUSE);
  glEnable(GL_COLOR_MATERIAL);

  // depth buffer
  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LEQUAL);
  glClearDepth(1.0);

  // background
  glClearColor(background_.Red(),background_.Green(),background_.Blue(),background_.Alpha());
  fog_color_=background_;

  // polygon orientation setting
  glFrontFace(GL_CCW);

  // blending
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glEnable(GL_BLEND);  

  // depth effect thru fog
  GLfloat fogc[]={fog_color_.Red(),fog_color_.Green(),fog_color_.Blue(),1.0};
  glFogfv(GL_FOG_COLOR,fogc);
  glFogi(GL_FOG_MODE,GL_LINEAR);
  if(fog_flag_) {
    glEnable(GL_FOG);
  } else {
    glDisable(GL_FOG);
  }

  // some polygon settings
  glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);

  // smooth shading across polygon faces
  glShadeModel(GL_SMOOTH);

#if OST_SHADER_SUPPORT_ENABLED & defined(OST_GL_VERSION_2_0)
  GLint mbufs=0,msamples=0;

  if(OST_GL_VERSION_2_0) {
    glGetIntegerv(GL_SAMPLE_BUFFERS, &mbufs);
    glGetIntegerv(GL_SAMPLES, &msamples);
  }

  if(mbufs>0 && msamples>0) {
    LOG_INFO("Scene: enabling multisampling with: " << msamples << " samples");
    glDisable(GL_LINE_SMOOTH);
    glDisable(GL_POINT_SMOOTH);
    glDisable(GL_POLYGON_SMOOTH);
    glEnable(GL_MULTISAMPLE);
  } else {
    glEnable(GL_LINE_SMOOTH);
    glDisable(GL_POINT_SMOOTH);
    glDisable(GL_POLYGON_SMOOTH);
  }
#else
  // line and point anti-aliasing
  glEnable(GL_LINE_SMOOTH);
  glDisable(GL_POINT_SMOOTH);
  glDisable(GL_POLYGON_SMOOTH);
#endif

  // rendering hints
  glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
  glHint(GL_FOG_HINT, GL_NICEST);
  glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
  glHint(GL_POINT_SMOOTH_HINT, GL_NICEST);
  // full color only
  glDisable(GL_DITHER);
  glDisable(GL_NORMALIZE);
  // double-buffer rendering requires this
  // glDrawBuffer(GL_BACK);

#if OST_SHADER_SUPPORT_ENABLED
  if(full) {
    LOG_DEBUG("Scene: shader setup");
    Shader::Instance().Setup();
    SetShadingMode(def_shading_mode_);
    LOG_DEBUG("Scene: scenefx setup");
    impl::SceneFX::Instance().Setup();
  }
#endif

  if(full) {
    prep_glyphs();
  }

  if(full) {
    glGenTextures(1,&scene_left_tex_);
    glGenTextures(1,&scene_right_tex_);
  }

  glEnable(GL_TEXTURE_2D);
#if OST_SHADER_SUPPORT_ENABLED
  if(OST_GL_VERSION_2_0) {
    glActiveTexture(GL_TEXTURE0);
  }
#endif

  glBindTexture(GL_TEXTURE_2D, scene_left_tex_);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
  glTexEnvf(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE, GL_REPLACE);

  glBindTexture(GL_TEXTURE_2D, scene_right_tex_);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
  glTexEnvf(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE, GL_REPLACE);

  LOG_DEBUG("Scene: calling gl init for all objects");
  GfxObjInitGL initgl;
  this->Apply(initgl);

  LOG_DEBUG("Scene: gl init done");
  gl_init_=true;
  
  if(!def_shading_mode_.empty()) SetShadingMode(def_shading_mode_);
}

void Scene::RequestRedraw()
{
  if (win_ && !offscreen_flag_) {
    win_->DoRefresh();
  }
}

void Scene::StatusMessage(const String& s)
{
  if(win_) win_->StatusMessage(s);
}

void Scene::SetBackground(const Color& c)
{
  background_=c;
  if(gl_init_) {
    glClearColor(c.Red(),c.Green(),c.Blue(),c.Alpha());
    SetFogColor(c);
    RequestRedraw();
  }
}

Color Scene::GetBackground() const
{
  return background_;
}

Viewport Scene::GetViewport() const
{
  Viewport vp;
  if(gl_init_) {
    glGetIntegerv(GL_VIEWPORT, reinterpret_cast<GLint*>(&vp));
  }
  return vp;
}

void Scene::SetViewport(int w, int h)
{
  vp_width_=w;
  vp_height_=h;
  aspect_ratio_=static_cast<float>(w)/static_cast<float>(h);
  if(!gl_init_) return;
  glViewport(0,0,w,h);
  ResetProjection();
#if OST_SHADER_SUPPORT_ENABLED
  impl::SceneFX::Instance().Resize(w,h);
#endif
}

void Scene::Resize(int w, int h)
{
  SetViewport(w,h);
  RequestRedraw();
}

void Scene::SetCenter(const Vec3& cen)
{
  Vec3 tcen=transform_.Apply(cen);
  float delta_z = tcen[2]-transform_.GetTrans()[2];

  transform_.SetCenter(cen);
  transform_.SetTrans(Vec3(0,0,transform_.GetTrans()[2]));
  SetNearFar(znear_+delta_z,zfar_+delta_z);
  RequestRedraw();  
}

Vec3 Scene::GetCenter() const
{
  return transform_.GetCenter();
}

void Scene::CenterOn(const String& s)
{
  CenterOn((*this)[s]);
}

void Scene::CenterOn(const GfxObjP& go)
{
  if(go) {
    this->SetCenter(go->GetCenter());
  }
}

void Scene::RenderText(const TextPrim& t)
{
  if(!gl_init_) return;
  if(t.str.empty() || t.points<=0.0) return;
  Vec3 ppos = Project(t.position,false);

  float gh = t.points*def_text_size_;
  float gw = gh*0.5;
  float tlen = static_cast<float>(t.str.size());

  Vec3 pos0 = t.position;
  Vec3 pos1 = UnProject(ppos+Vec3(0.0,gh,0.0),false);
  Vec3 pos2 = UnProject(ppos+Vec3(tlen*gw,gh,0.0),false);
  Vec3 pos3 = UnProject(ppos+Vec3(tlen*gw,0.0,0.0),false);

  Vec3 delta21 = (pos2-pos1)/tlen;
  Vec3 delta30 = (pos3-pos0)/tlen;

#if OST_SHADER_SUPPORT_ENABLED
  Shader::Instance().PushProgram();
  Shader::Instance().Activate("");
#endif

  glPushAttrib(GL_ENABLE_BIT | GL_COLOR_BUFFER_BIT);
  glDisable(GL_NORMALIZE);
  glDisable(GL_CULL_FACE);
  glDisable(GL_LIGHTING);
  glDisable(GL_COLOR_MATERIAL);
  glEnable(GL_TEXTURE_2D);
  glTexEnvf(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_MODULATE);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
  glBindTexture(GL_TEXTURE_2D,glyph_tex_id_);

  float tdx=1.0/16.0;
  float tdy=1.0/8.0;

  glBegin(GL_QUADS);
  glColor3fv(t.color);
  for(unsigned int c=0;c<t.str.size();++c) {
    float tx=glyph_map_[t.str[c]][0];
    float ty=glyph_map_[t.str[c]][1];
    Vec3 dir1 = static_cast<float>(c)*delta30;
    Vec3 dir2 = static_cast<float>(c)*delta21;

    glTexCoord2d(tx,ty);
    glVertex3v(Vec3(pos0+dir1).Data());

    glTexCoord2d(tx,ty+tdy);
    glVertex3v(Vec3(pos1+dir2).Data());

    glTexCoord2d(tx+tdx,ty+tdy);
    glVertex3v(Vec3(pos1+dir2+delta30).Data());

    glTexCoord2d(tx+tdx,ty);
    glVertex3v(Vec3(pos0+dir1+delta30).Data());
  }
  glEnd();

  glPopAttrib();

#if OST_SHADER_SUPPORT_ENABLED
  Shader::Instance().PopProgram();
#endif
}

float Scene::GetDefaultTextSize()
{
  return def_text_size_;
}

namespace {

void draw_lightdir(const Vec3& ldir, const mol::Transform& tf)
{
  glPushAttrib(GL_ENABLE_BIT);
  glMatrixMode(GL_MODELVIEW);
  glPushMatrix();
  glLoadIdentity();
  mol::Transform tmpt(tf);
  tmpt.SetRot(Mat3::Identity());
  glMultMatrix(tmpt.GetTransposedMatrix().Data());
  glDisable(GL_NORMALIZE);
  glDisable(GL_CULL_FACE);
  glDisable(GL_LIGHTING);
  glDisable(GL_COLOR_MATERIAL);
  glColor3f(1.0,0.0,1.0);
  glLineWidth(2.0);
  glBegin(GL_LINES);
  glVertex3v(tmpt.GetCenter().Data());
  glVertex3v((tmpt.GetCenter()-50.0*ldir).Data());
  glEnd();
  glPopMatrix();
  glPopAttrib();
}

}

void Scene::RenderGL()
{
  if(auto_autoslab_ || do_autoslab_) {
    do_autoslab();
    do_autoslab_=false;
  }

  prep_blur();

  if(stereo_mode_==1 || stereo_mode_==2) {
    render_stereo();
  } else {
    render_scene();
  }
  check_gl_error();
}

void Scene::Register(GLWinBase* win)
{
  LOG_DEBUG("Scene: registered win @" << win);
  win_=win;
}

void Scene::Unregister(GLWinBase* win)
{
  LOG_DEBUG("Scene: unregistered win @" << win);
  win_=0;
}

namespace {
  struct FindNode: public GfxNodeVisitor {
    FindNode(const String& n): nam(n), node() {}
    virtual bool VisitNode(GfxNode* n, const Stack& st) {
      if(n->GetName()==nam) node=n->shared_from_this();
      return true;
    }
    virtual void VisitObject(GfxObj* o, const Stack& st) {
      if(o->GetName()==nam) node=o->shared_from_this();
    }
    String nam;
    GfxNodeP node;
  };

  struct FindObj: public GfxNodeVisitor {
    FindObj(const String& n): nam(n), node() {}
    virtual bool VisitNode(GfxNode* n, const Stack& st) {
      return true;
    }
    virtual void VisitObject(GfxObj* o, const Stack& st) {
      if(o->GetName()==nam) {
        GfxNodeP nn = o->shared_from_this();
        node = dyn_cast<GfxObj>(nn);
      }
    }
    String nam;
    GfxObjP node;
  };

#if 0
  struct RegexFindObj: public GfxNodeVisitor {
    RegexFindObj(const String& rs): reg(), valid(false), node() {
      try {
        reg = boost::regex(rs);
        valid = true;
      } catch (boost::regex_error& e) {
        LOG_ERROR("invalid regex");
      }
    }

    virtual bool VisitNode(GfxNode* n, const Stack& st) {
      return true;
    }

    virtual void VisitObject(GfxObj* o, const Stack& st) {
      if(boost::regex_match(o->GetName(),reg)) {
        GfxNodeP nn = o->shared_from_this();
        GfxObjP oo = dyn_cast<GfxObj>(nn);
        if(oo) {
          olist.push_back(oo);
        }
      }
    }

    boost::regex reg;
    bool valid;
    std::vector<GfxObjP> olist;
  };
#endif

  struct FindNode2: public GfxNodeVisitor {
    FindNode2(GfxNodeP n): node(n), found(false) {}
    virtual bool VisitNode(GfxNode* n, const Stack& st) {
      if(node.get()==n) found=true;
      return true;
    }
    virtual void VisitObject(GfxObj* o, const Stack& st) {
      if(node.get()==o) found=true;
    }
    GfxNodeP node;
    bool found;
  };
  struct NodeCount: public GfxNodeVisitor {
    NodeCount(): count_(0) {}
    virtual bool VisitNode(GfxNode* n, const Stack& st) {
      count_++;
      return true;
    }
    size_t GetCount() { return count_; }
  private:
    size_t count_;
  };
}

size_t Scene::GetNodeCount() const
{
  NodeCount nc;
  this->Apply(nc);
  return nc.GetCount();
}

void Scene::Add(const GfxNodeP& n, bool redraw)
{
  if(!n) return;
  // even though IsNameAvailable() is called in GfxNode::Add, check here 
  // as well to produce error message specific to adding a node to the scene.
  if(!this->IsNameAvailable(n->GetName())){
    throw Error("Scene already has a node with name '"+n->GetName()+"'");
  }

  LOG_DEBUG("Scene: graphical object added @" << n.get() << std::endl);

  GfxObjP go = boost::dynamic_pointer_cast<GfxObj>(n);

  if(go) {
    if(gl_init_) {
      go->InitGL();
    }
    if(root_node_->GetChildCount()==0) {
      SetCenter(go->GetCenter());
    }
    do_autoslab_=true;
  }

  root_node_->Add(n);
  if (redraw) {
    this->RequestRedraw();
  }
}

bool Scene::IsNameAvailable(const String& name) const
{
  return root_node_->IsNameAvailable(name);
}

void Scene::NodeAdded(const GfxNodeP& node)
{
  this->NotifyObservers(bind(&SceneObserver::NodeAdded, _1, node));
}

void Scene::ObjectChanged(const String& name)
{
  FindObj fn(name);
  Apply(fn);
  if(fn.node) {
    this->NotifyObservers(bind(&SceneObserver::NodeChanged, _1, fn.node));
  }
}

void Scene::NodeTransformed(const GfxObjP& object)
{
  this->NotifyObservers(bind(&SceneObserver::NodeTransformed, _1, object));
}

void Scene::SelectionChanged(const String& name, const mol::EntityView& sel)
{
  FindObj fn(name);
  Apply(fn);
  if(fn.node) {
    this->NotifyObservers(bind(&SceneObserver::SelectionChanged, _1, fn.node, sel));
  }
}

void Scene::RenderModeChanged(const String& name)
{
  FindObj fn(name);
  Apply(fn);
  if(fn.node) {
    this->NotifyObservers(bind(&SceneObserver::RenderModeChanged, _1, fn.node));
  }
}

void Scene::Remove(const GfxNodeP& go)
{
  if(!go) return;
  root_node_->Remove(go);
  this->NotifyObservers(bind(&SceneObserver::NodeRemoved, _1,go));
}

void Scene::RemoveAll()
{
  root_node_->RemoveAll();
  this->RequestRedraw();
}

void Scene::Remove(const String& name)
{
  FindNode fn(name);
  Apply(fn);
  if(fn.node) {
    root_node_->Remove(name);
    if(GfxObjP go = dyn_cast<GfxObj>(fn.node)) {
      this->NotifyObservers(bind(&SceneObserver::NodeRemoved, _1,go));
    }
  }
}

bool Scene::Rename(const String& old_name, const String& new_name)
{
  if(this->IsNameAvailable(new_name)){
    FindNode fn(old_name);
    Apply(fn);
    if(fn.node) {
      if(GfxNodeP go = dyn_cast<GfxObj>(fn.node)) {
        go->Rename(new_name);
        return true;
      }
    }
  }
  return false;
}

GfxObjP Scene::operator[](const String& name)
{
  FindNode fn(name);
  Apply(fn);
  if(!fn.node) {
    LOG_ERROR("Scene: error: " << name << " not found");
    return GfxObjP();
  }
  GfxObjP nrvo = dyn_cast<GfxObj>(fn.node);
  if(!nrvo) {
    LOG_ERROR("Scene: error: " << name << " points to invalid entry");
  }
  return nrvo;
}

bool Scene::HasNode(const String& name) const
{
  FindNode fn(name);
  this->Apply(fn);
  return fn.node;
}

void Scene::Apply(const InputEvent& e, bool request_redraw)
{
  LOG_TRACE("Scene: received input: c=" << e.GetCommand() 
             << " i=" << e.GetIndex() << " t=" << e.GetTarget());
  /*
    TODO
    distribute event to transform that
    is meant to receive events from the
    particular device/index combo
    AND
    which is flagged as the edit entry
  */

  if(e.GetTarget()==TRANSFORM_VIEW) {
    // scene
    OnInput(e);
  }
  if(request_redraw) {
    RequestRedraw();
  }
}

void Scene::Apply(GfxNodeVisitor& v) const
{
  v.OnStart();
  GfxNodeVisitor::Stack st;
  // this is the reason for the mutable hack: root_node_ can't be const
  root_node_->Apply(v,st);
  v.OnEnd();
}


void Scene::OnInput(const InputEvent& e)
{
  if(e.GetCommand()==INPUT_COMMAND_AUTOSLAB) {
    Autoslab();
  } else if(e.GetCommand()==INPUT_COMMAND_TOGGLE_FOG) {
    SetFog(!fog_flag_);
  } else if(e.GetCommand()==INPUT_COMMAND_ROTX) {
    transform_.ApplyXAxisRotation(e.GetDelta());
  } else if(e.GetCommand()==INPUT_COMMAND_ROTY) {
    transform_.ApplyYAxisRotation(e.GetDelta());
  } else if(e.GetCommand()==INPUT_COMMAND_ROTZ) {
    transform_.ApplyZAxisRotation(e.GetDelta());
  } else if(e.GetCommand()==INPUT_COMMAND_TRANSX ||
            e.GetCommand()==INPUT_COMMAND_TRANSY) {

    // adjust translation by current distance of camera from center of rotation
    Vec3 trans = transform_.GetTrans();
    double mm[]={1,0,0,0,
                 0,1,0,0,
                 0,0,1,0,
                 trans[0],trans[1],trans[2],1};
    double pm[16];
    glGetDoublev(GL_PROJECTION_MATRIX,pm);
    GLint vp[4];
    glGetIntegerv(GL_VIEWPORT,vp);
    double wx,wy,wz;
    gluProject(0.0,0.0,0.0,mm,pm,vp,&wx,&wy,&wz);
    double ox,oy,oz;
    gluUnProject(wx+2.0,wy+2.0,wz,mm,pm,vp,&ox,&oy,&oz);
    Vec2 fxy = Vec2(ox,oy);

    if(fix_cor_flag_) {
      if(e.GetCommand()==INPUT_COMMAND_TRANSX) {
        transform_.SetCenter(transform_.GetCenter()+Transpose(transform_.GetRot())*Vec3(-fxy[0]*e.GetDelta(),0.0,0.0));
      } else {
        transform_.SetCenter(transform_.GetCenter()+Transpose(transform_.GetRot())*Vec3(0.0,-fxy[1]*e.GetDelta(),0.0));
      }
    } else {
      if(e.GetCommand()==INPUT_COMMAND_TRANSX) {
        transform_.ApplyXAxisTranslation(e.GetDelta()*fxy[0]);
      } else {
        transform_.ApplyYAxisTranslation(e.GetDelta()*fxy[1]);
      }
    }
  } else if(e.GetCommand()==INPUT_COMMAND_TRANSZ) {
    float currz=transform_.GetTrans()[2];
    float delta=currz*pow(1.01f,-e.GetDelta())-currz;
    transform_.ApplyZAxisTranslation(delta);
    SetNearFar(znear_-delta,zfar_-delta);
  } else if(e.GetCommand()==INPUT_COMMAND_SLABN) {
    SetNear(znear_+e.GetDelta());
  } else if(e.GetCommand()==INPUT_COMMAND_SLABF) {
    SetFar(zfar_+e.GetDelta());
  }
}

namespace {

class Selector: public GfxNodeVisitor
{
public:
  void VisitObject(GfxObj* obj, const Stack& st) {
    if(obj->OnSelect(line,picked_pos,zlim,pick_flag)) {
      Vec3 proj_pos=Scene::Instance().Project(picked_pos);
      zlim = proj_pos[2];
    }

  }
  Line3 line;
  bool pick_flag;
  float zlim;
  Vec3 picked_pos;
};

} // anon ns

void Scene::Pick(int mx, int my, int mask)
{
  this->ResetProjection();
  Vec3 v1=UnProject(Vec3(mx,my,0.0));
  Vec3 v2=UnProject(Vec3(mx,my,1.0));

  LOG_DEBUG("Scene: pick: " << v1 << " " << v2 << " " << mask);

  Scene::Instance().StatusMessage("");

  Selector stor;

  stor.line=Line3(v1,v2);
  stor.pick_flag = (mask==8) ? true : false;
  stor.zlim = std::numeric_limits<float>::max();

  Apply(stor);

  RequestRedraw();
}

namespace {


class AtomPicker : public GfxNodeVisitor {
public:
  AtomPicker(const Line3& line):
    zlim_(std::numeric_limits<float>::max()), line_(line), obj_(NULL)
  { }

  void VisitObject(GfxObj* obj, const Stack& st)
  {
    if (Entity* ec=dynamic_cast<Entity*>(obj)) {
      if(mol::AtomHandle pa=ec->PickAtom(line_)) {
        Vec3 proj_pos=Scene::Instance().Project(pa.GetPos());
        if (proj_pos[2]<zlim_) {
          zlim_=proj_pos[2];
          picked_atom_=pa;
          obj_=obj;
        }
      }
    }
  }
  mol::AtomHandle GetPickedAtom() { return picked_atom_; }

  GfxObjP GetObj()
  {
    if (obj_) {
      return dyn_cast<GfxObj>(obj_->shared_from_this());
    } else {
      return GfxObjP();
    }
  }
private:
  float zlim_;
  Line3 line_;
  mol::AtomHandle picked_atom_;
  GfxObj* obj_;
};
}


std::pair<GfxObjP, mol::AtomHandle> Scene::PickAtom(int mx, int my)
{
  Vec3 v1=UnProject(Vec3(mx,my,0.0));
  Vec3 v2=UnProject(Vec3(mx,my,1.0));
  AtomPicker picker(Line3(v1, v2));
  this->Apply(picker);
  return std::make_pair(picker.GetObj(), picker.GetPickedAtom());
}


void Scene::AttachObserver(SceneObserver* o) {
  observers_.push_back(o);
}

void Scene::DetachObserver(SceneObserver* o) {
  SceneObserverList::iterator i;
  i=std::remove(observers_.begin(), observers_.end(), o);
  observers_.erase(i, observers_.end());
}

Vec3 Scene::Project(const Vec3& v, bool ignore_vp) const
{
  if(!gl_init_) return Vec3();
  GLdouble gl_mmat[16];
  glGetDoublev(GL_MODELVIEW_MATRIX,gl_mmat);
  GLdouble gl_pmat[16];
  glGetDoublev(GL_PROJECTION_MATRIX,gl_pmat);
  GLint gl_vp[]={0,0,1,1};
  if(!ignore_vp) {
    glGetIntegerv(GL_VIEWPORT,gl_vp);
  }

  double res[3];
  gluProject(v[0],v[1],v[2],
       gl_mmat,gl_pmat,gl_vp,
       &res[0],&res[1],&res[2]);
  return geom::Vec3(float(res[0]),
                    float(res[1]),
                    float(res[2]));
}

Vec3 Scene::UnProject(const Vec3& v, bool ignore_vp) const
{
  if(!gl_init_) return Vec3();
  GLdouble gl_mmat[16];
  glGetDoublev(GL_MODELVIEW_MATRIX,gl_mmat);
  GLdouble gl_pmat[16];
  glGetDoublev(GL_PROJECTION_MATRIX,gl_pmat);
  GLint gl_vp[]={0,0,1,1};
  if(!ignore_vp) {
    glGetIntegerv(GL_VIEWPORT,gl_vp);
  }

  double res[3];
  gluUnProject(v[0],v[1],v[2],
         gl_mmat,gl_pmat,gl_vp,
         &res[0],&res[1],&res[2]);
  return geom::Vec3(float(res[0]),
                    float(res[1]),
                    float(res[2]));
}

namespace {

class BBCalc: public GfxNodeVisitor
{
public:
  BBCalc(const geom::Vec3& mmin, const geom::Vec3& mmax, const mol::Transform& tf): 
    minc(mmin),maxc(mmax),tf(tf),valid(false) {}

  bool VisitNode(GfxNode* node, const Stack& st) {
    return node->IsVisible(); // only descend into visible nodes
  }
  void VisitObject(GfxObj* obj, const Stack& st) {
    if(obj->IsVisible()) {
      geom::AlignedCuboid bb=obj->GetBoundingBox();
      if(bb.GetVolume()>0.0) {
        Vec3 t1 = tf.Apply(Vec3(bb.GetMin()[0],bb.GetMin()[1],bb.GetMin()[2]));
        Vec3 t2 = tf.Apply(Vec3(bb.GetMin()[0],bb.GetMax()[1],bb.GetMin()[2]));
        Vec3 t3 = tf.Apply(Vec3(bb.GetMax()[0],bb.GetMax()[1],bb.GetMin()[2]));
        Vec3 t4 = tf.Apply(Vec3(bb.GetMax()[0],bb.GetMin()[1],bb.GetMin()[2]));
        Vec3 t5 = tf.Apply(Vec3(bb.GetMin()[0],bb.GetMin()[1],bb.GetMax()[2]));
        Vec3 t6 = tf.Apply(Vec3(bb.GetMin()[0],bb.GetMax()[1],bb.GetMax()[2]));
        Vec3 t7 = tf.Apply(Vec3(bb.GetMax()[0],bb.GetMax()[1],bb.GetMax()[2]));
        Vec3 t8 = tf.Apply(Vec3(bb.GetMax()[0],bb.GetMin()[1],bb.GetMax()[2]));
        minc = Min(minc,Min(t1,Min(t2,Min(t3,Min(t4,Min(t5,Min(t6,Min(t7,t8))))))));
        maxc = Max(maxc,Max(t1,Max(t2,Max(t3,Max(t4,Max(t5,Max(t6,Max(t7,t8))))))));
        valid=true;
      }
    }
  }

  Vec3 minc,maxc;
  mol::Transform tf;
  bool valid;
};

}

geom::AlignedCuboid Scene::GetBoundingBox(const mol::Transform& tf) const
{
  BBCalc bbcalc(Vec3(std::numeric_limits<float>::max(),
                     std::numeric_limits<float>::max(),
                     std::numeric_limits<float>::max()),
                Vec3(-std::numeric_limits<float>::max(),
                     -std::numeric_limits<float>::max(),
                     -std::numeric_limits<float>::max()),
                tf);

  Apply(bbcalc);

  if(bbcalc.valid) {
    return geom::AlignedCuboid(bbcalc.minc,bbcalc.maxc);
  }
  return geom::AlignedCuboid(geom::Vec3(),geom::Vec3());
}

mol::Transform Scene::GetTransform() const
{
  return transform_;
}

void Scene::SetTransform(const mol::Transform& t)
{
  transform_=t;
  this->RequestRedraw();
}

Mat4 Scene::GetRTC() const
{
  Mat4 nrvo(transform_.GetRot());
  for(int i=0;i<3;++i) {
    nrvo(3,i) = transform_.GetTrans()[i];
    nrvo(i,3) = transform_.GetCenter()[i];
  }
  return nrvo;
}

void Scene::SetRTC(const Mat4& rtc)
{
  transform_.SetRot(Mat3(rtc(0,0), rtc(0,1), rtc(0,2),
                               rtc(1,0), rtc(1,1), rtc(1,2),
                               rtc(2,0), rtc(2,1), rtc(2,2)));
  transform_.SetTrans(Vec3(rtc(3,0),rtc(3,1),rtc(3,2)));
  transform_.SetCenter(Vec3(rtc(0,3),rtc(1,3),rtc(2,3)));
  RequestRedraw();
}

void Scene::PushView()
{
  SceneViewStackEntry se = {transform_,fov_,znear_,zfar_};
  scene_view_stack_.push(se);
}

void Scene::PopView()
{
  if(!scene_view_stack_.empty()) {
    transform_=scene_view_stack_.top().transform;
    fov_=scene_view_stack_.top().fov;
    znear_=scene_view_stack_.top().znear;
    zfar_=scene_view_stack_.top().zfar;
    scene_view_stack_.pop();
    ResetProjection();
    update_fog();
    RequestRedraw();
  }
}

float Scene::GetNear() const
{
  return znear_;
}

void Scene::SetNear(float n)
{
  set_near(n);
  ResetProjection();
  RequestRedraw();
}

float Scene::GetFar() const
{
  return zfar_;
}

void Scene::SetFar(float f)
{
  set_far(f);
  ResetProjection();
  RequestRedraw();
}

void Scene::SetNearFar(float n, float f)
{
  // this apparent setting duplication is necessary
  // due to the limit check in set_*
  znear_=n;
  zfar_=f;
  set_near(n);
  set_far(f);
  ResetProjection();
  RequestRedraw();
}

void Scene::SetFOV(float f)
{
  fov_=std::max(1.0f,f);
  ResetProjection();
  RequestRedraw();
}

float Scene::GetFOV() const
{
  return fov_;
}

void Scene::SetFogNearOffset(float o)
{
  fnear_=o;
  update_fog();
  RequestRedraw();
}

float Scene::GetFogNearOffset() const
{
  return fnear_;
}

void Scene::SetFogFarOffset(float o)
{
  ffar_=o;
  update_fog();
  RequestRedraw();
}

float Scene::GetFogFarOffset() const
{
  return ffar_;
}

void Scene::SetFogOffsets(float no, float fo)
{
  fnear_=no;
  ffar_=fo;
  update_fog();
  RequestRedraw();
}

void Scene::SetStereoMode(unsigned int m)
{
  if(m==1) {
    if(win_ && win_->HasStereo()) {
      stereo_mode_=1;
    } else {
      LOG_INFO("Scene: No visual present for quad-buffered stereo");
      stereo_mode_=0;
    }
  } else if(m==2) {
    stereo_mode_=2;
  } else {
    stereo_mode_=0;
  }
  RequestRedraw();
}

void Scene::SetStereoFlip(bool f)
{
  stereo_inverted_=f;
  RequestRedraw();
}

void Scene::SetStereoView(int m)
{
  stereo_eye_= m;
  ResetProjection();
  RequestRedraw();
}

void Scene::SetStereoIOD(Real d)
{
  stereo_iod_=d;
  if(stereo_mode_>0) {
    RequestRedraw();
  }
}

void Scene::SetStereoDistance(Real d)
{
  stereo_distance_=d;
  if(stereo_mode_>0) {
    RequestRedraw();
  }
}

void Scene::SetStereoAlg(unsigned int a)
{
  stereo_alg_=a;
  if(stereo_alg_>1) {
    stereo_alg_=0;
  }
  if(stereo_mode_>0) {
    RequestRedraw();
  }
}

void Scene::SetLightDir(const Vec3& dir)
{
  if(Length2(dir)>0.0) {
    light_dir_=Normalize(dir);
    light_rot_=calc_light_rot(light_dir_);
    set_light_dir(light_dir_);
    RequestRedraw();
  }
}

void Scene::SetLightProp(const Color& amb, const Color& diff, 
                         const Color& spec)
{
  light_amb_=amb;
  light_diff_=diff;
  light_spec_=spec;
  if(!gl_init_) return;
  glLightfv(GL_LIGHT0, GL_AMBIENT, light_amb_);
  glLightfv(GL_LIGHT0, GL_DIFFUSE, light_diff_);
  glLightfv(GL_LIGHT0, GL_SPECULAR, light_spec_);
  RequestRedraw();
}

void Scene::SetLightProp(float amb, float diff, float spec)
{
  light_amb_=Color(amb,amb,amb,1.0);
  light_diff_=Color(diff,diff,diff,1.0);
  light_spec_=Color(spec,spec,spec,1.0);
  if(!gl_init_) return;
  glLightfv(GL_LIGHT0, GL_AMBIENT, light_amb_);
  glLightfv(GL_LIGHT0, GL_DIFFUSE, light_diff_);
  glLightfv(GL_LIGHT0, GL_SPECULAR, light_spec_);
  RequestRedraw();
}

void Scene::SetSelectionMode(uint m)
{
  if (selection_mode_!=m) {
    selection_mode_=m;
    this->NotifyObservers(bind(&SceneObserver::SelectionModeChanged, _1,
                               selection_mode_));
  }

}

uint Scene::GetSelectionMode() const
{
  return selection_mode_;
}

bool Scene::StartOffscreenMode(unsigned int width, unsigned int height)
{
  LOG_DEBUG("Scene: starting offscreen rendering mode " << width << "x" << height);
  if(main_offscreen_buffer_) return false;
  main_offscreen_buffer_ = new OffscreenBuffer(width,height,OffscreenBufferFormat(),true);

  if(!main_offscreen_buffer_->IsValid()) {
    LOG_ERROR("Scene: error during offscreen buffer creation");
    delete main_offscreen_buffer_;   
    main_offscreen_buffer_=0;
    return false;
  }
  old_vp_[0]=vp_width_;
  old_vp_[1]=vp_height_;
  main_offscreen_buffer_->MakeActive();
  offscreen_flag_=true;
  root_node_->ContextSwitch();

#if OST_SHADER_SUPPORT_ENABLED
  String shader_name = !def_shading_mode_.empty() ? def_shading_mode_ : Shader::Instance().GetCurrentName();
#endif

  LOG_DEBUG("Scene: initializing GL");
  if(gl_init_) {
    this->InitGL(false);
  } else {
    this->InitGL(true);
  }
  LOG_DEBUG("Scene: setting viewport");
  Resize(width,height);
  LOG_DEBUG("Scene: updating fog settings");
  update_fog();
  glDrawBuffer(GL_FRONT);
#if OST_SHADER_SUPPORT_ENABLED
  LOG_DEBUG("Scene: activating shader " << shader_name);
  Shader::Instance().Activate(shader_name);
#endif
  return true;
}

void Scene::StopOffscreenMode()
{
  if(main_offscreen_buffer_) {
    if (win_) {
      win_->MakeActive();
    }
    delete main_offscreen_buffer_;
    main_offscreen_buffer_=0;
    Scene::Instance().SetViewport(old_vp_[0],old_vp_[1]);
    offscreen_flag_=false;
    root_node_->ContextSwitch();
    glDrawBuffer(GL_BACK);
    update_fog();
  }
}

void Scene::Export(const String& fname, unsigned int width,
                   unsigned int height, bool transparent)
{
  int d_index=fname.rfind('.');
  if (d_index==-1) {
    LOG_ERROR("Scene: no file extension specified");
    return;
  }
  String ext = fname.substr(d_index);
  if(!(ext==".png")) {
    LOG_ERROR("Scene: unknown file format (" << ext << ")");
    return;
  }

  bool of_flag = (main_offscreen_buffer_==0);

  // only switch if offscreen mode is not active
  if(of_flag) {
    if(!StartOffscreenMode(width,height)) {
      return;
    }
  }
  LOG_DEBUG("Scene: rendering into offscreen buffer");
  this->RenderGL();
  // make sure drawing operations are finished
  glFlush();
  glFinish();

  boost::shared_array<uchar> img_data(new uchar[width*height*4]);
      
  LOG_DEBUG("Scene: setting background transparency");
  if (transparent) {
    glPixelTransferf(GL_ALPHA_BIAS, 0.0);
  } else {
    // shift alpha channel by one to make sure pixels are read out as opaque
    glPixelTransferf(GL_ALPHA_BIAS, 1.0);
  }
  
  LOG_DEBUG("Scene: reading framebuffer pixels");
  glReadBuffer(GL_FRONT);
  glReadPixels(0,0,width,height,GL_RGBA,GL_UNSIGNED_BYTE,img_data.get());
  glReadBuffer(GL_BACK);

  LOG_DEBUG("Scene: calling bitmap export");
  BitmapExport(fname,ext,width,height,img_data.get());

  // only switch back if it was not on to begin with
  if(of_flag) {
    StopOffscreenMode();
  }
}

void Scene::Export(const String& fname, bool transparent)
{
  if(!win_ && !main_offscreen_buffer_) {
    LOG_ERROR("Scene: Export without dimensions either requires an interactive session \nor an active offscreen mode (scene.StartOffscreenMode(W,H))");
    return;
  }
  int d_index=fname.rfind('.');
  if (d_index==-1) {
    LOG_ERROR("Scene: no file extension specified");
    return;
  }
  String ext = fname.substr(d_index);
  if(ext!=".png") {
    LOG_ERROR("Scene: unknown file format (" << ext << ")");
    return;
  }
  GLint vp[4];
  glGetIntegerv(GL_VIEWPORT,vp);

  if(main_offscreen_buffer_) {
    this->RenderGL();
    glFlush();
    glFinish();
  }

  if (transparent) {
    glPixelTransferf(GL_ALPHA_BIAS, 0.0);
  } else {
    // shift alpha channel by one to make sure pixels are read out as opaque    
    glPixelTransferf(GL_ALPHA_BIAS, 1.0);    
  }
  boost::shared_array<uchar> img_data(new uchar[vp[2]*vp[3]*4]);
  glReadPixels(0,0,vp[2],vp[3],GL_RGBA,GL_UNSIGNED_BYTE,img_data.get());
  glFinish();
  BitmapExport(fname,ext,vp[2],vp[3],img_data.get());
  glPixelTransferf(GL_ALPHA_BIAS, 0.0);  
}

void Scene::ExportPov(const std::string& fname, const std::string& wdir)
{
  std::string wdir2=wdir;
  if(wdir2=="") wdir2=".";
  PovState pov(fname+".pov",fname+".inc",wdir2+"/");
  pov.tf=transform_;
  pov.use_tf=false;

  pov.write_preamble();
  pov.write_background(background_);
  pov.write_camera(fov_,-transform_.GetTrans()[2]*aspect_ratio_*1.3);
  pov.write_default_light();
  pov.write_fog(-transform_.GetTrans()[2],znear_+fnear_,zfar_+ffar_,fog_color_,fog_flag_);

  // descend into scene graph
  root_node_->RenderPov(pov);
  pov.write_postamble();
}

void Scene::Export(Exporter* ex) const
{
  ex->SceneStart(this);
  root_node_->Export(ex);
  ex->SceneEnd(this);
}

void Scene::ResetProjection()
{
  LOG_TRACE("Scene: projection matrix " << fov_ << " " << znear_ << " " << zfar_);
  stereo_projection(stereo_eye_);
}

void Scene::SetBlur(uint n)
{
  blur_count_=std::min(n,3u);
  if(!gl_init_) return;
  glClearAccum(0.0,0.0,0.0,0.0);
  glClear(GL_ACCUM_BUFFER_BIT);
  RequestRedraw();
}

void Scene::BlurSnapshot()
{
  if(!gl_init_) return;
  if(blur_count_==0) return;
  glFinish();
  glAccum(GL_MULT, 0.5);
  glAccum(GL_ACCUM, 0.5);
  RequestRedraw();
}

void Scene::AutoAutoslab(bool f)
{
  auto_autoslab_=f;
  do_autoslab_=f;
  RequestRedraw();
}

namespace {

class LimCalc: public GfxNodeVisitor
{
public:
  LimCalc(): minc(),maxc(),transform(),valid(false) {}
  void VisitObject(GfxObj* obj, const Stack& st) {
    if(obj->IsVisible()) {
      obj->ProcessLimits(minc,maxc, transform);
      // this is buggy - ProcessLimits should really return a boolean 
      // indicating whether it could succesfully apply limits or not
      valid=true;
    }
  }
  Vec3 minc,maxc;
  mol::Transform transform;
  bool valid;
};

} // anon ns

void Scene::Autoslab(bool fast, bool)
{
  do_autoslab_=true;
  do_autoslab_fast_=fast;
  RequestRedraw();
}

void Scene::AutoslabMax()
{
  geom::AlignedCuboid bb =this->GetBoundingBox(transform_);

  if(bb.GetVolume()==0.0) {
    znear_=1;
    zfar_=100;
    set_near(1);
    set_far(100);
  } else {

    Vec3 cen = transform_.Apply(transform_.GetCenter());
    
    float bmax = std::max(std::abs(cen[0]-bb.GetMin()[0]),
                          std::abs(cen[0]-bb.GetMax()[0]));
    bmax = std::max(bmax,float(std::abs(cen[1]-bb.GetMin()[1])));
    bmax = std::max(bmax,float(std::abs(cen[1]-bb.GetMax()[1])));
    bmax = std::max(bmax,float(std::abs(cen[2]-bb.GetMin()[2])));
    bmax = std::max(bmax,float(std::abs(cen[2]-bb.GetMax()[2])));
    
    float nnear = -(cen[2]+bmax*1.5);
    float nfar = -(cen[2]-bmax*1.5);
    
    // necessary code duplication due to awkward slab limit impl
    znear_=nnear;
    zfar_=nfar;
    set_near(nnear);
    set_far(nfar);
  }
  ResetProjection();
}


void Scene::set_near(float n)
{
  znear_=n;
  if(znear_>zfar_-0.1) {
    znear_=zfar_-0.1;
  }
  update_fog();
}

void Scene::set_far(float f)
{
  zfar_=f;
  if(znear_>zfar_-0.1) {
    zfar_=znear_+0.1;
  }
  update_fog();
}

void Scene::update_fog()
{
  if(gl_init_) {
    glFogf(GL_FOG_START,znear_+fnear_);
    glFogf(GL_FOG_END,zfar_+ffar_);
  }
}


namespace {
class DirtyAll: public GfxNodeVisitor
{
public:
  void VisitObject(GfxObj* obj, const Stack& st) {obj->FlagRebuild();}
};

} // anon ns

void Scene::flag_all_dirty()
{
  static DirtyAll da;
  Apply(da);
}

GfxNodeP Scene::GetRootNode() const
{
  return root_node_;
}

bool Scene::InOffscreenMode() const
{
  return offscreen_flag_;
}

float Scene::ElapsedTime() const
{
#ifndef _MSC_VER
  static timeval tm;
  gettimeofday(&tm,NULL);
  return static_cast<float>(tm.tv_sec)+static_cast<float>(tm.tv_usec)*1.0e-6;
#else
  return 0.0;
#endif
}

void Scene::SetTestMode(bool f)
{
  test_flag_=f;
  if(f) {
    Add(GfxObjP(new GfxTestObj()));
    Autoslab();
  } else {
    Remove("GfxTestObj");
  }
}

void Scene::SetShowCenter(bool f)
{
  cor_flag_=f;
  RequestRedraw();
}

void Scene::prep_glyphs()
{
  glGenTextures(1,&glyph_tex_id_);
  String ost_root =GetSharedDataPath();
  bf::path ost_root_dir(ost_root);
  bf::path tex_file(ost_root_dir / "textures/glyph_texture.png");
  Bitmap bm = BitmapImport(tex_file.string(),".png");
  if(!bm.data) return;

  LOG_DEBUG("Scene: importing glyph tex with id " << glyph_tex_id_);
  glBindTexture(GL_TEXTURE_2D, glyph_tex_id_);
  glPixelStorei(GL_UNPACK_ALIGNMENT,1);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  if(bm.channels==1) {
    glTexImage2D(GL_TEXTURE_2D,0,GL_INTENSITY,bm.width,bm.height,0,GL_LUMINANCE,GL_UNSIGNED_BYTE,bm.data.get());
  } else if(bm.channels==3) {
    glTexImage2D(GL_TEXTURE_2D,0,GL_INTENSITY,bm.width,bm.height,0,GL_RGB,GL_UNSIGNED_BYTE,bm.data.get());
  } else if(bm.channels==4) {
    glTexImage2D(GL_TEXTURE_2D,0,GL_INTENSITY,bm.width,bm.height,0,GL_RGBA,GL_UNSIGNED_BYTE,bm.data.get());
  } else {
    LOG_ERROR("Scene: unsupported glyph texture channel count of " << bm.channels);
    return;
  }
  float ir = 1.0/8.0;
  float ic = 1.0/16.0;
  glyph_map_.resize(256);
  for(int cc=0;cc<17;++cc) glyph_map_[cc]=Vec2(0.0,0.0);
  for(int r=0;r<7;++r) {
    for(int c=0;c<16;++c) {
      glyph_map_[r*16+c+17]=Vec2(ic*static_cast<float>(c), 1.0-ir*static_cast<float>(r)-ir);
    }
  }
  for(int cc=128;cc<256;++cc) glyph_map_[cc]=Vec2(0.0,0.0);

  LOG_DEBUG("Scene: done loading glyphs");
}

void Scene::prep_blur()
{
  if(blur_count_==0) return;
  glAccum(GL_RETURN, 0.5);
  glFlush();
}

void Scene::render_scene()
{
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();

  glMultMatrix(transform_.GetTransposedMatrix().Data());

#if OST_SHADER_SUPPORT_ENABLED
  impl::SceneFX::Instance().Preprocess();
#endif

  if(cor_flag_) {
    geom::Vec3 cen=transform_.GetCenter();
    glPushAttrib(GL_ENABLE_BIT | GL_LIGHTING_BIT | GL_LINE_BIT | GL_CURRENT_BIT);
#if OST_SHADER_SUPPORT_ENABLED
    Shader::Instance().PushProgram();
    Shader::Instance().Activate("");
#endif
    glLineWidth(1.5);
    glDisable(GL_LIGHTING);
    glDisable(GL_COLOR_MATERIAL);
    
    glBegin(GL_LINES);
    glColor3f(0.5,0.5,0.5);
    glVertex3f(cen[0]-1.0,cen[1],cen[2]);
    glVertex3f(cen[0]+1.0,cen[1],cen[2]);
    glVertex3f(cen[0],cen[1]-1.0,cen[2]);
    glVertex3f(cen[0],cen[1]+1.0,cen[2]);
    glVertex3f(cen[0],cen[1],cen[2]-1.0);
    glVertex3f(cen[0],cen[1],cen[2]+1.0);
    glEnd();
    glPopAttrib();
#if OST_SHADER_SUPPORT_ENABLED
    Shader::Instance().PopProgram();
#endif
  }

  root_node_->RenderGL(STANDARD_RENDER_PASS);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  root_node_->RenderGL(TRANSPARENT_RENDER_PASS);
  glDisable(GL_BLEND);
  root_node_->RenderGL(OVERLAY_RENDER_PASS);

#if OST_SHADER_SUPPORT_ENABLED
  impl::SceneFX::Instance().Postprocess();
#endif

  render_glow();
}

void Scene::render_glow()
{
  glPushAttrib(GL_ALL_ATTRIB_BITS);
  glEnable(GL_COLOR_MATERIAL);
  glShadeModel(GL_FLAT);
  glDisable(GL_FOG);
  glDisable(GL_LIGHTING);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

#if OST_SHADER_SUPPORT_ENABLED
  Shader::Instance().PushProgram();
  Shader::Instance().Activate("");
#endif  
  float eps=0.005;
  glDepthRange(-eps, 1.0-eps);
  root_node_->RenderGL(GLOW_RENDER_PASS);
  glDepthRange(0.0, 1.0);
  glPopAttrib();
  glShadeModel(GL_SMOOTH);
#if OST_SHADER_SUPPORT_ENABLED
  Shader::Instance().PopProgram();
#endif  
}

void Scene::stereo_projection(int view)
{
  if(!gl_init_) return;
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();

  Real zn=std::max<Real>(1.0,znear_);
  Real zf=std::max<Real>(1.2,zfar_);
  Real top = zn * std::tan(fov_*M_PI/360.0);
  Real bot = -top;
  Real left = -top*aspect_ratio_;
  Real right = -left;

  if(view!=0) {
    
    Real ff=(view<0 ? -1.0 : 1.0);
    Real iod=std::max<Real>(0.1,stereo_iod_);

    if(stereo_alg_==1) {
      // Toe-in method, easy but wrong
      glFrustum(left,right,bot,top,zn,zf);
      Real dist = -transform_.GetTrans()[2]+stereo_distance_;
      glTranslated(0.0,0.0,-dist);
      glRotated(-180.0/M_PI*atan(0.1*ff/iod),0.0,1.0,0.0);
      glTranslated(0.0,0.0,dist);
    } else {
      // correct off-axis frustims

      Real fo=-transform_.GetTrans()[2]+stereo_distance_;

      // correction of near clipping plane to avoid extreme drifting
      // of left and right view
#if 0
      if(iod*zn/fo<2.0) {
        zn=2.0*fo/iod;
        zf=std::max(zn+Real(0.2),zf);
      }
#endif
    
      Real sd = -ff*0.5*iod*zn/fo;
      left+=sd;
      right+=sd;

      glFrustum(left,right,bot,top,zn,zf);
      glTranslated(-ff*iod*0.5,0.0,0.0);
    }

  } else { // view==0
    // standard viewing frustum
    glFrustum(left,right,bot,top,zn,zf);
  }
}

void Scene::render_stereo()
{
  glPushAttrib(GL_ALL_ATTRIB_BITS);
  glPushClientAttrib(GL_ALL_ATTRIB_BITS);

  int old_stereo_eye=stereo_eye_;
  stereo_eye_=-1;
  stereo_projection(-1);
  render_scene();

  glEnable(GL_TEXTURE_2D);
#if OST_SHADER_SUPPORT_ENABLED
  if(OST_GL_VERSION_2_0) {
    glActiveTexture(GL_TEXTURE0);
  }
#endif
  glBindTexture(GL_TEXTURE_2D, scene_left_tex_);
  glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 0, 0, vp_width_, vp_height_, 0);

  stereo_eye_=1;
  stereo_projection(1);
  render_scene();

  glEnable(GL_TEXTURE_2D);
#if OST_SHADER_SUPPORT_ENABLED
  if(OST_GL_VERSION_2_0) {
    glActiveTexture(GL_TEXTURE0);
  }
#endif
  glBindTexture(GL_TEXTURE_2D, scene_right_tex_);
  glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 0, 0, vp_width_, vp_height_, 0);
  stereo_eye_=old_stereo_eye;
  stereo_projection(0);

#if OST_SHADER_SUPPORT_ENABLED
  Shader::Instance().PushProgram();
  Shader::Instance().Activate("");
#endif

  glDisable(GL_DEPTH_TEST);
  glDisable(GL_LIGHTING);
  glDisable(GL_COLOR_MATERIAL);
  glDisable(GL_FOG);
  glDisable(GL_CULL_FACE);
  glDisable(GL_BLEND);
  glDisable(GL_LINE_SMOOTH);
  glDisable(GL_POINT_SMOOTH);
#if defined(OST_GL_VERSION_2_0)
  glDisable(GL_MULTISAMPLE);
#endif
  glMatrixMode(GL_PROJECTION);
  glPushMatrix();
  glLoadIdentity();
  glOrtho(0,vp_width_,0,vp_height_,-1,1);
  glMatrixMode(GL_MODELVIEW);
  glPushMatrix();
  glLoadIdentity();

  if(stereo_mode_==2) {
    // draw interlaced lines in stencil buffer
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glLineWidth(1.0);
    glEnable(GL_STENCIL_TEST);
    glStencilMask(0x1);
    glClearStencil(0x0);
    glClear(GL_STENCIL_BUFFER_BIT);
    glStencilFunc(GL_ALWAYS,0x1,0x1);
    glStencilOp(GL_REPLACE,GL_REPLACE,GL_REPLACE);
    glBegin(GL_LINES);
    glColor3f(1.0,1.0,1.0);
    for(unsigned int i=0;i<vp_height_;i+=2) {
      glVertex2i(0,i);
      glVertex2i(vp_width_-1,i);
    } 
    glEnd();
    
    glStencilOp(GL_KEEP,GL_KEEP,GL_KEEP);
  }

  // right eye
  if(stereo_mode_==1) {
    glDrawBuffer(GL_BACK_RIGHT);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  } else if(stereo_mode_==2) {
    glStencilFunc(GL_EQUAL,0x0,0x1);
  }
#if OST_SHADER_SUPPORT_ENABLED
  if(OST_GL_VERSION_2_0) {
    glActiveTexture(GL_TEXTURE0);
  }
#endif
  glBindTexture(GL_TEXTURE_2D, stereo_inverted_ ? scene_left_tex_ : scene_right_tex_);
  // draw
  glColor3f(1.0,0.0,1.0);
  glBegin(GL_QUADS);
  glTexCoord2f(0.0,0.0); glVertex2i(0,0);
  glTexCoord2f(0.0,1.0); glVertex2i(0,vp_height_);
  glTexCoord2f(1.0,1.0); glVertex2i(vp_width_,vp_height_);
  glTexCoord2f(1.0,0.0); glVertex2i(vp_width_,0);
  glEnd();

  // left eye
  if(stereo_mode_==1) {
    glDrawBuffer(GL_BACK_LEFT);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  } else if(stereo_mode_==2) {
    glStencilFunc(GL_EQUAL,0x1,0x1);
  }
#if OST_SHADER_SUPPORT_ENABLED
  if(OST_GL_VERSION_2_0) {
    glActiveTexture(GL_TEXTURE0);
  }
#endif
  glBindTexture(GL_TEXTURE_2D, stereo_inverted_ ? scene_right_tex_ : scene_left_tex_);
  // draw
  glColor3f(1.0,0.0,1.0);
  glBegin(GL_QUADS);
  glTexCoord2f(0.0,0.0); glVertex2i(0,0);
  glTexCoord2f(0.0,1.0); glVertex2i(0,vp_height_);
  glTexCoord2f(1.0,1.0); glVertex2i(vp_width_,vp_height_);
  glTexCoord2f(1.0,0.0); glVertex2i(vp_width_,0);
  glEnd();
  
  // restore settings
  glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 0, 0, 0, 0, 0);
  glBindTexture(GL_TEXTURE_2D, 0);
  glDisable(GL_TEXTURE_2D);
  glMatrixMode(GL_PROJECTION);
  glPopMatrix();
  glMatrixMode(GL_MODELVIEW);
  glPopMatrix();
  glPopClientAttrib();
  glPopAttrib();
#if OST_SHADER_SUPPORT_ENABLED
  Shader::Instance().PopProgram();
#endif
}

void Scene::do_autoslab()
{
  // skip autoslab if nothing to show yet
  if(root_node_->GetChildCount()==0) return;
  if(do_autoslab_fast_) {
    geom::AlignedCuboid bb =this->GetBoundingBox(transform_);
    // necessary code duplication due to awkward slab limit impl
    if(bb.GetVolume()==0.0) {
      // skip if empty BB
      return;
    } else {
      float mynear=-(bb.GetMax()[2])-1.0;
      float myfar=-(bb.GetMin()[2])+1.0;
      znear_=mynear;
      zfar_=myfar;
      set_near(mynear);
      set_far(myfar);
    }
  } else {
    LimCalc limcalc;
    limcalc.transform=transform_;
    limcalc.minc = Vec3(std::numeric_limits<float>::max(),
                              std::numeric_limits<float>::max(),
                              std::numeric_limits<float>::max());
    limcalc.maxc = Vec3(-std::numeric_limits<float>::max(),
                              -std::numeric_limits<float>::max(),
                              -std::numeric_limits<float>::max());
    this->Apply(limcalc);
    if(!limcalc.valid) {
      return;
    }
    float mynear=std::max(float(0.0), std::min(float(-limcalc.minc[2]),float(-limcalc.maxc[2])))-float(1.0);
    float myfar=std::max(float(-limcalc.minc[2]),float(-limcalc.maxc[2]))+float(1.0);
    znear_=mynear;
    zfar_=myfar;
    set_near(mynear);
    set_far(myfar);
  }
  ResetProjection();
  RequestRedraw();
}

}} // ns
