//------------------------------------------------------------------------------
// This file is part of the OpenStructure project <www.openstructure.org>
//
// Copyright (C) 2008-2011 by the OpenStructure authors
//
// This library is free software; you can redistribute it and/or modify it under
// the terms of the GNU Lesser General Public License as published by the Free
// Software Foundation; either version 3.0 of the License, or (at your option)
// any later version.
// This library is distributed in the hope that it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for more
// details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this library; if not, write to the Free Software Foundation, Inc.,
// 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
//------------------------------------------------------------------------------
#ifndef GEOM_VEC3_H
#define GEOM_VEC3_H

#include <stdexcept>
#include <cstddef> // for size_t
#include <ostream>
#include <vector>
#include <boost/operators.hpp>


#include <ost/config.hh>
#include <ost/geom/module_config.hh>
#include <ost/geom/exc.hh>
namespace geom {

// fw decl
class Vec2;
class Vec4;
class Line3;
class Plane;

/// \brief Three dimensional vector class, using Real precision.
class DLLEXPORT_OST_GEOM Vec3:
    private boost::equality_comparable<Vec3>,
    private boost::additive<Vec3>,
    private boost::additive<Vec3, Real>,
    private boost::multiplicative<Vec3, Real>
{
public:
  //! Default initialization, all components are set to zero
  Vec3(): x(0), y(0), z(0) {}

  //! Initialization with x, y and z component
  Vec3(Real px, Real py, Real pz): x(px), y(py), z(pz) {}

  //! copy ctor
  Vec3(const Vec3& v): x(v.x), y(v.y), z(v.z) { }

  //! (implicit) initialization with 2D vector
  Vec3(const Vec2& v);

  //! explicit initialization with a 4D homogeneous vector
  /*!
    a 4D (homogeneous) vector can be explicitely converted
    to a 3D vector, resulting in (x/w,y/w,z/w)
  */
  explicit Vec3(const Vec4& v);
  
  explicit Vec3(Real v): x(v), y(v), z(v) { }
  
  //! explicit initialization with an array of doubles
  explicit Vec3(const double v[3]): x(v[0]), y(v[1]), z(v[2]) { }

  //! explicit initialization with an array of floats
  explicit Vec3(const float v[3]): x(v[0]), y(v[1]), z(v[2]) { }

  //! assignement op
  Vec3& operator=(const Vec3& v)
  {
    x=v.x;
    y=v.y;
    z=v.z;
    return *this;
  }

  //! comparable
  bool operator==(const Vec3& rhs) const
  {
    return x==rhs.x && y==rhs.y && z==rhs.z;
  }

  //! element access
  Real& operator[](std::size_t indx)
  {
    if (indx>2) {
      throw std::out_of_range("Index must be in the range [0-2]");
    }
    return (&x)[indx];
  }
  
  //! const element access
  const Real& operator[](std::size_t indx) const
  {
    if (indx>2) {
      throw std::out_of_range("Index must be in the range [0-2]");
    }
    return (&x)[indx];
  }
  //! element access
  Real GetX() const { return x; }
  Real GetY() const { return y; }
  Real GetZ() const { return z; }
  void SetX(Real v) { x=v; }
  void SetY(Real v) { y=v; }
  void SetZ(Real v) { z=v; }

  //! addable op
  Vec3& operator+=(const Vec3& rhs)
  {
    x+=rhs.x;
    y+=rhs.y;
    z+=rhs.z;
    return *this;
  }
  
  Vec3& operator+=(Real d)
  {
    x+=d;
    y+=d;
    z+=d;
    return *this;
  }
  
  //! subtractable op
  Vec3& operator-=(const Vec3& rhs)
  {
    x-=rhs.x;
    y-=rhs.y;
    z-=rhs.z;
    return *this;
  }
  
  Vec3& operator-=(Real d)
  {
    x-=d;
    y-=d;
    z-=d;
    return *this;
  }
  //! negateable
  Vec3 operator-() const
  {
    return Vec3(-x, -y, -z);
  }
  
  //! multipliable
  Vec3& operator*=(Real d)
  {
    x*=d;
    y*=d;
    z*=d;
    return *this;
  }
  
  //! dividable
  Vec3& operator/=(Real d)
  {
    Real one_over_d=Real(1.0)/d;
    x*=one_over_d;
    y*=one_over_d;
    z*=one_over_d;
    return *this;
  }

  Real* Data() {return &x;}
  const Real* Data() const {return &x;}

  Real x;
  Real y;
  Real z;
};

inline Vec3 operator/(Real d, const Vec3& v)
{
  Vec3 nrvo(d/v[0],d/v[1],d/v[2]);
  return nrvo;
}

inline std::ostream& operator<<(std::ostream& os, const Vec3& v)
{
  os << "[" << v.x << ", " << v.y << ", " << v.z << "]";
  return os;
}
} // ns geom

namespace geom {

  // TODO: move to separate file
  class Mat3;

class DLLEXPORT_OST_GEOM Vec3List : public std::vector<Vec3> {
public:
  typedef std::vector<Vec3> base_type;
  Vec3List() : base_type() {}
  
  Vec3List(size_t size, const Vec3& value=Vec3()) : base_type(size, value) {}
  Vec3List(base_type::iterator b, base_type::iterator e): base_type(b, e) { }
  
  Vec3List(const Vec3List& rhs) : base_type(rhs) { }
  Vec3List(const base_type& rhs) : base_type(rhs) { }
  Vec3List& operator=(const Vec3List& rhs) 
  {
    base_type::operator=(rhs);
    return *this;
  }

  // TODO: move some or all of these to stand-alone functions
  Mat3 GetInertia() const;
  Vec3 GetCenter() const;
  Mat3 GetPrincipalAxes() const;
  Line3 GetODRLine() const;
  Plane GetODRPlane() const;

  //This function fits a cylinder to the positions in Vec3List
  //It takes as argument an initial guess for the direction and the geometric
  //center of the atoms. The center is not changed during optimisation as the
  //best fitting cylinder can be shown to have its axis pass through the geometric center
  Line3 FitCylinder(const Vec3& initial_direction, const Vec3& center) const;
};
} // ns geom


#include <ost/geom/vec2.hh>
#include <ost/geom/vec4.hh>
#include <ost/geom/mat3.hh>
#include <ost/geom/composite3.hh>

namespace geom {
  inline Vec3::Vec3(const Vec2& v): x(v.x), y(v.y), z(0.0) { }
  
  inline Vec3::Vec3(const Vec4& v): x(v.x), y(v.y), z(v.z) 
  { 
    if (std::fabs(v.w)<1e-10) {
      // it is better to ignore very small w and to simply assume
      // that this is not a homogeneous coordinate rather than
      // throwing an exception
      //throw DivideByZeroException();
    } else {
      x/=v.w;
      y/=v.w;
      z/=v.w;
    }
  }
} // namespace geom


# endif
