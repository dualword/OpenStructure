set(OST_BASE_PYMOD_SOURCES
  wrap_base.cc
  export_logger.cc
  export_range.cc
  export_units.cc
)

pymod(NAME base OUTPUT_DIR ost 
      CPP ${OST_BASE_PYMOD_SOURCES} 
      PY __init__.py settings.py stutil.py table.py xmlrunner.py testutils.py)
