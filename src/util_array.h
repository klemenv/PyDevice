/*************************************************************************\
* PyDevice is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#ifndef UTIL_ARRAY_H
#define UTIL_ARRAY_H


#include <string>
#include <menuFtype.h>

#include "util.h"
template <typename T>
static std::string rec_bptr_to_strings(T* rec)
{

    switch(rec->ftvl){
    case menuFtypeCHAR:   return Util::to_pylist_string ( reinterpret_cast< epicsInt8*    >(rec->bptr) , rec->nelm ); break;
    case menuFtypeUCHAR:  return Util::to_pylist_string ( reinterpret_cast< epicsUInt8*   >(rec->bptr) , rec->nelm ); break;
    case menuFtypeSHORT:  return Util::to_pylist_string ( reinterpret_cast< epicsInt16*   >(rec->bptr) , rec->nelm ); break;
    case menuFtypeUSHORT: return Util::to_pylist_string ( reinterpret_cast< epicsUInt16*  >(rec->bptr) , rec->nelm ); break;
    case menuFtypeLONG:	  return Util::to_pylist_string ( reinterpret_cast< epicsInt32*   >(rec->bptr) , rec->nelm ); break;
    case menuFtypeULONG:  return Util::to_pylist_string ( reinterpret_cast< epicsUInt32*  >(rec->bptr) , rec->nelm ); break;
    case menuFtypeFLOAT:  return Util::to_pylist_string ( reinterpret_cast< epicsFloat32* >(rec->bptr) , rec->nelm ); break;
    case menuFtypeDOUBLE: return Util::to_pylist_string ( reinterpret_cast< epicsFloat64* >(rec->bptr) , rec->nelm ); break;
    default:
	break;
    }
    throw std::runtime_error("could not convert vertor to string") ;
}


#endif // UTIL_ARRAY_H
