//------------------------------------------------------------------------------
// This file is part of the OpenStructure project <www.openstructure.org>
//
// Copyright (C) 2008-2010 by the OpenStructure authors
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

/*
  Author: Andreas Schenk
*/

#include "graphics_image_item.hh"
#include "graphics_image_values_item.hh"
#include <ost/gui/data_viewer/viewer_normalizer.hh>
#include <ost/img/alg/stat_min_max.hh>
#include <QPainter>
#include <QStyleOptionGraphicsItem>
#include <QPixmapCache>
#include <QGraphicsSceneMouseEvent>
#include <QKeyEvent>

uint qHash(const CacheKey& key)
{
  typedef QPair<int,int> IntPair;
  typedef QPair<IntPair,IntPair> IntPairPair;
  return qHash(QPair<ost::img::gui::GraphicsImageItem*,IntPairPair>(key.item_,IntPairPair(IntPair(key.x_,key.y_),IntPair(key.z_,key.binning_))));
}

namespace ost { namespace img { namespace gui {

namespace detail{




template <int MODE>
class ImageFillerBase
{
public:
  ImageFillerBase() {}
  ImageFillerBase(QImage* image,const Point& start,const NormalizerPtr& normalizer,int binning):
    image_(image),
    start_(start),
    norm_(normalizer),
    binning_(binning)
  {
  }

  template <typename T, class D>
  void VisitState(const ImageStateImpl<T,D>& isi)
  {
    image_->fill(QColor(0,0,0,0).rgba());
    Extent ext=isi.GetLogicalExtent();
    int xstart=std::max<int>(ext.GetStart()[0]-start_[0],0);
    int xend=std::min<int>(ext.GetEnd()[0]-start_[0],image_->width()*binning_-1);
    int ystart=std::max<int>(ext.GetStart()[1]-start_[1],0);
    int yend=std::min<int>(ext.GetEnd()[1]-start_[1],image_->height()*binning_-1);
    for(int x=xstart;x<=xend;x+=binning_){
      for(int y=ystart;y<=yend;y+=binning_){
        if(MODE==GraphicsImageItem::GREY){
          uchar v=(unsigned char)::round(norm_->Convert(Val2Val<T,Real>(isi.Value(Point(x,y)+start_))));
          image_->setPixel(x/binning_,y/binning_, QColor(v,v,v).rgb());
        }else if(MODE==GraphicsImageItem::SIGNCOLOR){
          Real rv = Val2Val<T,Real>(isi.Value(Point(x,y)+start_));
          bool sg = rv<0.0;
          QColor color=QColor::fromHsvF(sg ? 0.0 : 1.0/3.0,1.0,norm_->Convert(std::abs(rv))/255.0);
          image_->setPixel(x/binning_,y/binning_, color.rgb());

        }else if(MODE==GraphicsImageItem::PHASECOLOR){
          Complex cv=Val2Val<T,Complex>(isi.Value(Point(x,y)+start_));
          // phase is hue
          // saturation is always 1
          // brightness is amplitude
          QColor color=QColor::fromHsvF(fmod(std::arg(cv)*0.5/M_PI+0.5,1.0),1.0,norm_->Convert(std::abs(cv))/255.0);
          image_->setPixel(x/binning_,y/binning_, color.rgb());
        }
      }
    }
  }

  //overload for HalfFrequencyDomain to correctly handle conjugated values without having to fall back on boundary checked getters
  void VisitState(const ImageStateImpl<Complex,HalfFrequencyDomain>& isi)
  {
    image_->fill(QColor(0,0,0,0).rgba());
    Extent ext=isi.GetLogicalExtent();
    int xstart=std::max<int>(ext.GetStart()[0]-start_[0],0);
    int xend=std::min<int>(ext.GetEnd()[0]-start_[0],image_->width()*binning_-1);
    int ystart=std::max<int>(ext.GetStart()[1]-start_[1],0);
    int yend=std::min<int>(ext.GetEnd()[1]-start_[1],image_->height()*binning_-1);
    for(int x=xstart;x<=xend;x+=binning_){
      for(int y=ystart;y<=yend;y+=binning_){
        Point cp=CONJPOINT(Point(x,y)+start_);
        if(MODE==GraphicsImageItem::GREY){
          uchar v=(unsigned char)::round(norm_->Convert(Val2Val<Complex,Real>(isi.Value(cp))));
          image_->setPixel(x/binning_,y/binning_, QColor(v,v,v).rgb());
        }else if(MODE==GraphicsImageItem::SIGNCOLOR){
          Real rv = Val2Val<Complex,Real>(isi.Value(cp));
          bool sg = rv<0.0;
          QColor color=QColor::fromHsvF(sg ? 0.0 : 1.0/3.0,1.0,norm_->Convert(std::abs(rv))/255.0);
          image_->setPixel(x/binning_,y/binning_, color.rgb());

        }else if(MODE==GraphicsImageItem::PHASECOLOR){
          Complex cv;
          if(cp==Point(x,y)+start_){
            cv=isi.Value(cp);
          }else{
            cv=conj(isi.Value(cp));
          }
          // phase is hue
          // saturation is always 1
          // brightness is amplitude
          QColor color=QColor::fromHsvF(fmod(std::arg(cv)*0.5/M_PI+0.5,1.0),1.0,norm_->Convert(std::abs(cv))/255.0);
          image_->setPixel(x/binning_,y/binning_, color.rgb());
        }
      }
    }
  }

  void VisitFunction(const Function& f)
  {}

  static String GetAlgorithmName()
  {
    return "ImageFiller";
  }

private:
  QImage* image_;
  Point start_;
  NormalizerPtr norm_;
  int binning_;
};



  typedef ImageStateNonModAlgorithm<ImageFillerBase<GraphicsImageItem::GREY> > ImageGreyFiller;
  typedef ImageStateNonModAlgorithm<ImageFillerBase<GraphicsImageItem::SIGNCOLOR> > ImageSignColorFiller;
  typedef ImageStateNonModAlgorithm<ImageFillerBase<GraphicsImageItem::PHASECOLOR> > ImagePhaseColorFiller;



  void greyscale_filler(QImage* image,const Data& data,const Point& start,const NormalizerPtr& normalizer,int binning)
 {
   ImageGreyFiller filler_alg(image,start,normalizer,binning);
   data.Apply(filler_alg);
 }

 void signcolor_filler(QImage* image,const Data& data,const Point& start,const NormalizerPtr& normalizer,int binning)
 {
   ImageSignColorFiller filler_alg(image,start,normalizer,binning);
   data.Apply(filler_alg);
 }

 void phasecolor_filler(QImage* image,const Data& data,const Point& start,const NormalizerPtr& normalizer,int binning)
 {
   ImagePhaseColorFiller filler_alg(image,start,normalizer,binning);
   data.Apply(filler_alg);
 }

} //ns

GraphicsImageItem::GraphicsImageItem( const Data& data,QGraphicsItem* parent):
  QGraphicsObject(parent),
  DataObserver(data),
  cmode_(GREY),
  slab_(GetObservedData().GetExtent().GetCenter()[2]),
  normalizer_(ViewerNormalizerPtr(new ViewerNormalizer)),
  rubberband_(new QGraphicsRectItem(this)),
  cached_keys_(),
  value_display_(new GraphicsImageValuesItem(data,this))
{
  setFlag(QGraphicsItem::ItemUsesExtendedStyleOption);
  setFlag(QGraphicsItem::ItemIsFocusable);
  setAcceptHoverEvents(true);

  rubberband_->setBrush(QBrush(QColor(255,255,0,20)));
  rubberband_->setPen(QPen(QColor(255,255,0,255)));
  rubberband_->setVisible(false);

  Renormalize();
}
QCache<CacheKey,QImage> GraphicsImageItem::cache_(1000);
const int GraphicsImageItem::blocksize_=128;

void 	GraphicsImageItem::paint( QPainter * painter, const QStyleOptionGraphicsItem * option, QWidget * widget)
{
  Real scale=painter->transform().m11();
  int binning=std::max<int>(1,static_cast<int>(1/scale));
  //painter->setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing, antialiasing_);
  void (*filler)(QImage*,const Data&,const Point&,const NormalizerPtr&,int)=0;
  switch(cmode_){
  case SIGNCOLOR:
    filler=detail::signcolor_filler;
    break;
  case PHASECOLOR:
    filler=detail::phasecolor_filler;
    break;
  default:
    filler=detail::greyscale_filler;
    break;
  }

  QRectF intersect = option->exposedRect & boundingRect();
  int xstart=static_cast<int>(floor(intersect.left()/(blocksize_*binning)));
  int xend=static_cast<int>(ceil(intersect.right()/(blocksize_*binning)));
  int ystart=static_cast<int>(floor(intersect.top()/(blocksize_*binning)));
  int yend=static_cast<int>(ceil(intersect.bottom()/(blocksize_*binning)));


  for(int x=xstart;x<xend;++x){
    for(int y=ystart;y<yend;++y){
      CacheKey cache_key(this,x,y,slab_,binning);
      if (( !cache_.contains(cache_key))) {
        QImage* tile=new QImage(blocksize_,blocksize_,QImage::Format_ARGB32);
        filler(tile,GetObservedData(),Point(x*(blocksize_*binning),y*(blocksize_*binning),slab_),normalizer_,binning);
        cache_.insert(cache_key,tile);
        cached_keys_<< cache_key;
        painter->drawImage(QRectF(QPointF(x*(blocksize_*binning),y*(blocksize_*binning)),QSize(blocksize_*binning,blocksize_*binning)),*tile);
        // next line only used for cache debugging
        //painter.drawLine(x*blocksize_,y*blocksize_,(x+1)*blocksize_,(y+1)*blocksize_-);
      }else{
        QImage* tile=cache_.object(cache_key);
        painter->drawImage(QRectF(QPointF(x*(blocksize_*binning),y*(blocksize_*binning)),QSize(blocksize_*binning,blocksize_*binning)),*tile);
      }
      // next line only used for cache debugging
      //painter.drawRect(x*blocksize_,y*blocksize_,blocksize_,blocksize_);
    }
  }
  painter->setPen(Qt::white);
  painter->drawRect(boundingRect());
}

QRectF GraphicsImageItem::boundingRect() const
{
  Extent ext=GetObservedData().GetExtent();
  return QRectF(ext.GetStart()[0],ext.GetStart()[1],ext.GetSize()[0],ext.GetSize()[1]);
}

void GraphicsImageItem::ObserverUpdate()
{
  ClearCache();
  update();
}

void GraphicsImageItem::ObserverUpdate(const Extent& e)
{
  ClearCacheRegion(e);
  update();
}

void GraphicsImageItem::ObserverUpdate(const Point& p)
{
  ClearCacheRegion(Extent(p,p));
  update();
}

void GraphicsImageItem::ObserverRelease()
{
  //todo: handle observer release
}

void 	GraphicsImageItem::mouseMoveEvent(QGraphicsSceneMouseEvent * event)
{
  if(event->buttons() & Qt::LeftButton){
    rubberband_->setVisible(true);
    rubberband_->setRect(QRectF(event->buttonDownPos(Qt::LeftButton).toPoint(),event->pos().toPoint()).normalized());
  }
}

void 	GraphicsImageItem::mousePressEvent(QGraphicsSceneMouseEvent * event)
{
  if(event->button()==Qt::LeftButton){
    rubberband_->setVisible(false);
    rubberband_->setRect(QRectF());
    event->accept();
  }else{
    event->ignore();
  }
}

void 	GraphicsImageItem::mouseReleaseEvent(QGraphicsSceneMouseEvent * event)
{
  if(event->button()==Qt::LeftButton){
    if(rubberband_->rect().width()==0 || rubberband_->rect().height()==0){
      rubberband_->setVisible(false);
      emit deselected();
    }else{
      QRectF rect=rubberband_->rect();
      emit selected(Extent(Point(rect.left(),rect.top()),Point(rect.right(),rect.bottom())),GetObservedData());
    }
    event->accept();
  }else{
    event->ignore();
  }
}
void GraphicsImageItem::Renormalize()
{
  ClearCache();
  alg::StatMinMax s;
  if(HasSelection()){
    ImageHandle tmp=CreateImage(GetSelection());
    tmp.Paste(GetObservedData());
    tmp.Apply(s);
  }else{
    GetObservedData().Apply(s);
  }
  normalizer_=ViewerNormalizerPtr(new ViewerNormalizer(s.GetMinimum(),s.GetMaximum(),0.0,255.0,1.0,GetInvert()));
  update();
}


Extent GraphicsImageItem::GetSelection() const
{
  if(!HasSelection()){
    return Extent();
  }
  QRectF rect=rubberband_->rect();
  return Extent(Point(rect.left(),rect.top(),slab_),Point(rect.right(),rect.bottom(),slab_)-Point(1,1));
}

bool GraphicsImageItem::HasSelection() const
{
  return rubberband_->isVisible();
}

void GraphicsImageItem::keyPressEvent(QKeyEvent* event)
{
  switch(event->key()) {
  case 'V':
    value_display_->setVisible(! value_display_->isVisible());
    update();
    break;
  case 'I':
    SetInvert(!GetInvert());
    break;
  case 'N':
    Renormalize();
    break;
  case 'Z':
  case 'Y':
    MoveSlab(+1);
    break;
  case 'X':
    MoveSlab(-1);
    break;
//  case 'M': selection_mode_=(selection_mode_+1)%3; break;
//  case 'V': SetDisplayPixelValues(!display_pixel_values_);break;
  case 'P':
    if(cmode_==PHASECOLOR){
      cmode_=GREY;
      ClearCache();
      update();
    }else{
      cmode_=PHASECOLOR;
      ClearCache();
      update();
    }
    break;
  case 'S':
    if(cmode_==SIGNCOLOR){
      cmode_=GREY;
      ClearCache();
      update();
    }else{
      cmode_=SIGNCOLOR;
      ClearCache();
      update();
    }
    break;
  case 'G':
    cmode_=GREY;
    ClearCache();
    update();
    break;
  default:
    event->ignore();
    break;
  }
}

void GraphicsImageItem::SetInvert(bool invert)
{
  ClearCache();
  normalizer_->SetInvert(invert);
  update();

}

bool GraphicsImageItem::GetInvert() const
{
 return normalizer_->GetInvert();
}

void GraphicsImageItem::ClearCache()
{
  for (int i = 0; i < cached_keys_.size(); ++i) {
    cache_.remove(cached_keys_[i]);
  }
  cached_keys_.clear();
}

void GraphicsImageItem::ClearCacheRegion(const Extent& e)
{
  int xstart=static_cast<int>(floor(static_cast<Real>(e.GetStart()[0])/blocksize_));
  int xend=static_cast<int>(ceil(static_cast<Real>(e.GetEnd()[0])/blocksize_));
  int ystart=static_cast<int>(floor(static_cast<Real>(e.GetStart()[1])/blocksize_));
  int yend=static_cast<int>(ceil(static_cast<Real>(e.GetEnd()[1])/blocksize_));
  int zstart=e.GetStart()[2];
  int zend=e.GetEnd()[2];

  for(int x=xstart;x<xend;++x){
    for(int y=ystart;y<yend;++y){
      for(int z=zstart;z<zend;++z){
        for(int binning=1;binning<=16; binning<<=1){
          CacheKey cache_key(this,x,y,slab_,binning);
          cache_.remove(cache_key);
          cached_keys_.removeOne(cache_key);
        }
      }
    }
  }
}

void GraphicsImageItem::MoveSlab(int n)
{
  slab_=std::min<int>(std::max<int>(slab_+n,GetObservedData().GetExtent().GetStart()[2]),GetObservedData().GetExtent().GetEnd()[2]);
  update();
  emit SlabChanged(GetSlab());
}

ViewerNormalizerPtr GraphicsImageItem::GetNormalizer() const
{
  return normalizer_;
}

int GraphicsImageItem::GetSlab() const
{
  return slab_;
}

void GraphicsImageItem::hoverMoveEvent(QGraphicsSceneHoverEvent * event)
{
  QPointF pos=mapFromScene(event->scenePos());
  if(GetObservedData().GetType()==REAL){
    emit MousePositionReal(pos,GetObservedData().GetReal(Point(floor(pos.x()),floor(pos.y()))));
  }else{
    emit MousePositionComplex(pos,GetObservedData().GetComplex(Point(floor(pos.x()),floor(pos.y()))));
  }
}

void GraphicsImageItem::focusOutEvent(QFocusEvent* event)
{
  rubberband_->setVisible(false);
}

}}}  //ns
