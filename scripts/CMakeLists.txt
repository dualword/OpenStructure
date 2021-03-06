set(SUBST_DICT BUILD_TYPE=${CMAKE_BUILD_TYPE} PYTHON_BINARY=${PYTHON_BINARY} LIBDIR=${LIB_DIR} LIBEXEC_PATH=${LIBEXEC_PATH})
message("${BUILD_TYPE}")
add_custom_target(ost_scripts ALL)

# may god forgive us
if (ENABLE_GUI)
  add_dependencies(ost_scripts _ost_gui)
elseif(ENABLE_GFX)
  add_dependencies(ost_scripts _ost_gfx)
else()
  add_dependencies(ost_scripts _ost_io)
endif()

if(WIN32)
  script(NAME  dng.bat INPUT dng.bat.in SUBSTITUTE ${SUBST_DICT}
         TARGET ost_scripts)
  script(NAME ost.bat INPUT ost.bat.in SUBSTITUTE ${SUBST_DICT}
         TARGET ost_scripts)
  script(NAME gipltng.bat INPUT gipltng.bat.in SUBSTITUTE ${SUBST_DICT})
else()
  script(NAME ost_config INPUT ost_config.in SUBSTITUTE ${SUBST_DICT}
         TARGET ost_scripts OUTPUT_DIR ${LIBEXEC_PATH})
  script(NAME ost INPUT ost.in SUBSTITUTE ${SUBST_DICT} 
         TARGET ost_scripts)
  if (ENABLE_GUI)
    script(NAME dng INPUT dng.in SUBSTITUTE ${SUBST_DICT}
           TARGET ost_scripts)
  endif()
endif()

set(INIT_SCRIPTS 
  init_cl.py
)

if (ENABLE_GUI)
  list(APPEND INIT_SCRIPTS init.py)
endif()
copy_if_different("./" "${STAGE_DIR}/${LIB_DIR}/openstructure" 
                  "${INIT_SCRIPTS}" "python init scripts" ost_scripts)

install(FILES ${INIT_SCRIPTS} DESTINATION "${LIB_DIR}/openstructure")

