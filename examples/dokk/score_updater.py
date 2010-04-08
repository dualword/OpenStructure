import os, threading
from PyQt4 import QtCore, QtGui
from ost import mol, qa

class ScoreUpdater(QtCore.QObject):
  def __init__(self, level, parent=None):
    QtCore.QObject.__init__(self, parent)
    self.level = level
    self.old_atoms = list()

  def UpdateScores(self):
    for atom in self.old_atoms:
      atom.SetGenericFloatProperty('clash', 0)
    
    ligand = self.level.ligand
    prot_within = list()
    ligand_atoms = ligand.handle.atoms
    for ligand_atom in ligand_atoms:
      within=self.level.protein.handle.FindWithin(ligand_atom.GetPos(), 3)
      for atom in within:
        prot_within.append(atom)
    lig_view=ligand.handle.CreateFullView()
    for a in prot_within:
      score=qa.ClashScore(a, lig_view)
      a.SetGenericFloatProperty('clash', score)
    self.level.surface.go.ReapplyColorOps()
    self.old_atoms = prot_within
