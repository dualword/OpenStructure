set(OST_SEQ_ALG_UNIT_TESTS
  test_merge_pairwise_alignments.cc
  test_sequence_identity.cc
  tests.cc
  test_renumber.py
  test_local_align.py
  test_global_align.py
  test_weight_matrix.py
  test_aligntoseqres.py
)

ost_unittest(MODULE seq_alg SOURCES "${OST_SEQ_ALG_UNIT_TESTS}")

