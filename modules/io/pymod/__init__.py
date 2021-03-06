#------------------------------------------------------------------------------
# This file is part of the OpenStructure project <www.openstructure.org>
#
# Copyright (C) 2008-2011 by the OpenStructure authors
#
# This library is free software; you can redistribute it and/or modify it under
# the terms of the GNU Lesser General Public License as published by the Free
# Software Foundation; either version 3.0 of the License, or (at your option)
# any later version.
# This library is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
# FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for more
# details.
#
# You should have received a copy of the GNU Lesser General Public License
# along with this library; if not, write to the Free Software Foundation, Inc.,
# 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
#------------------------------------------------------------------------------
import os, tempfile, ftplib, httplib

from _ost_io import *
from ost import mol, geom, conop

profiles=None

class IOProfiles:
  def __init__(self):
     self._dict={}

  def __getitem__(self, key):
    return IOProfileRegistry.Instance().Get(key)

  def __setitem__(self, key, value):
    if isinstance(value, str):
      value=self[value]
    IOProfileRegistry.Instance().Set(key, value)
    self._dict[key]=value

  def __len__(self):
    return len(self._dict)

  def __iter__(self):
    return self._dict.__iter__()

if not profiles:
  profiles=IOProfiles()
  profiles['STRICT']=IOProfile(dialect='PDB', fault_tolerant=False,
                               strict_hydrogens=False, quack_mode=False)
  profiles['SLOPPY']=IOProfile(dialect='PDB', fault_tolerant=True,
                               strict_hydrogens=False, quack_mode=True)
  profiles['CHARMM']=IOProfile(dialect='CHARMM', fault_tolerant=True,
                               strict_hydrogens=False, quack_mode=False)
  profiles['DEFAULT']='STRICT'

def _override(val1, val2):
  if val2!=None:
    return val2
  else:
    return val1

def __GetModelFromPDB(model_id, output_dir, file_pattern='pdb%s.ent.gz'):
  file_name = file_pattern % model_id
  file_path = os.path.join(output_dir,file_name)
  try:
    server="ftp.wwpdb.org"
    ftp=ftplib.FTP(server,"anonymous","user@")
    ftp.cwd("pub/pdb/data/structures/all/pdb")
    ftp_retrfile=open(file_path,"wb")
    ftp.retrbinary("RETR "+file_name,ftp_retrfile.write)
    ftp_retrfile.close()
  except:
    conn=httplib.HTTPConnection('www.pdb.org')
    conn.request('GET', '/pdb/files/%s.pdb.gz' % model_id )
    response=conn.getresponse()
    if response.status==200:
      data=response.read()
      f=open(os.path.join(output_dir, file_pattern % model_id), 'w+')
      f.write(data)
      f.close()
    else:
      conn.close()
      return False
  return os.path.getsize(file_path) > 0

def LoadPDB(filename, restrict_chains="", no_hetatms=None,
            fault_tolerant=None, load_multi=False, quack_mode=None,
            join_spread_atom_records=None, calpha_only=None,
            profile='DEFAULT', remote=False, dialect=None,
            strict_hydrogens=None, seqres=False):
  """
  Load PDB file from disk and return one or more entities. Several options 
  allow to customize the exact behaviour of the PDB import. For more information 
  on these options, see :doc:`profile`.
  
  Residues are flagged as ligand if they are mentioned in a HET record.

  :param restrict_chains: If not an empty string, only chains listed in the
     string will be imported.

  :param fault_tolerant: Enable/disable fault-tolerant import. If set, overrides 
     the value of :attr:`IOProfile.fault_tolerant`.

  :param no_hetatms: If set to True, HETATM records will be ignored. Overrides 
      the value of :attr:`IOProfile.no_hetatms`

  :param load_multi: If set to True, a list of entities will be returned instead
      of only the first. This is useful when dealing with multi-PDB files.

  :param join_spread_atom_records: If set, overrides the value of 
      :attr:`IOProfile.join_spread_atom_records`.
  
  :param remote: If set to true, the method tries to load the pdb from the 
     remote pdb repository www.pdb.org. The filename is then interpreted as the 
     pdb id.
     
  :rtype: :class:`~ost.mol.EntityHandle` or a list thereof if `load_multi` is 
      True.

  :param dialect: Specifies the particular dialect to use. If set, overrides 
    the value of :attr:`IOProfile.dialect`

  :param seqres: Whether to read SEQRES records. If set to true, the loaded 
    entity and seqres entry will be returned as a tuple.

  :type dialect: :class:`str`
  
  :param strict_hydrogens: If set, overrides the value of 
     :attr:`IOProfile.strict_hydrogens`.

  :raises: :exc:`~ost.io.IOException` if the import fails due to an erroneous or 
      inexistent file
  """
  def _override(val1, val2):
    if val2!=None:
      return val2
    else:
      return val1
  if isinstance(profile, str):
    prof=profiles[profile].Copy()
  else:
    prof=profile.Copy()
  if dialect not in (None, 'PDB', 'CHARMM',):
    raise ValueError('dialect must be PDB or CHARMM')
  prof.calpha_only=_override(prof.calpha_only, calpha_only)
  prof.no_hetatms=_override(prof.no_hetatms, no_hetatms)
  prof.dialect=_override(prof.dialect, dialect)
  prof.quack_mode=_override(prof.quack_mode, quack_mode)
  prof.strict_hydrogens=_override(prof.strict_hydrogens, strict_hydrogens)
  prof.fault_tolerant=_override(prof.fault_tolerant, fault_tolerant)
  prof.join_spread_atom_records=_override(prof.join_spread_atom_records,
                                          join_spread_atom_records)

  if remote:
    output_dir = tempfile.gettempdir()
    if __GetModelFromPDB(filename, output_dir):
      filename = os.path.join(output_dir, 'pdb%s.ent.gz' % filename)
    else:
      raise IOError('Can not load PDB %s from www.pdb.org'%filename) 
  
  conop_inst=conop.Conopology.Instance()
  builder=conop_inst.GetBuilder("DEFAULT")
  if prof.dialect=='PDB':
    builder.dialect=conop.PDB_DIALECT
  elif prof.dialect=='CHARMM':
    builder.dialect=conop.CHARMM_DIALECT
  builder.strict_hydrogens=prof.strict_hydrogens
  reader=PDBReader(filename, prof)
  reader.read_seqres=seqres
  try:
    if load_multi:
      ent_list=[]
      while reader.HasNext():
        ent=mol.CreateEntity()
        reader.Import(ent, restrict_chains)
        conop_inst.ConnectAll(builder, ent, 0)
        ent_list.append(ent)
      if len(ent_list)==0:
        raise IOError("File '%s' doesn't contain any entities" % filename)
      return ent_list
    else:
      ent=mol.CreateEntity()
      if reader.HasNext():
        reader.Import(ent, restrict_chains)
        conop_inst.ConnectAll(builder, ent, 0)
      else:
        raise IOError("File '%s' doesn't contain any entities" % filename)
      if seqres:
        return ent, reader.seqres
      return ent
  except:
    raise

def SavePDB(models, filename, dialect=None,  pqr=False, profile='DEFAULT'):
  """
  Save entity or list of entities to disk. If a list of entities is supplied
  the PDB file will be saved as a multi PDB file. Each of the entities is
  wrapped into a MODEL/ENDMDL pair.

  If the atom number exceeds 99999, '*****' is used.

  :param models: The entity or list of entities (handles or views) to be saved
  :param filename: The filename
  :type  filename: string
  """
  if not getattr(models, '__len__', None):
    models=[models]
  if isinstance(profile, str):
    profile=profiles[profile].Copy()
  else:
    profile.Copy()
  profile.dialect=_override(profile.dialect, dialect)
  writer=PDBWriter(filename, profile)
  writer.SetIsPQR(pqr)
  if len(models)>1:
    writer.write_multi_model=True
  for model in models:
    writer.Write(model)

try:
  from ost import img
  LoadMap = LoadImage
  SaveMap = SaveImage
except ImportError:
  pass

 ## loads several images and puts them in an ImageList
 # \sa \ref fft_li.py "View Fourier Transform Example"
def LoadImageList (files):
  image_list=img.ImageList()
  for file in files:
    image=LoadImage(file)
    image_list.append(image)
  return image_list

LoadMapList=LoadImageList

def LoadCHARMMTraj(crd, dcd_file=None, profile='CHARMM',
                   lazy_load=False, stride=1, 
                   dialect=None):
  """
  Load CHARMM trajectory file.
  
  :param crd: EntityHandle or filename of the (PDB) file containing the
      structure. The structure must have the same number of atoms as the 
      trajectory
  :param dcd_file: The filename of the DCD file. If not set, and crd is a 
      string, the filename is set to the <crd>.dcd
  :param layz_load: Whether the trajectory should be loaded on demand. Instead 
      of loading the complete trajectory into memory, the trajectory frames are 
      loaded from disk when requested.
  :param stride: The spacing of the frames to load. When set to 2, for example, 
      every second frame is loaded from the trajectory. By default, every frame 
      is loaded.
  :param dialect: The dialect for the PDB file to use. See :func:`LoadPDB`. If 
      set, overrides the value of the profile
  :param profile: The IO profile to use for loading the PDB file. See 
      :doc:`profile`.
  """
  if not isinstance(crd, mol.EntityHandle):
    if dcd_file==None:
      dcd_file='%s.dcd' % os.path.splitext(crd)[0]    
    crd=LoadPDB(crd, profile=profile, dialect=dialect)

  else:
    if not dcd_file:
      raise ValueError("No DCD filename given")
  return LoadCHARMMTraj_(crd, dcd_file, stride, lazy_load)

def LoadMMCIF(filename, restrict_chains="", fault_tolerant=None, calpha_only=None, profile='DEFAULT', remote=False, strict_hydrogens=None, seqres=False, info=False):
  """
  Load MMCIF file from disk and return one or more entities. Several options 
  allow to customize the exact behaviour of the MMCIF import. For more
  information on these options, see :doc:`profile`.
  
  Residues are flagged as ligand if they are mentioned in a HET record.

  :param restrict_chains: If not an empty string, only chains listed in the
     string will be imported.

  :param fault_tolerant: Enable/disable fault-tolerant import. If set, overrides
     the value of :attr:`IOProfile.fault_tolerant`.
  
  :param remote: If set to true, the method tries to load the pdb from the 
     remote pdb repository www.pdb.org. The filename is then interpreted as the 
     pdb id.
     
  :rtype: :class:`~ost.mol.EntityHandle`.
  
  :param strict_hydrogens: If set, overrides the value of 
     :attr:`IOProfile.strict_hydrogens`.

  :param seqres: Whether to read SEQRES records. If set to true, the loaded 
    entity and seqres entry will be returned as second item.

  :param info: Whether to return an info container with the other output.
               Returns a :class:`MMCifInfo` object as last item.

  :raises: :exc:`~ost.io.IOException` if the import fails due to an erroneous
  or inexistent file
  """
  def _override(val1, val2):
    if val2!=None:
      return val2
    else:
      return val1
  if isinstance(profile, str):
    prof = profiles[profile].Copy()
  else:
    prof = profile.Copy()

  prof.calpha_only=_override(prof.calpha_only, calpha_only)
  prof.strict_hydrogens=_override(prof.strict_hydrogens, strict_hydrogens)
  prof.fault_tolerant=_override(prof.fault_tolerant, fault_tolerant)

  if remote:
    output_dir = tempfile.gettempdir()
    if __GetModelFromPDB(filename, output_dir):
      filename = os.path.join(output_dir, 'pdb%s.ent.gz' % filename)
    else:
      raise IOError('Can not load PDB %s from www.pdb.org'%filename) 
  
  conop_inst = conop.Conopology.Instance()
  builder = conop_inst.GetBuilder("DEFAULT")

  builder.strict_hydrogens = prof.strict_hydrogens

  try:
    ent = mol.CreateEntity()
    reader = MMCifReader(filename, ent, prof)
    reader.read_seqres = seqres
    #if reader.HasNext():
    reader.Parse()
    conop_inst.ConnectAll(builder, ent, 0)
    #else:
    #  raise IOError("File doesn't contain any entities")
    if seqres and info:
      return ent, reader.seqres, reader.info
    if seqres:
      return ent, reader.seqres
    if info:
      return ent, reader.info
    return ent
  except:
    raise

# this function uses a dirty trick: should be a member of MMCifInfoBioUnit
# which is totally C++, but we want the method in Python... so we define it
# here (__init__) and add it as a member to the class. With this, the first
# arguement is the usual 'self'.
# documentation for this function was moved to mmcif.rst,
# MMCifInfoBioUnit.PDBize, since this function is not included in SPHINX.
def _PDBize(biounit, asu, seqres=None, min_polymer_size=10):
  def _CopyAtoms(src_res, dst_res, edi, trans=geom.Mat4()):
    for atom in src_res.atoms:
      tmp_pos = geom.Vec4(atom.pos)
      new_atom=edi.InsertAtom(dst_res, atom.name, geom.Vec3(trans*tmp_pos), 
                              element=atom.element,
                              occupancy=atom.occupancy, 
                              b_factor=atom.b_factor,
                              is_hetatm=atom.is_hetatom)

  chain_names='ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789abcdefghijklmnopqrstuvwxyz'
  # create list of operations
  # for cartesian products, operations are stored in a list, multiplied with
  # the next list of operations and re-stored... until all lists of operations
  # are multiplied in an all-against-all manner.
  operations = biounit.GetOperations()
  trans_matrices = list()
  if len(operations) > 0:
    for op in operations[0]:
      rot = geom.Mat4()
      rot.PasteRotation(op.rotation)
      trans = geom.Mat4()
      trans.PasteTranslation(op.translation)
      tr = geom.Mat4()
      tr = trans * rot
      trans_matrices.append(tr)
      for op_n in range(1, len(operations)):
        tmp_ops = list()
        for o in operations[op_n]:
          rot = geom.Mat4()
          rot.PasteRotation(o.rotation)
          trans = geom.Mat4()
          trans.PasteTranslation(o.translation)
          tr = geom.Mat4()
          tr = trans * rot
          for t_o in trans_matrices:
            tp = t_o * tr
            tmp_ops.append(tp)
        trans_matrices = tmp_ops
  # select chains into a view as basis for each transformation
  assu = asu.Select('cname=' + ','.join(biounit.GetChainList()))
  # use each transformation on the view, store as entity and transform, PDBize
  # the result while adding everything to one large entity
  pdb_bu = mol.CreateEntity()
  edi = pdb_bu.EditXCS(mol.BUFFERED_EDIT)
  cur_chain_name = 0
  water_chain = mol.ChainHandle()
  ligand_chain = mol.ChainHandle()
  for tr in trans_matrices:
    # do a PDBize, add each new entity to the end product
    for chain in assu.chains:
      residue_count = len(chain.residues)
      if seqres:
        seqres_chain = seqres.FindSequence(chain.name)
        if seqres_chain.IsValid():
          residue_count = len(seqres_chain)
      if chain.is_polymer and residue_count >= min_polymer_size:
        if len(chain_names) == cur_chain_name:
          raise RuntimeError('Running out of chain names')
        new_chain = edi.InsertChain(chain_names[cur_chain_name])
        cur_chain_name += 1
        edi.SetChainDescription(new_chain, chain.description)
        edi.SetChainType(new_chain, chain.type)
        new_chain.SetStringProp('original_name', chain.name)
        for res in chain.residues:
          new_res = edi.AppendResidue(new_chain, res.name, res.number)
          _CopyAtoms(res, new_res, edi, tr)
      elif chain.type == mol.CHAINTYPE_WATER:
        if not water_chain.IsValid():
          # water gets '-' as name
          water_chain = edi.InsertChain('-')
          edi.SetChainDescription(water_chain, chain.description)
          edi.SetChainType(water_chain, chain.type)
        for res in chain.residues:
          new_res = edi.AppendResidue(water_chain, res.name)
          new_res.SetStringProp('type', mol.StringFromChainType(chain.type))
          new_res.SetStringProp('description', chain.description)
          _CopyAtoms(res, new_res, edi, tr)
      else:
        if not ligand_chain.IsValid():
          # all ligands, put in one chain, are named '_'
          ligand_chain = edi.InsertChain('_')
          last_rnum = 0
        else:
          last_rnum = ligand_chain.residues[-1].number.num
        residues=chain.residues
        ins_code='\0'
        if len(residues)>1:
          ins_code='A'
        for res in chain.residues:
          new_res = edi.AppendResidue(ligand_chain, res.name, 
                                      mol.ResNum(last_rnum+1, ins_code))
          new_res.SetStringProp('description', chain.description)
          new_res.SetStringProp('type', mol.StringFromChainType(chain.type))
          ins_code = chr(ord(ins_code)+1)
          _CopyAtoms(res, new_res, edi, tr)
  conop.ConnectAll(pdb_bu)
  return pdb_bu

MMCifInfoBioUnit.PDBize = _PDBize

## \example fft_li.py
#
# This scripts loads one or more images and shows their Fourier Transforms on 
# the screen. A viewer is opened for each loaded image. The Fourier Transform 
# honors the origin of the reference system, which is assumed to be at the 
# center of the image.
#
# Usage:
#
# \code giplt view_ft.py <image1> [<image2> <image3> .... ] \endcode
#
# <BR>
# <BR>
