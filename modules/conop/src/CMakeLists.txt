set(OST_CONOP_HEADERS
builder.hh
builder_fw.hh
conop.hh
heuristic_builder.hh
amino_acids.hh
compound.hh
compound_lib.hh
module_config.hh
nonstandard.hh
rule_based_builder.hh
ring_finder.hh
)

set(OST_CONOP_SOURCES
builder.cc
amino_acids.cc
conop.cc
heuristic_builder.cc
compound.cc
compound_lib.cc
nonstandard.cc
rule_based_builder.cc
ring_finder.cc
)

module(NAME conop SOURCES ${OST_CONOP_SOURCES}
       HEADERS ${OST_CONOP_HEADERS} DEPENDS_ON ost_mol ost_mol_alg ost_geom ost_db)

executable(NAME chemdict_tool SOURCES chemdict_tool.cc DEPENDS_ON ost_io STATIC)

if (COMPOUND_LIB)
  if (EXISTS "${COMPOUND_LIB}")
    copy_if_different("./" "${STAGE_DIR}/share/openstructure" 
                      "${COMPOUND_LIB}" "COMPOUND_LIB" ost_conop)
    install(FILES ${COMPOUND_LIB} DESTINATION "share/openstructure")
  else()
    message(FATAL_ERROR "${COMPOUND_LIB} does not exist. \n"
            "Pass -DCOMPOUND_LIB=NONE if you would like to build "
            "OpenStructure without a compound library")
  endif()

endif()
