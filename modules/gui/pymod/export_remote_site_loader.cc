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

#include <boost/python.hpp>
#include <string>


#include <ost/gui/remote_site_loader.hh>
#include <boost/python/register_ptr_to_python.hpp>
#include <boost/python/suite/indexing/vector_indexing_suite.hpp>

#include "sip_handler.hh"

using namespace ost;
using namespace ost::gui;
using namespace boost::python;

struct WrappedRemoteSiteLoader : public RemoteSiteLoader
{
    WrappedRemoteSiteLoader(PyObject *p):
        RemoteSiteLoader(), self(p)
    { }

    virtual void LoadById(const QString& id, const QString& selection=QString()){
       return call_method<void, std::string>(self, "LoadById", id.toStdString(), selection.toStdString());
    }

    virtual QString GetRemoteSiteName(){
      return call_method<QString>(self, "GetRemoteSiteName");
    }

    virtual bool IsImg() const{
      return call_method<bool>(self, "IsImg");
    }

    virtual QNetworkReply* ById(const QString& id, const QString& selection=QString()){
      object obj = call_method<object, std::string, std::string>(self, "ById", id.toStdString(), selection.toStdString());
      QNetworkReply* network_reply= get_cpp_qobject<QNetworkReply>(obj);
      if(network_reply){
        return network_reply;
      }
      return NULL;
    }

 private:
    PyObject* self;
};

void export_RemoteSiteLoader()
{
  class_<RemoteSiteLoader, WrappedRemoteSiteLoader, boost::noncopyable>("RemoteSiteLoader")
    .def("LoadById",&WrappedRemoteSiteLoader::LoadById)
    .def("GetRemoteSiteName", &WrappedRemoteSiteLoader::GetRemoteSiteName)
    .def("IsImg", &WrappedRemoteSiteLoader::IsImg)
  ;

}



