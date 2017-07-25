import unittest, os, sys
import ost
from ost import io, mol, settings
# check if we can import: fails if numpy or scipy not available
try:
  from ost.mol.alg.qsscoring import *
except ImportError:
  print "Failed to import qsscoring. Happens when numpy or scipy missing. " \
        "Ignoring test_qsscoring.py tests."
  sys.exit(0)


def _LoadFile(file_name):
  """Helper to avoid repeating input path over and over."""
  return io.LoadPDB(os.path.join('testfiles', file_name))


class TestQSscore(unittest.TestCase):

  # TESTS base

  def test_QSscoreEntity(self):
    # use smallest test structure...
    ent = _LoadFile('3ia3.1.pdb')
    ent.SetName("my_ent")
    # create clean one
    qs_ent = QSscoreEntity(ent)
    self.assertTrue(qs_ent.is_valid)
    # check naming
    qs_ent.SetName("my_qs_ent")
    self.assertEqual(qs_ent.original_name, "my_ent")
    self.assertEqual(qs_ent.GetName(), "my_qs_ent")
    # check cleanup
    self.assertEqual(sorted(ch.name for ch in qs_ent.ent.chains), ['A', 'B'])
    self.assertEqual(qs_ent.removed_chains, ['_'])
    self.assertFalse(qs_ent.calpha_only)
    # check CA entity
    ca_ent = qs_ent.ca_entity
    self.assertEqual(sorted(ch.name for ch in ca_ent.chains), ['A', 'B'])
    self.assertEqual(ca_ent.residue_count, ca_ent.atom_count)
    self.assertEqual(ca_ent.Select('aname=CA').atom_count, ca_ent.atom_count)
    self.assertEqual(sorted(qs_ent.ca_chains.keys()), ['A', 'B'])
    for ch in ca_ent.chains:
      self.assertEqual(''.join([r.one_letter_code for r in ch.residues]),
                       str(qs_ent.ca_chains[ch.name]))
    # check chem groups
    self.assertEqual(sorted(qs_ent.chem_groups), [['A'], ['B']])
    # check contacts
    self.assertEqual(len(qs_ent.contacts['A']['B']), 45)
    self.assertAlmostEqual(qs_ent.contacts['A']['B'][23][127], 10.069, 2)
    self.assertEqual(len(qs_ent.contacts_ca['A']['B']), 42)
    self.assertAlmostEqual(qs_ent.contacts_ca['A']['B'][23][127], 10.471, 2)
    # check contact filtering
    old_contacts = qs_ent.contacts.copy()
    old_contacts_ca = qs_ent.contacts_ca.copy()
    qs_ent.contacts = old_contacts
    self.assertEqual(qs_ent.contacts, old_contacts)
    qs_ent.contacts_ca = old_contacts_ca
    self.assertEqual(qs_ent.contacts_ca, old_contacts_ca)
    dummy_contacts = {'A': {'B': {1: {2: 3.0, 4: 5.0}},
                            'C': {10: {20: 30.0, 40: 50.0}}},
                      'B': {'C': {100: {200: 300.0, 400: 500.0}}}}
    qs_ent.contacts = dummy_contacts
    self.assertEqual(qs_ent.contacts, {'A': {'B': {1: {2: 3.0, 4: 5.0}}}})
    self.assertEqual(qs_ent.contacts_ca, old_contacts_ca)
    qs_ent.contacts = old_contacts
    qs_ent.contacts_ca = dummy_contacts
    self.assertEqual(qs_ent.contacts, old_contacts)
    self.assertEqual(qs_ent.contacts_ca, {'A': {'B': {1: {2: 3.0, 4: 5.0}}}})

    # check chain removal for non-amino acid chains
    ent_extra = ent.Copy()
    edi = ent_extra.EditXCS()
    # classic ligand chain
    ch = edi.InsertChain('C')
    for _ in range(30):
      r = edi.AppendResidue(ch, 'HOH')
      edi.InsertAtom(r, 'O', ost.geom.Vec3())
    # DNA chain
    ch = edi.InsertChain('D')
    for _ in range(30):
      r = edi.AppendResidue(ch, 'A')
      edi.InsertAtom(r, 'P', ost.geom.Vec3())
    edi.UpdateICS()
    # ensure both removed
    qs_ent_test = QSscoreEntity(ent_extra)
    self.assertEqual(sorted(qs_ent_test.removed_chains), ['C', 'D', '_'])

    # invalid structures: monomers (or less) before or after cleaning
    ost.PushVerbosityLevel(-1)
    # empty view
    ent_empty = ent.CreateEmptyView()
    qs_ent_invalid = QSscoreEntity(ent_empty)
    self.assertFalse(qs_ent_invalid.is_valid)
    # monomer
    ent_mono = ent.Select('cname=A')
    qs_ent_invalid = QSscoreEntity(ent_mono)
    self.assertFalse(qs_ent_invalid.is_valid)
    # short chain removed
    ent_short = ent.Select('cname=A or rnum<20')
    qs_ent_invalid = QSscoreEntity(ent_short)
    self.assertFalse(qs_ent_invalid.is_valid)
    self.assertEqual(sorted(qs_ent_invalid.removed_chains), ['B', '_'])
    # non-AA chain removal
    ent_non_AA = ent_extra.Select('cname=A,C,D')
    qs_ent_invalid = QSscoreEntity(ent_non_AA)
    self.assertFalse(qs_ent_invalid.is_valid)
    self.assertEqual(sorted(qs_ent_invalid.removed_chains), ['C', 'D'])
    ost.PopVerbosityLevel()

    # exception when scoring with invalid QSscoreEntity
    with self.assertRaises(QSscoreError):
      qs_scorer_tst = QSscorer(qs_ent_invalid, qs_ent)
      qs_scorer_tst.global_score
    with self.assertRaises(QSscoreError):
      qs_scorer_tst = QSscorer(qs_ent, qs_ent_invalid)
      qs_scorer_tst.global_score


  # TESTS HETERO

  def test_HeteroCase1(self):
    # additional chains
    ent_1 = _LoadFile('4ux8.1.pdb') # A2 B2 C2, symmetry: C2
    ent_2 = _LoadFile('3fub.2.pdb') # A2 B2   , symmetry: C2
    qs_scorer = QSscorer(ent_1, ent_2)
    # check properties
    self.assertFalse(qs_scorer.calpha_only)
    # check mappings
    self.assertEqual(qs_scorer.chem_mapping,
                     {('D', 'F'): ('B', 'D'), ('C', 'E'): ('A', 'C')})
    self.assertEqual(sorted(qs_scorer.symm_1), [('D', 'C'), ('F', 'E')])
    self.assertEqual(sorted(qs_scorer.symm_2), [('A', 'B'), ('C', 'D')])
    self.assertEqual(qs_scorer.chain_mapping,
                     {'C': 'A', 'E': 'C', 'D': 'B', 'F': 'D'})
    # check scoring
    self.assertAlmostEqual(qs_scorer.global_score, 0.825, 2)
    # without penalties the interface is the same
    self.assertAlmostEqual(qs_scorer.best_score, 1.0, 2)
    self._CheckScorer(qs_scorer)

  def test_HeteroCase1b(self):
    # as above but with assymetric unit of 3fub
    # -> no overlap found: check if extensive search can deal with it
    ent_1 = _LoadFile('4ux8.1.pdb')
    ent_2 = _LoadFile('3fub.au.pdb')
    qs_scorer = QSscorer(ent_1, ent_2)
    # check properties
    self.assertFalse(qs_scorer.calpha_only)
    # check mappings
    self.assertEqual(qs_scorer.chem_mapping,
                     {('C', 'E'): ('A', 'C'), ('D', 'F'): ('D', 'B')})
    self.assertEqual(sorted(qs_scorer.symm_1), [('D', 'C'), ('F', 'E')])
    self.assertEqual(sorted(qs_scorer.symm_2), [('B', 'A'), ('D', 'C')])
    self.assertEqual(qs_scorer.chain_mapping,
                     {'C': 'A', 'E': 'C', 'D': 'B', 'F': 'D'})
    # check scoring
    self.assertAlmostEqual(qs_scorer.global_score, 0.356, 2)
    self.assertAlmostEqual(qs_scorer.best_score, 0.419, 2)
    self._CheckScorer(qs_scorer)

    # enforce different chain mapping
    enforced_cm = {'C': 'C', 'E': 'A', 'D': 'D', 'F': 'B'}
    qs_scorer_2 = QSscorer(qs_scorer.qs_ent_1, qs_scorer.qs_ent_2)
    qs_scorer_2.chain_mapping = enforced_cm
    self.assertAlmostEqual(qs_scorer_2.global_score, 0.356, 2)
    self.assertAlmostEqual(qs_scorer_2.best_score, 0.419, 2)
    self.assertEqual(qs_scorer_2.chain_mapping, enforced_cm)
    self._CheckScorer(qs_scorer_2)

    # check if we get same mapping with restricted max_ca_per_chain_for_cm
    qs_scorer_2 = QSscorer(qs_scorer.qs_ent_1, qs_scorer.qs_ent_2)
    qs_scorer_2.max_ca_per_chain_for_cm = 50
    self.assertEqual(qs_scorer_2.chain_mapping, qs_scorer.chain_mapping)
    self.assertAlmostEqual(qs_scorer_2.global_score, qs_scorer.global_score, 2)
    self.assertAlmostEqual(qs_scorer_2.best_score, qs_scorer.best_score, 2)
    self._CheckScorer(qs_scorer_2)


  def test_HeteroCase2(self):
    # different stoichiometry
    ent_1 = _LoadFile('1efu.1.pdb') # A2 B2, symmetry: C2
    ent_2 = _LoadFile('4pc6.1.pdb') # A B  , no symmetry
    qs_scorer = QSscorer(ent_1, ent_2)
    self.assertAlmostEqual(qs_scorer.global_score, 0.3131, 2)
    self.assertAlmostEqual(qs_scorer.best_score, 0.941, 2)
    self._CheckScorer(qs_scorer)
    # check properties
    self.assertFalse(qs_scorer.calpha_only)
    self.assertEqual(qs_scorer.chem_mapping,
                     {('A', 'C'): ('A',), ('B', 'D'): ('B',)})
    self.assertEqual(sorted(qs_scorer.symm_1), [('A', 'B'), ('C', 'D')])
    self.assertEqual(sorted(qs_scorer.symm_2), [('A', 'B')])
    self.assertEqual(qs_scorer.chain_mapping, {'A': 'A', 'B': 'B'})

  def test_HeteroCase3(self):
    # more chains
    ent_1 = _LoadFile('2vjt.1.pdb') # A6 B6, symmetry: D3
    ent_2 = _LoadFile('3dbj.1.pdb') # A3 B3, symmetry: C3
    qs_scorer = QSscorer(ent_1, ent_2)
    self.assertAlmostEqual(qs_scorer.global_score, 0.359, 2)
    self.assertAlmostEqual(qs_scorer.best_score, 0.958, 2)
    self._CheckScorer(qs_scorer)
    # check properties
    self.assertFalse(qs_scorer.calpha_only)
    self.assertEqual(qs_scorer.chem_mapping,
                     {('A', 'C', 'E', 'G', 'I', 'K'): ('A', 'C', 'E'),
                      ('B', 'D', 'F', 'H', 'J', 'L'): ('B', 'D', 'F')})
    self.assertEqual(sorted(qs_scorer.symm_1),
                     [('A', 'B'), ('C', 'D'), ('E', 'F'),
                      ('G', 'H'), ('I', 'J'), ('K', 'L')])
    self.assertEqual(sorted(qs_scorer.symm_2),
                     [('A', 'B'), ('C', 'D'), ('E', 'F')])
    self.assertEqual(qs_scorer.chain_mapping,
                     {'A':'A', 'B':'B', 'G':'C', 'H':'D', 'I':'E', 'J':'F'})

    # user's symmetry groups
    symm_1 = [('E', 'D', 'C', 'B', 'A', 'F'), ('G', 'J', 'I', 'L', 'K', 'H')]
    symm_2 = [('A', 'B', 'C', 'D', 'E', 'F')]
    # use QSscoreEntity to go faster
    qs_scorer_symm = QSscorer(qs_scorer.qs_ent_1, qs_scorer.qs_ent_2)
    qs_scorer_symm.SetSymmetries(symm_1, symm_2)
    self.assertAlmostEqual(qs_scorer_symm.global_score,
                           qs_scorer.global_score, 2)
    self.assertAlmostEqual(qs_scorer_symm.best_score, qs_scorer.best_score, 2)
    self._CheckScorer(qs_scorer_symm)
    # check properties
    self.assertFalse(qs_scorer_symm.calpha_only)
    self.assertEqual(qs_scorer_symm.chem_mapping, qs_scorer.chem_mapping)
    self.assertEqual(qs_scorer_symm.symm_1, symm_1)
    self.assertEqual(qs_scorer_symm.symm_2, symm_2)
    self.assertEqual(qs_scorer_symm.chain_mapping,
                     {'C':'A', 'D':'B', 'E':'E', 'F':'F', 'K':'C', 'L':'D'})
    self._CheckScorer(qs_scorer_symm)

  def test_HeteroCase4(self):
    # inverted chains
    ent_1 = _LoadFile('3ia3.1.pdb') # AB, no symmetry
    ent_2 = _LoadFile('3ia3.2.pdb') # BA, no symmetry
    qs_scorer = QSscorer(ent_1, ent_2)
    self.assertAlmostEqual(qs_scorer.global_score, 0.980, 2)
    self.assertAlmostEqual(qs_scorer.best_score, 0.980, 2)
    self._CheckScorer(qs_scorer)
    # check properties
    self.assertFalse(qs_scorer.calpha_only)
    # check mappings
    self.assertEqual(qs_scorer.chem_mapping, {('A',): ('B',), ('B',): ('A',)})
    self.assertEqual(qs_scorer.chain_mapping, {'A': 'B', 'B': 'A'})

    # check superposition
    sup = qs_scorer.superposition
    self.assertAlmostEqual(sup.rmsd, 0.3372, 2)
    self.assertEqual(sup.view1.atom_count, 225)
    self.assertEqual(sup.view2.atom_count, 225)
    rmsd = mol.alg.CalculateRMSD(sup.view1, sup.view2, sup.transformation)
    self.assertAlmostEqual(sup.rmsd, rmsd, 2)

    # check if CA-only scoring is close to this
    ent_2_ca = ent_2.Select('aname=CA')
    # use QSscoreEntity to go faster
    qs_scorer_ca = QSscorer(qs_scorer.qs_ent_1, ent_2_ca)
    self.assertTrue(qs_scorer_ca.calpha_only)
    self.assertAlmostEqual(qs_scorer_ca.global_score, qs_scorer.global_score, 2)
    self.assertAlmostEqual(qs_scorer_ca.best_score, qs_scorer.best_score, 2)
    self._CheckScorer(qs_scorer_ca)
    # throw exception for messed up chains without CA atoms
    ent_2_no_ca = ent_2.Select('aname!=CA')
    with self.assertRaises(QSscoreError):
      qs_scorer_tst = QSscorer(qs_scorer.qs_ent_1, ent_2_no_ca)
      qs_scorer_tst.global_score
    ent_2_almost_no_ca = ent_2.Select('aname!=CA or cname=A')
    with self.assertRaises(QSscoreError):
      qs_scorer_tst = QSscorer(qs_scorer.qs_ent_1, ent_2_almost_no_ca)
      qs_scorer_tst.global_score


  def test_HeteroModel(self):
    # uncomplete model missing 2 third of the contacts
    ent_1 = _LoadFile('1eud_ref.pdb')               # AB, no symmetry
    ent_2 = _LoadFile('1eud_mdl_partial-dimer.pdb') # BA, no symmetry
    qs_scorer = QSscorer(ent_1, ent_2)
    self.assertAlmostEqual(qs_scorer.global_score, 0.323, 2)
    self.assertAlmostEqual(qs_scorer.best_score, 0.921, 2)
    self._CheckScorer(qs_scorer)
    # check properties
    self.assertFalse(qs_scorer.calpha_only)
    self.assertEqual(qs_scorer.chem_mapping, {('A',): ('A',), ('B',): ('B',)})
    self.assertEqual(qs_scorer.chain_mapping, {'A': 'A', 'B': 'B'})


  # TESTS HOMO

  def test_HomoCase1(self):
    # different stoichiometry SOD
    ent_1 = _LoadFile('4dvh.1.pdb') # A2, symmetry: C2
    ent_2 = _LoadFile('4br6.1.pdb') # A4, symmetry: D2
    qs_scorer = QSscorer(ent_1, ent_2)
    self.assertAlmostEqual(qs_scorer.global_score, 0.147, 2)
    self.assertAlmostEqual(qs_scorer.best_score, 0.866, 2)
    self._CheckScorer(qs_scorer)
    # check properties
    self.assertFalse(qs_scorer.calpha_only)
    self.assertEqual(qs_scorer.chem_mapping, {('A', 'B'): ('B', 'C', 'D', 'A')})
    self.assertEqual(sorted(qs_scorer.symm_1), [('A', 'B')])
    self.assertEqual(sorted(qs_scorer.symm_2), [('B', 'A'), ('C', 'D')])
    self.assertEqual(qs_scorer.chain_mapping, {'A': 'B', 'B': 'A'})

  def test_HomoCase2(self):
    # broken cyclic symmetry
    ent_1 = _LoadFile('4r7y.1.pdb')   # A6, symmetry: C6
    ent_2 = ent_1.Select('cname=A,B') # A2, no symmetry
    qs_scorer = QSscorer(ent_1, ent_2)
    # only one interface over 6
    self.assertAlmostEqual(qs_scorer.global_score, 1/6., 2)
    # without penalties the interface is the same
    self.assertAlmostEqual(qs_scorer.best_score, 1.0, 2)
    # check properties
    self.assertFalse(qs_scorer.calpha_only)
    self.assertEqual(qs_scorer.chem_mapping,
                     {('A', 'B', 'C', 'D', 'E', 'F'): ('A', 'B')})
    self.assertEqual(sorted(qs_scorer.symm_1), [('A', 'B', 'C', 'D', 'E', 'F')])
    self.assertEqual(sorted(qs_scorer.symm_2), [('B', 'A')])
    self.assertEqual(qs_scorer.chain_mapping, {'A': 'A', 'B': 'B'})
    self._CheckScorer(qs_scorer)

    # using user symmetry groups
    symm_1 = [('A', 'B'), ('C', 'D'), ('E', 'F')]
    symm_2 = [('A', 'B')]
    # use QSscoreEntity to go faster
    qs_scorer_symm = QSscorer(qs_scorer.qs_ent_1, qs_scorer.qs_ent_2)
    qs_scorer_symm.SetSymmetries(symm_1, symm_2)
    self.assertEqual(qs_scorer_symm.global_score, qs_scorer.global_score)
    self.assertEqual(qs_scorer_symm.best_score, qs_scorer.best_score)
    # check properties
    self.assertFalse(qs_scorer_symm.calpha_only)
    self.assertEqual(qs_scorer_symm.chem_mapping, qs_scorer.chem_mapping)
    self.assertEqual(qs_scorer_symm.symm_1, symm_1)
    self.assertEqual(qs_scorer_symm.symm_2, symm_2)
    self.assertEqual(qs_scorer_symm.chain_mapping, qs_scorer.chain_mapping)
    self._CheckScorer(qs_scorer_symm)

    # try to put invalid symmetries
    qs_scorer_symm = QSscorer(qs_scorer.qs_ent_1, qs_scorer.qs_ent_2)
    triv_1 = [tuple(ch.name for ch in qs_scorer_symm.ent_to_cm_1.chains)]
    triv_2 = [tuple(ch.name for ch in qs_scorer_symm.ent_to_cm_2.chains)]
    qs_scorer_symm.SetSymmetries([], [])
    self.assertEqual(qs_scorer_symm.symm_1, triv_1)
    self.assertEqual(qs_scorer_symm.symm_2, triv_2)
    qs_scorer_symm.SetSymmetries([('A', 'B', 'C'), ('D', 'E', 'F')],
                                 [('A', 'B')])
    self.assertEqual(qs_scorer_symm.symm_1, triv_1)
    self.assertEqual(qs_scorer_symm.symm_2, triv_2)
    qs_scorer_symm.SetSymmetries([('A', 'B'), ('C', 'D'), ('E', 'F')],
                                 [('A', 'C')])
    self.assertEqual(qs_scorer_symm.symm_1, triv_1)
    self.assertEqual(qs_scorer_symm.symm_2, triv_2)
    qs_scorer_symm.SetSymmetries([('A', 'B'), ('C', 'D')],
                                 [('A', 'B')])
    self.assertEqual(qs_scorer_symm.symm_1, triv_1)
    self.assertEqual(qs_scorer_symm.symm_2, triv_2)
    qs_scorer_symm.SetSymmetries([('A', 'B'), ('C', 'D'), ('A', 'B')],
                                 [('A', 'B')])
    self.assertEqual(qs_scorer_symm.symm_1, triv_1)
    self.assertEqual(qs_scorer_symm.symm_2, triv_2)
    qs_scorer_symm.SetSymmetries([('A', 'B'), ('C', 'D', 'E', 'F')],
                                 [('A', 'B')])
    self.assertEqual(qs_scorer_symm.symm_1, triv_1)
    self.assertEqual(qs_scorer_symm.symm_2, triv_2)
    # check cleaning of extra chains
    qs_scorer_symm.SetSymmetries([('A','B'), ('C','D'), ('E','F'), ('G','H')],
                                 [('A', 'B', 'C')])
    self.assertEqual(qs_scorer_symm.symm_1, [('A','B'), ('C','D'), ('E','F')])
    self.assertEqual(qs_scorer_symm.symm_2, [('A', 'B')])


  # TEST EXTRA SCORES
  
  def test_lDDT(self):
    # lDDT is not symmetrical and does not account for overprediction!
    ref = _LoadFile('4br6.1.pdb').Select('cname=A,B')
    mdl = _LoadFile('4br6.1.pdb')
    qs_scorer = QSscorer(ref, mdl)
    self.assertAlmostEqual(qs_scorer.global_score, 0.171, 2)
    self.assertAlmostEqual(qs_scorer.best_score, 1.00, 2)
    self.assertAlmostEqual(qs_scorer.lddt_score, 1.00, 2)
    self._CheckScorerLDDT(qs_scorer)
    # flip them (use QSscoreEntity to go faster)
    qs_scorer2 = QSscorer(qs_scorer.qs_ent_2, qs_scorer.qs_ent_1)
    self.assertAlmostEqual(qs_scorer2.global_score, 0.171, 2)
    self.assertAlmostEqual(qs_scorer2.best_score, 1.00, 2)
    self.assertAlmostEqual(qs_scorer2.lddt_score, 0.483, 2)
    self._CheckScorerLDDT(qs_scorer)
    # check properties
    self.assertFalse(qs_scorer.calpha_only)
    self.assertEqual(qs_scorer.chem_mapping, {('B', 'A'): ('B', 'C', 'D', 'A')})
    self.assertEqual(sorted(qs_scorer.symm_1), [('B', 'A')])
    self.assertEqual(sorted(qs_scorer.symm_2), [('B', 'A'), ('C', 'D')])
    self.assertEqual(qs_scorer.chain_mapping, {'A': 'A', 'B': 'B'})


  # TEST BIG STUFF and FANCY SYMMETRIES

  def test_HeteroBig(self):
    # comparing rubisco with half of it
    ent_1 = _LoadFile('4f0h.1.pdb')      # A8 B8, symmetry: D4
    ent_2 = _LoadFile('4f0h.1_half.pdb') # A4 B4, symmetry: C4
    qs_scorer = QSscorer(ent_1, ent_2)
    self.assertAlmostEqual(qs_scorer.global_score, 1/4., 2)
    self.assertAlmostEqual(qs_scorer.best_score, 1.0, 2)
    self._CheckScorer(qs_scorer)
    # check properties
    self.assertFalse(qs_scorer.calpha_only)
    self.assertEqual(qs_scorer.chem_mapping,
            {('A', 'C', 'E', 'G', 'I', 'K', 'M', 'O'): ('A', 'C', 'E', 'G'),
             ('B', 'D', 'F', 'H', 'J', 'L', 'N', 'P'): ('B', 'D', 'F', 'H')})
    self.assertEqual(sorted(qs_scorer.symm_1),
                     [('A', 'B'), ('C', 'D'), ('E', 'F'), ('G', 'H'),
                      ('I', 'J'), ('K', 'L'), ('M', 'N'), ('O', 'P')])
    self.assertEqual(sorted(qs_scorer.symm_2),
                     [('A', 'B'), ('C', 'D'), ('E', 'F'), ('G', 'H')])
    self.assertEqual(qs_scorer.chain_mapping,
                     {'A': 'A', 'C': 'C', 'B': 'B', 'E': 'E',
                      'D': 'D', 'G': 'G', 'F': 'F', 'H': 'H'})

  def test_Capsid(self):
    ent_1 = _LoadFile('4gh4.2.pdb') # A5 B5 C5 D5, symmetry: C5
    ent_2 = _LoadFile('1qqp.2.pdb') # A5 B5 C5 D5, symmetry: C5
    qs_scorer = QSscorer(ent_1, ent_2)
    self.assertAlmostEqual(qs_scorer.global_score, 0.921, 2)
    self.assertAlmostEqual(qs_scorer.best_score, 0.941, 2)
    self._CheckScorer(qs_scorer)
    # check properties
    self.assertFalse(qs_scorer.calpha_only)
    self.assertEqual(qs_scorer.chem_mapping,
            {('D', 'H', 'L', 'P', 'T'): ('D', 'H', 'L', 'P', 'T'),
             ('A', 'E', 'I', 'M', 'Q'): ('A', 'E', 'I', 'M', 'Q'),
             ('C', 'G', 'K', 'O', 'S'): ('C', 'G', 'K', 'O', 'S'),
             ('B', 'F', 'J', 'N', 'R'): ('B', 'F', 'J', 'N', 'R')})
    self.assertEqual(sorted(qs_scorer.symm_1),
                     [('D', 'A', 'S', 'B'), ('H', 'E', 'C', 'F'),
                      ('L', 'I', 'G', 'J'), ('P', 'M', 'K', 'N'),
                      ('T', 'Q', 'O', 'R')])
    self.assertEqual(sorted(qs_scorer.symm_2),
                     [('D', 'A', 'S', 'B'), ('H', 'E', 'C', 'F'),
                      ('L', 'I', 'G', 'J'), ('P', 'M', 'K', 'N'),
                      ('T', 'Q', 'O', 'R')])
    self.assertEqual(qs_scorer.chain_mapping,
                     {'A': 'A', 'C': 'C', 'B': 'B', 'E': 'E', 'D': 'D',
                      'G': 'G', 'F': 'F', 'I': 'I', 'H': 'H', 'K': 'K',
                      'J': 'J', 'M': 'M', 'L': 'L', 'O': 'O', 'N': 'N',
                      'Q': 'Q', 'P': 'P', 'S': 'S', 'R': 'R', 'T': 'T'})

  def test_TetrahedralSymmetry(self):
    ent_1 = _LoadFile('1mog.1.pdb') # A12, symmetry: T
    ent_2 = _LoadFile('2cc6.1.pdb') # A12, symmetry: T
    qs_scorer = QSscorer(ent_1, ent_2)
    self.assertAlmostEqual(qs_scorer.global_score, 0.954, 2)
    self.assertAlmostEqual(qs_scorer.best_score, 0.994, 2)
    self._CheckScorer(qs_scorer)
    # check properties
    self.assertFalse(qs_scorer.calpha_only)
    self.assertEqual(qs_scorer.chem_mapping,
            {('A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L'): \
               ('A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L')})
    self.assertEqual(sorted(qs_scorer.symm_1),
                     [('A', 'D'), ('B', 'E'), ('C', 'F'),
                      ('G', 'J'), ('H', 'K'), ('I', 'L')])
    self.assertEqual(sorted(qs_scorer.symm_2),
                     [('A', 'G'), ('B', 'I'), ('C', 'E'),
                      ('D', 'K'), ('F', 'J'), ('H', 'L')])
    # chain mapping is ambiguous here so we cannot check it

  def test_Urease(self):
    ent_1 = _LoadFile('1e9y.1.pdb') # A12 B12, symmetry: T
    ent_2 = _LoadFile('1e9z.1.pdb') # A12 B12, symmetry: T
    qs_scorer = QSscorer(ent_1, ent_2)
    self.assertAlmostEqual(qs_scorer.global_score, 0.958, 2)
    self.assertAlmostEqual(qs_scorer.best_score, 0.958, 2)
    self._CheckScorer(qs_scorer)
    # check properties
    self.assertFalse(qs_scorer.calpha_only)
    self.assertEqual(qs_scorer.chem_mapping,
            {('B', 'D', 'F', 'H', 'J', 'L', 'N', 'P', 'R', 'T', 'V', 'X'): \
               ('B', 'D', 'F', 'H', 'J', 'L', 'N', 'P', 'R', 'T', 'V', 'X'),
             ('A', 'C', 'E', 'G', 'I', 'K', 'M', 'O', 'Q', 'S', 'U', 'W'): \
               ('A', 'C', 'E', 'G', 'I', 'K', 'M', 'O', 'Q', 'S', 'U', 'W')})
    self.assertEqual(sorted(qs_scorer.symm_1),
                     [('B', 'Q'), ('D', 'G'), ('F', 'W'), ('H', 'U'),
                      ('J', 'K'), ('L', 'S'), ('N', 'A'), ('P', 'E'),
                      ('R', 'M'), ('T', 'I'), ('V', 'C'), ('X', 'O')])
    self.assertEqual(sorted(qs_scorer.symm_2),
                     [('B', 'Q'), ('D', 'W'), ('F', 'U'), ('H', 'C'),
                      ('J', 'K'), ('L', 'S'), ('N', 'A'), ('P', 'E'),
                      ('R', 'M'), ('T', 'I'), ('V', 'O'), ('X', 'G')])
    self.assertEqual(qs_scorer.chain_mapping,
                     {'A': 'A', 'C': 'W', 'B': 'B', 'E': 'E', 'D': 'X',
                      'G': 'G', 'F': 'F', 'I': 'I', 'H': 'H', 'K': 'K',
                      'J': 'J', 'M': 'M', 'L': 'L', 'O': 'O', 'N': 'N',
                      'Q': 'Q', 'P': 'P', 'S': 'S', 'R': 'R', 'U': 'C',
                      'T': 'T', 'W': 'U', 'V': 'D', 'X': 'V'})

  def test_C6SymmetryHetero(self):
    ent_1 = _LoadFile('3j3r.1.pdb') # A6 B6, symmetry: C6
    ent_2 = _LoadFile('3j3s.1.pdb') # A6 B6, symmetry: C6
    qs_scorer = QSscorer(ent_1, ent_2)
    self.assertAlmostEqual(qs_scorer.global_score, 0.559, 2)
    self.assertAlmostEqual(qs_scorer.best_score, 0.559, 2)
    self._CheckScorer(qs_scorer)
    # check properties
    self.assertFalse(qs_scorer.calpha_only)
    self.assertEqual(qs_scorer.chem_mapping,
            {('A', 'B', 'C', 'D', 'E', 'F'): ('A', 'B', 'C', 'D', 'E', 'F'),
             ('G', 'H', 'I', 'J', 'K', 'L'): ('G', 'H', 'I', 'J', 'K', 'L')})
    self.assertEqual(sorted(qs_scorer.symm_1),
                     [('A', 'G'), ('B', 'H'), ('C', 'I'),
                      ('D', 'J'), ('E', 'K'), ('F', 'L')])
    self.assertEqual(sorted(qs_scorer.symm_2),
                     [('A', 'G'), ('B', 'H'), ('C', 'I'),
                      ('D', 'J'), ('E', 'K'), ('F', 'L')])
    self.assertEqual(qs_scorer.chain_mapping,
                     {'A': 'A', 'C': 'C', 'B': 'B', 'E': 'E',
                      'D': 'D', 'G': 'G', 'F': 'F', 'I': 'I',
                      'H': 'H', 'K': 'K', 'J': 'J', 'L': 'L'})

  def test_OctahedralSymmetry(self):
    ent_1 = _LoadFile('3vcd.1.pdb') # A24, symmetry: O
    ent_2 = _LoadFile('4ddf.1.pdb') # A24, symmetry: O
    qs_scorer = QSscorer(ent_1, ent_2)
    self.assertAlmostEqual(qs_scorer.global_score, 0.975, 2)
    self.assertAlmostEqual(qs_scorer.best_score, 0.975, 2)
    self._CheckScorer(qs_scorer)
    # check properties
    self.assertFalse(qs_scorer.calpha_only)
    self.assertEqual(qs_scorer.chem_mapping,
            {('A', 'E', 'I', 'M', 'Q', 'U', 'B', 'C', 'D', 'F', 'G', 'H',
              'J', 'K', 'L', 'N', 'O', 'P', 'R', 'S', 'T', 'V', 'W', 'X'): \
               ('A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L',
                'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X')})
    self.assertEqual(sorted(qs_scorer.symm_1),
                     [('A', 'X'), ('B', 'H'), ('C', 'P'), ('D', 'K'),
                      ('E', 'I'), ('F', 'J'), ('G', 'T'), ('L', 'R'),
                      ('M', 'S'), ('O', 'V'), ('Q', 'N'), ('U', 'W')])
    self.assertEqual(sorted(qs_scorer.symm_2),
                     [('A', 'O'), ('B', 'M'), ('C', 'N'), ('D', 'G'),
                      ('E', 'H'), ('F', 'I'), ('J', 'W'), ('K', 'X'),
                      ('L', 'V'), ('P', 'T'), ('Q', 'U'), ('R', 'S')])
    self.assertEqual(qs_scorer.chain_mapping,
                     {'A': 'J', 'C': 'L', 'B': 'K', 'E': 'P', 'D': 'R',
                      'G': 'O', 'F': 'Q', 'I': 'T', 'H': 'X', 'K': 'S',
                      'J': 'U', 'M': 'G', 'L': 'I', 'O': 'N', 'N': 'H',
                      'Q': 'E', 'P': 'V', 'S': 'D', 'R': 'F', 'U': 'B',
                      'T': 'A', 'W': 'M', 'V': 'C', 'X': 'W'})


  ###########################################################################
  # HELPERS
  ###########################################################################
  
  def _CheckEntity(self, qs_ent, check_ca_contacts):
    # check internal consistency of QS entity
    self.assertTrue(qs_ent.is_valid)
    # CA chains
    ca_ent = qs_ent.ca_entity
    self.assertEqual(ca_ent.residue_count, ca_ent.atom_count)
    ca_names = sorted([ch.name for ch in ca_ent.chains])
    self.assertEqual(sorted(qs_ent.ca_chains.keys()), ca_names)
    # chem groups
    self.assertEqual(sorted([c for cg in qs_ent.chem_groups for c in cg]),
                     ca_names)
    # check contacts (only chain names)
    if check_ca_contacts:
      contacts = qs_ent.contacts_ca
    else:
      contacts = qs_ent.contacts
    for c1 in contacts:
      self.assertTrue(c1 in ca_names)
      for c2 in contacts[c1]:
        self.assertTrue(c2 in ca_names)
        self.assertLess(c1, c2)

  def _CheckScorer(self, qs_scorer):
    # check if we live up to our promises (assume: we did global score)
    qs_ent_1 = qs_scorer.qs_ent_1
    qs_ent_2 = qs_scorer.qs_ent_2
    # check QS entities
    self._CheckEntity(qs_ent_1, qs_scorer.calpha_only)
    self._CheckEntity(qs_ent_2, qs_scorer.calpha_only)
    self.assertNotEqual(qs_ent_1.GetName(), qs_ent_2.GetName())
    # check scorer attributes
    self.assertEqual(qs_scorer.calpha_only,
                     qs_ent_1.calpha_only or qs_ent_2.calpha_only)
    # check chem_mapping
    cn1 = set([c for cg in qs_ent_1.chem_groups for c in cg])
    cn2 = set([c for cg in qs_ent_2.chem_groups for c in cg])
    cm_names_1 = list()
    cm_names_2 = list()
    for cg1, cg2 in qs_scorer.chem_mapping.iteritems():
      ch_ref = qs_scorer.ent_to_cm_1.FindChain(cg1[0])
      self.assertEqual(ch_ref.residue_count, ch_ref.atom_count)
      self.assertGreaterEqual(ch_ref.residue_count, 5)
      self.assertLessEqual(ch_ref.residue_count,
                           qs_scorer.max_ca_per_chain_for_cm)
      for ch_name in cg1:
        self.assertTrue(ch_name in cn1)
        ch = qs_scorer.ent_to_cm_1.FindChain(ch_name)
        self.assertTrue(ch.IsValid())
        self.assertEqual(ch_ref.residue_count, ch.residue_count)
        self.assertEqual(ch_ref.atom_count, ch.atom_count)
        cm_names_1.append(ch_name)
      for ch_name in cg2:
        self.assertTrue(ch_name in cn2)
        ch = qs_scorer.ent_to_cm_2.FindChain(ch_name)
        self.assertTrue(ch.IsValid())
        self.assertEqual(ch_ref.residue_count, ch.residue_count)
        self.assertEqual(ch_ref.atom_count, ch.atom_count)
        cm_names_2.append(ch_name)
    # check that there's no extra stuff in ent_to_cm_1/2
    cm_names_1.sort()
    self.assertEqual(sorted(ch.name for ch in qs_scorer.ent_to_cm_1.chains),
                     cm_names_1)
    cm_names_2.sort()
    self.assertEqual(sorted(ch.name for ch in qs_scorer.ent_to_cm_2.chains),
                     cm_names_2)
    # check symm_1 / symm_2
    # (>= 1 symm. group, all groups same length, all chains appear)
    self.assertGreaterEqual(qs_scorer.symm_1, 1)
    ref_symm_1 = qs_scorer.symm_1[0]
    self.assertTrue(all(len(cg) == len(ref_symm_1) for cg in qs_scorer.symm_1))
    self.assertEqual(sorted(c for cg in qs_scorer.symm_1 for c in cg),
                     cm_names_1)
    self.assertGreaterEqual(qs_scorer.symm_2, 1)
    ref_symm_2 = qs_scorer.symm_2[0]
    self.assertTrue(all(len(cg) == len(ref_symm_2) for cg in qs_scorer.symm_2))
    self.assertEqual(sorted(c for cg in qs_scorer.symm_2 for c in cg),
                     cm_names_2)
    # check chain_mapping
    # (all chains of ent with less chains mapped, each only once, chem_map)
    chm_names_1 = qs_scorer.chain_mapping.keys()
    chm_names_2 = qs_scorer.chain_mapping.values()
    self.assertEqual(len(chm_names_1), min(len(cm_names_1), len(cm_names_2)))
    self.assertEqual(len(set(chm_names_1)), len(chm_names_1))
    self.assertEqual(len(set(chm_names_2)), len(chm_names_2))
    for cg1, cg2 in qs_scorer.chem_mapping.iteritems():
      for ch_name in cg1:
        if ch_name in qs_scorer.chain_mapping:
          self.assertTrue(qs_scorer.chain_mapping[ch_name] in cg2)
    # check alignments
    # (sorted and mapped and with views, all in mapped_residues)
    self.assertEqual(len(qs_scorer.alignments), len(chm_names_1))
    self.assertEqual(len(qs_scorer.alignments), len(qs_scorer.mapped_residues))
    for aln, ch_name in zip(qs_scorer.alignments, sorted(chm_names_1)):
      self.assertEqual(aln.sequences[0].name, ch_name)
      self.assertEqual(aln.sequences[1].name, qs_scorer.chain_mapping[ch_name])
      self.assertTrue(aln.sequences[0].HasAttachedView())
      self.assertTrue(aln.sequences[1].HasAttachedView())
      self.assertEqual(aln.sequences[0].attached_view.handle, qs_ent_1.ent)
      self.assertEqual(aln.sequences[1].attached_view.handle, qs_ent_2.ent)
      self.assertTrue(ch_name in qs_scorer.mapped_residues)
    # best_score / global_score in [0,1]
    self.assertGreaterEqual(qs_scorer.best_score, 0.0)
    self.assertLessEqual(qs_scorer.best_score, 1.0)
    self.assertGreaterEqual(qs_scorer.global_score, 0.0)
    self.assertLessEqual(qs_scorer.global_score, 1.0)


  def _CheckScorerLDDT(self, qs_scorer):
    # check if we live up to our promises (assume: we did global and lddt score)
    self._CheckScorer(qs_scorer)
    # check lddt_mdl and lddt_ref
    self.assertEqual(qs_scorer.lddt_mdl.chain_count, 1)
    self.assertEqual(qs_scorer.lddt_ref.chain_count, 1)
    # unique resnum?
    resnum_mdl = [r.number.num for r in qs_scorer.lddt_mdl.residues]
    resnum_mdl_set = set(resnum_mdl)
    self.assertEqual(len(resnum_mdl), len(resnum_mdl_set))
    resnum_ref = [r.number.num for r in qs_scorer.lddt_ref.residues]
    resnum_ref_set = set(resnum_ref)
    self.assertEqual(len(resnum_ref), len(resnum_ref_set))
    # independent shared residues count from mapped_residues
    num_shared = sum(len(v) for _,v in qs_scorer.mapped_residues.iteritems())
    shared_set = resnum_ref_set.intersection(resnum_mdl_set)
    self.assertEqual(len(shared_set), num_shared)
    # "lddt" prop on residues and B-factors?
    for r in qs_scorer.lddt_mdl.residues:
      if r.number.num in shared_set:
        self.assertTrue(r.HasProp('lddt'))
        r_lddt = r.GetFloatProp('lddt')
      else:
        r_lddt = 0
      self.assertTrue(all([a.b_factor == r_lddt for a in r.atoms]))


if __name__ == "__main__":
  try:
    settings.Locate(('clustalw', 'clustalw2'))
  except:
    print "Could not find ClustalW. Ignoring test_qsscoring.py tests."
    sys.exit(0)
  from ost import testutils
  if testutils.SetDefaultCompoundLib():
    testutils.RunTests()
  else:
    print 'No compound library available. Ignoring test_qsscoring.py tests.'