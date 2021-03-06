import dokk
from PyQt4 import QtCore, QtGui
from hud import *
from ost import gui
from level_info import HighScore

ALPHABET = ['A','B','C','D','E','F','G','H','I','J','K','L','M','N','O','P','Q','R','S','T','U','V','W','X','Y','Z','0','1','2','3','4','5','6','7','8','9']
LETTER_BREAK = 3000
NAME_BREAK = 8000

class NameEnter(QtCore.QObject):
  def __init__(self, parent=None):
    QtCore.QObject.__init__(self, parent)
    try:
      self._spnav = gui.SpnavInput.GetQThread()
      self._spnav.start()
    except AttributeError:
      self._spnav = None  
    self.Reset()
    
  def Reset(self):
    self.letter_pos = 0
    self.al_len = len(ALPHABET)
    self.letter_score = 0
    
    self.name = "AAA"
    self.name_pos = 0
    self.name_len = len(self.name)
    self.name_score = 0
    
  def InputChanged(self, tx,ty,tz,rx,ry,rz):
    if(abs(tx) > abs(tz)):
      self.name_score += tx
      if(self.name_score > NAME_BREAK):
        self.name_pos +=1
        self.__SetNamePos()
      elif(self.name_score < -NAME_BREAK):
        self.name_pos -=1
        self.__SetNamePos()      
    else:
      self.letter_score += tz
      if(self.letter_score > LETTER_BREAK):
        self.letter_pos +=1
        self.__SetLetterPos()
      elif(self.letter_score < -LETTER_BREAK):
        self.letter_pos -=1
        self.__SetLetterPos()
    
  def KeyPressed(self, event):
    if event.key() == Qt.Key_Right:
      self.name_pos +=1
      self.__SetNamePos()
    elif event.key() == Qt.Key_Left:
      self.name_pos -=1
      self.__SetNamePos()
    elif event.key() == Qt.Key_Up:
      self.letter_pos +=1
      self.__SetLetterPos()
    elif event.key() == Qt.Key_Down:
      self.letter_pos -=1
      self.__SetLetterPos()
    elif event.key() == Qt.Key_Enter or event.key() == Qt.Key_Return:
      self.emit(QtCore.SIGNAL("Finished()"))
        
  def ToggleInputMode(self, button):
    if button == 10:
      self.emit(QtCore.SIGNAL("Finished()"))
  
  def GetName(self):
    return self.name
  
  def Start(self):
    self.Reset()
    if self._spnav:
      QtCore.QObject.connect(self._spnav,QtCore.SIGNAL("deviceTransformed(int,int,int,int,int,int)"), self.InputChanged)
      QtCore.QObject.connect(self._spnav,QtCore.SIGNAL("deviceButtonPressed(int)"), self.ToggleInputMode)     
    self.connect(dokk.Dokk().gl_win.canvas, QtCore.SIGNAL("KeyPressed"),self.KeyPressed)
    
  def Stop(self):
    if self._spnav:
      QtCore.QObject.disconnect(self._spnav,QtCore.SIGNAL("deviceTransformed(int,int,int,int,int,int)"), self.InputChanged)
      QtCore.QObject.disconnect(self._spnav,QtCore.SIGNAL("deviceButtonPressed(int)"), self.ToggleInputMode)
    self.disconnect(dokk.Dokk().gl_win.canvas, QtCore.SIGNAL("KeyPressed"),self.KeyPressed)
    
  def __SetLetterPos(self):
    self.letter_pos = self.letter_pos % self.al_len
    self.__SetName()
    self.letter_score = 0
    
  def __SetNamePos(self):
    self.name_pos = self.name_pos % self.name_len    
    for i in range(0,self.al_len):
      if ALPHABET[i] == self.name[self.name_pos]:
        self.letter_pos = i
    self.name_score = 0
    self.emit(QtCore.SIGNAL("Changed()"))
        
  def __SetName(self):
    self.name = self.name[0:self.name_pos] + ALPHABET[self.letter_pos] + self.name[self.name_pos+1:]
    self.emit(QtCore.SIGNAL("Changed()"))
    
class HUDNameInput(QtCore.QObject):
  def __init__(self, parent=None):
    QtCore.QObject.__init__(self, parent)
    self.ne = NameEnter()
    self.connect(self.ne,QtCore.SIGNAL("Changed()"),self.Update)
    self.connect(self.ne,QtCore.SIGNAL("Finished()"),self.Finish)
    self.huds = list()
    self.high_score = HighScore(None)
    self.hud_text = None
    self.bg = None
      
  def Start(self):
    self.high_score.topten = dokk.Dokk().GetLevel().topten
    self.high_score.Start()
    self.ne.Start()
    del(self.huds[:])
    rect = QtCore.QRect(QtCore.QPoint(60, 60), QtCore.QSize(dokk.Dokk().gl_win.Width()-120, 220))
    self.bg = RectHUDObject(-1,rect, bg_color=QtGui.QColor(128,128,128,200))
    dokk.Dokk().gl_win.AddHUDObject(self.bg)
    self.hud_text = RectTextHUDObject(dokk.Dokk().GetLevel().config.Name["TEXT"], rect=rect, time=-1,font = QtGui.QFont("Verdana",30))
    dokk.Dokk().gl_win.AddHUDObject(self.hud_text)
    
    font = QtGui.QFont("Courier", 30)
    xpos = (dokk.Dokk().gl_win.Width()/2) - ((len(self.ne.GetName())*30)/2)
    for i in range(0,len(self.ne.GetName())):
      hud_obj = TextHUDObject(self.ne.GetName()[i], QtCore.QPoint(xpos,250), -1,font = font)
      if self.ne.name_pos == i:
        hud_obj.color = QtGui.QColor(255,255,0)
      else:
        hud_obj.color = QtGui.QColor(255,255,255)
      self.huds.append(hud_obj)
      dokk.Dokk().gl_win.AddHUDObject(hud_obj)
      xpos += 30
    
  def GetName(self):
    return self.ne.GetName()
  
  def Update(self):
    for i in range(0,len(self.ne.GetName())):
      self.huds[i].text = self.ne.GetName()[i]
      if self.ne.name_pos == i:
        self.huds[i].color = QtGui.QColor(255,255,0)
      else:
        self.huds[i].color = QtGui.QColor(255,255,255)
        
  def Stop(self):
    self.high_score.Finish()
    self.ne.Stop()
    for hud in self.huds:
      dokk.Dokk().gl_win.RemoveHUDObject(hud)
    if self.hud_text:
      dokk.Dokk().gl_win.RemoveHUDObject(self.hud_text)
    if self.bg:
      dokk.Dokk().gl_win.RemoveHUDObject(self.bg)
      
  def Finish(self):
    self.Stop()
    self.emit(QtCore.SIGNAL("Finished()"))