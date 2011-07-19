import unittest
if __name__== '__main__':
  import sys
  sys.path.insert(0,"../../../stage/lib64/openstructure/")

import ost
import ost.gfx as gfx
import ost.geom as geom

def col_delta(c1,c2):
  return geom.Distance(geom.Vec3(c1[0],c1[1],c1[2]),geom.Vec3(c2[0],c2[1],c2[2]))

class TestGfx(unittest.TestCase):
  def runTest(self):
    self.test_gradient()
    self.test_color()
    self.test_primlist()

  def test_gradient(self):
    gs=[gfx.Gradient(),
        gfx.Gradient({0.0: [1,0,0], 1.0: gfx.Color(0,1,0)})]
    gs[0].SetColorAt(0.0,gfx.Color(1.0,0.0,0.0))
    gs[0].SetColorAt(1.0,gfx.Color(0.0,1.0,0.0))
    for g in gs:
      self.assertAlmostEqual(col_delta(g.GetColorAt(0.0),gfx.Color(1.0,0,0)),0.0)
      self.assertAlmostEqual(col_delta(g.GetColorAt(0.5),gfx.Color(0.5,0.5,0)),0.0)
      self.assertAlmostEqual(col_delta(g.GetColorAt(1.0),gfx.Color(0,1.0,0)),0.0)

  def test_color(self):
    c=gfx.Color(0.5,0.3,0.2)
    self.assertAlmostEqual(c.r,0.5)
    self.assertAlmostEqual(c.g,0.3)
    self.assertAlmostEqual(c.b,0.2)
    self.assertAlmostEqual(c.a,1.0)
    self.assertAlmostEqual(c.red,0.5)
    self.assertAlmostEqual(c.green,0.3)
    self.assertAlmostEqual(c.blue,0.2)
    self.assertAlmostEqual(c.alpha,1.0)
    self.assertAlmostEqual(c[0],0.5)
    self.assertAlmostEqual(c[1],0.3)
    self.assertAlmostEqual(c[2],0.2)
    self.assertAlmostEqual(c[3],1.0)
    c.r=0.9
    self.assertAlmostEqual(c.r,0.9)
    self.assertAlmostEqual(c.red,0.9)
    self.assertAlmostEqual(c[0],0.9)

  def test_primlist(self):
    pl=gfx.PrimList("foo")
    pl.AddPoint([0,0,0])
    pl.AddPoint(geom.Vec3(1,2,3),color=gfx.RED)
    pl.AddLine([0,0,0],[1,2,3])
    pl.AddLine(geom.Vec3(0,0,0),geom.Vec3(1,2,3),color=gfx.BLUE)
    pl.AddSphere([0,0,0],radius=2.0)
    pl.AddSphere(geom.Vec3(1,2,3),color=gfx.RED,radius=3.0)
    pl.AddCyl([0,0,0],[1,2,3],radius=0.5,color=gfx.YELLOW)
    pl.AddCyl(geom.Vec3(0,0,0),geom.Vec3(1,2,3),radius1=0.5,radius2=0.1,color1=gfx.BLUE,color2=gfx.GREEN)
    pl.AddText("foo",[0,2,3])
    pl.AddText("bar",[-2,0,0],color=gfx.WHITE,point_size=8)

if __name__== '__main__':
  unittest.main()
