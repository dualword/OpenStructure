import bundle
import deps
import sys
from optparse import OptionParser
import shutil
import os
p=OptionParser()
p.add_option('--bundle', action='store_true', default=False)
p.add_option('--no_rpath', action='store_true',
             default=False)
p.add_option('--macports_workaround', action='store_true', default=False)
p.add_option('--dmg', action='store_true', default=False)
opts, args=p.parse_args()
deps.make_standalone('../../stage', 'standalone', True, 
                     opts.no_rpath, 
                     macports_workaround=opts.macports_workaround)
if os.path.exists('DNG.app'):
  shutil.rmtree('DNG.app')
bundle.create_bundle('DNG', opts.bundle)
if opts.bundle:
  shutil.copytree('../../examples', 'DNG.app/Contents/examples')
  os.system('rm `find DNG.app/Contents/examples/ -name "*.pyc"` 2> /dev/null')
  os.system('rm -rf DNG.app/Contents/examples/demos/harmony')
  os.system('rm -rf DNG.app/Contents/examples/demos/dokk')
  if opts.dmg:
    os.system('rm -rf openstructure.dmg')
    os.system('hdiutil create -srcFolder DNG.app openstructure.dmg')