from ost.bindings import utils
from ost import settings, io, seq, LogError
import os
import subprocess

def ClustalW(seq1, seq2=None, clustalw=None, keep_files=False, nopgap=False, 
             clustalw_option_string=False):
  '''
  Runs a clustalw multiple sequence alignment. The results are returned as a
  :class:`~ost.seq.AlignmentHandle` instance.
  
  There are two ways to use this function:
  
   - align exactly two sequences:
   
      :param seq1: sequence_one
      :type seq1: :class:`~ost.seq.SequenceHandle` or :class:`str`
      
      :param seq2: sequence_two
      :type seq2: :class:`~ost.seq.SequenceHandle` or :class:`str`
  
      The two sequences can be specified as two separate function parameters 
      (`seq1`, `seq2`). The type of both parameters can be either
      :class:`~ost.seq.SequenceHandle` or :class:`str`, but must be the same for
      both parameters.
      
   - align two or more sequences:
   
      :param seq1: sequence_list
      :type seq1: :class:`~ost.seq.SequenceList`
      
      :param seq2: must be :class:`None`
      
      Two or more sequences can be specified by using a
      :class:`~ost.seq.SequenceList`. It is then passed as the first function 
      parameter (`seq1`). The second parameter (`seq2`) must be :class:`None`.
      
       
  :param clustalw: path to clustalw executable (used in :func:`~ost.settings.Locate`)
  :type clustalw: :class:`str`
  :param nopgap: turn residue-specific gaps off
  :type nopgap: :class:`bool`
  :param clustalw_option_string: additional clustalw flags (see http://toolkit.tuebingen.mpg.de/clustalw/help_params)
  :type clustalw_option_string: :class:`str`
  :param keep_files: do not delete temporary files
  :type keep_files: :class:`bool`
  '''
  clustalw_path=settings.Locate(('clustalw', 'clustalw2'), 
                                explicit_file_name=clustalw)
  
  if seq2!=None:
    if isinstance(seq1, seq.SequenceHandle) and isinstance(seq2, seq.SequenceHandle):
      seq_list=seq.CreateSequenceList()
      seq_list.AddSequence(seq1)
      seq_list.AddSequence(seq2)
    elif isinstance(seq1, str) and isinstance(seq2, str):
      seqh1=seq.CreateSequence("seq1", seq1)
      seqh2=seq.CreateSequence("seq2", seq2)
      seq_list=seq.CreateSequenceList()
      seq_list.AddSequence(seqh1)
      seq_list.AddSequence(seqh2)
    else:
      LogError("WARNING: Specify at least two Sequences")
      return
  elif isinstance(seq1, seq.SequenceList):
    seq_list=seq1
  else:
    LogError("WARNING: Specify either two SequenceHandles or one SequenceList")
    return
  
  temp_dir=utils.TempDirWithFiles((seq_list,))
  out=os.path.join(temp_dir.dirname, 'out.fasta')
  command='%s -infile="%s" -output=fasta -outfile="%s"' % (clustalw_path,
                                                           temp_dir.files[0],
                                                           out)
  if nopgap:
    command+=" -nopgap"
  if clustalw_option_string!=False:
    command=command+" "+clustalw_option_string  #see useful flags: http://toolkit.tuebingen.mpg.de/clustalw/help_params

  ps=subprocess.Popen(command, shell=True, stdout=subprocess.PIPE)
  ps.stdout.readlines()
  aln=io.LoadAlignment(out)
  

  for sequence in seq_list:
    for seq_num,aln_seq in enumerate(aln.sequences):
      if aln_seq.GetName()==sequence.GetName():
        break
    aln.SetSequenceOffset(seq_num,sequence.offset)
    if sequence.HasAttachedView():
      aln.AttachView(seq_num,sequence.GetAttachedView().Copy())

  if not keep_files:
    temp_dir.Cleanup()
  
  return aln
