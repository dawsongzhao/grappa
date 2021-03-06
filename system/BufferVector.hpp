////////////////////////////////////////////////////////////////////////
// Copyright (c) 2010-2015, University of Washington and Battelle
// Memorial Institute.  All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
//     * Redistributions of source code must retain the above
//       copyright notice, this list of conditions and the following
//       disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials
//       provided with the distribution.
//     * Neither the name of the University of Washington, Battelle
//       Memorial Institute, or the names of their contributors may be
//       used to endorse or promote products derived from this
//       software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
// FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
// UNIVERSITY OF WASHINGTON OR BATTELLE MEMORIAL INSTITUTE BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
// OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
// BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
// LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
// USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
// DAMAGE.
////////////////////////////////////////////////////////////////////////

#ifndef BUFFER_VECTOR
#define BUFFER_VECTOR

#include "LocaleSharedMemory.hpp"

#include <stdlib.h>

/* Vector that exposes its storage array as read-only.
 * increases in size as elements are added.
 *
 * Only supports insertions. Insertions allowed only
 * while vector is in write only (WO) mode and 
 * getting the buffer pointer only allowed in read only mode
 */

template < typename T >
class BufferVector {

  private:
    T * buf;
    size_t size;
    uint64_t nextIndex;

    enum OpMode { RO, WO };
    OpMode mode;

  public:
    BufferVector( size_t capacity = 2 ) 
      : buf ( static_cast<T*>(Grappa::impl::locale_shared_memory.allocate( capacity*sizeof(T) )) )   // new T[capacity]
      , mode( WO )
      , nextIndex( 0 )
      , size( capacity ) { }

    ~BufferVector() {
      Grappa::impl::locale_shared_memory.deallocate( buf );
    }

    void setWriteMode() {
      CHECK( mode != WO ) << "already in WO mode";
      mode = WO;
    }

    void setReadMode() {
      CHECK( mode != RO ) << "already in RO mode";
      mode = RO;
    }

    void insert( const T& v ) {
      CHECK( mode == WO ) << "Must be in WO mode to insert";

      if ( nextIndex == size ) {
        // expand the size of the buffer
        size_t newsize = size * 2;
        VLOG(3) << "New size of cell size: " << newsize << " bytes: " << newsize*sizeof(T);
        T * newbuf = static_cast<T*>(Grappa::impl::locale_shared_memory.allocate( newsize*sizeof(T) ));      // new T[newsize]
        memcpy( newbuf, buf, size*sizeof(T) );
        Grappa::impl::locale_shared_memory.deallocate( buf );   // delete buf
        size = newsize;
        buf = newbuf;
      }

      // store element and increment index
      buf[nextIndex++] = v;
    }

    GlobalAddress<const T> getReadBuffer() const {
      CHECK( mode == RO ) << "Must be in RO mode to see buffer";
      return make_global( const_cast<const T*>(buf) );
    }

    size_t getLength() const {
      return nextIndex; 
    }

    /* to close the safety loop, RO clients should release buffers,
     * allowing setWriteMode() to check that all buffers are released
     * but currently we will avoid this extra communication
     *
     * If we want more fine-grained concurrent R/W then it is better
     * not to expose this getBuffer type of interface 
     */

    template < typename T1 >
    friend std::ostream& operator<<( std::ostream& o, const BufferVector<T1>& v );
};

template< typename T >
std::ostream& operator<<( std::ostream& o, const BufferVector<T>& v ) {
  o << "BV(" 
             << "size=" << v.size << ", "
             << "nextIndex=" << v.nextIndex << /*", "
             << "mode=" << v.mode==BufferVector<T>::OpMode::RO?"RO":"WO" <<*/ ")[";

  // print contents; ignores RO/WO functionality
  for (uint64_t i=0; i<v.nextIndex; i++) {
    int64_t r;
    o << v.buf[i] << ",";
  }
  o << "]";
  return o;
}

#endif // BUFFER_VECTOR
