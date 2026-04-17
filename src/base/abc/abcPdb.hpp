#ifndef ABC__base__abc__abcPdb_hpp
#define ABC__base__abc__abcPdb_hpp

#ifdef __cplusplus

#include <algorithm>
#include <vector>

#include "abc.h"

ABC_NAMESPACE_CXX_HEADER_START

struct Pdb
{
    explicit Pdb( std::size_t nObjs = 0 )
        : m_part_ids( nObjs, ABC_PART_ID_NONE )
    {
    }

    part_id get( int ObjId ) const
    {
        if ( ObjId < 0 || static_cast<std::size_t>(ObjId) >= m_part_ids.size() )
            return ABC_PART_ID_NONE;
        return m_part_ids[ObjId];
    }

    void set( int ObjId, part_id PartId )
    {
        assert( ObjId >= 0 );
        ensure( ObjId );
        m_part_ids[ObjId] = PartId;
    }

    void clear( int ObjId )
    {
        if ( ObjId < 0 || static_cast<std::size_t>(ObjId) >= m_part_ids.size() )
            return;
        m_part_ids[ObjId] = ABC_PART_ID_NONE;
    }

    void clear_all()
    {
        std::fill( m_part_ids.begin(), m_part_ids.end(), ABC_PART_ID_NONE );
    }

private:
    void ensure( int ObjId )
    {
        if ( static_cast<std::size_t>(ObjId) >= m_part_ids.size() )
            m_part_ids.resize( static_cast<std::size_t>(ObjId) + 1, ABC_PART_ID_NONE );
    }

private:
    std::vector<part_id> m_part_ids;
};

ABC_NAMESPACE_CXX_HEADER_END

#endif

#endif
