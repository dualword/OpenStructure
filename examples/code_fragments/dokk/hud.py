import math
import time
from ost import gfx
from PyQt4.QtGui import *
from PyQt4.QtCore import *
from PyQt4.QtOpenGL import *

class HUD(QObject):
  def __init__(self, parent=None):
    QObject.__init__(self,parent)
    self.objects = list()
    
  def Add(self, hud_object):
    if hud_object in self.objects:
      self.objects.remove(hud_object)
    self.objects.append(hud_object)
    hud_object.Appended(self)
    self.emit(SIGNAL('HUDChanged()'))

  def Remove(self, hud_object):
    if hud_object in self.objects:
      self.objects.remove(hud_object)
      self.emit(SIGNAL('HUDChanged()'))
        
  def Paint(self, painter):
    for hud_object in self.objects:
      hud_object.Paint(painter)
  
  def Clear(self):
    del(self.objects[:])
  
class HUDObject(QObject):
  def __init__(self, time, parent=None):
    QObject.__init__(self, parent)
    self.time = time
    self.timer = QTimer()
    self.timer.setSingleShot(True)
    self.hud = None
    self.endtime = None
    QObject.connect(self.timer, SIGNAL("timeout()"), self.Timeout)
    
  def Appended(self, hud):
    self.hud = hud
    if self.time >= 0:
      self.timer.start(self.time)
      self.endtime = float(time.time()+(self.time)/1000.0)
    
  def Timeout(self):
    self.timer.stop()
    if self.hud is not None:
      self.hud.Remove(self)
    self.emit(SIGNAL('Finished()'))
    
  def Reset(self):
    self.timer.stop()
    if self.hud is not None:
      self.hud.Add(self)
    
class TextHUDObject(HUDObject):
  def __init__(self, text, pos, time, color=QColor(255,255,255), font=QFont("Verdana"), parent=None):
    HUDObject.__init__(self,time,parent)
    self.text = text
    self.pos = pos
    self.rect = None
    self.color = color
    self.font = font
  
  def Paint(self, painter):
    if self.endtime is not None:
      rem_time = (self.endtime-float(time.time()))*1000.0
      if rem_time > 0:
        self.color.setAlpha((rem_time/self.time)*255)
    painter.setPen(QPen(self.color, Qt.SolidLine))
    painter.setFont(self.font)
    if self.pos is not None:
      painter.drawText(self.pos, self.text)
    if self.rect is not None:
      painter.drawText(self.rect, self.align, self.text)

class RectTextHUDObject(HUDObject):
  def __init__(self, text, rect, time, align=Qt.AlignCenter, color=QColor(255,255,255), font=QFont("Verdana"), parent=None):
    HUDObject.__init__(self,time,parent)
    self.text = text
    self.rect = rect
    self.align = align
    self.color = color
    self.font = font
    
  def Paint(self, painter):
    if self.endtime is not None:
      rem_time = (self.endtime-float(time.time()))*1000.0
      if rem_time > 0:
        self.color.setAlpha((rem_time/self.time)*255)
    painter.setPen(QPen(self.color, Qt.SolidLine))
    painter.setFont(self.font)
    painter.drawText(self.rect, self.align, self.text)

class RectHUDObject(HUDObject):
  def __init__(self, time, rect, fade=False, bg_color=QColor(255,255,255), border_color=QColor(255,255,255), parent=None):
    HUDObject.__init__(self,time,parent)
    self.rect = rect
    self.fade = fade
    self.bg_color = bg_color
    self.border_color = border_color

  def Paint(self, painter):
    if self.fade:
      rem_time = (self.endtime-float(time.time()))*1000.0
      if rem_time >0:
        alpha = (rem_time/self.time)*255
        self.bg_color.setAlpha(alpha)
        self.border_color.setAlpha(alpha)
    painter.setPen(self.border_color)
    painter.setBrush(self.bg_color)
    painter.drawRect(self.rect)
    painter.setPen(QPen(self.border_color, Qt.SolidLine))

class ImgHUDObject(HUDObject):
  def __init__(self, time, pos, img_loc="", parent=None):
    HUDObject.__init__(self,time,parent)
    self.pos = pos
    self.img = QImage(img_loc)

  def Paint(self, painter):
    painter.drawImage(self.pos, self.img)
  