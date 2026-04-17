/**CFile****************************************************************

  FileName    [abcPdb.cpp]

  SystemName  [ABC: Logic synthesis and verification system.]

  PackageName [Network and node package.]

  Synopsis    [Physical partition database and APIs.]

***********************************************************************/

#include "abcPdb.hpp"
#include "abcInt.h"

ABC_NAMESPACE_IMPL_START

static inline Abc_Obj_t * Abc_NtkPartObj( Abc_Ntk_t * pNtk, int ObjId )
{
    if ( pNtk == NULL || ObjId < 0 || ObjId >= Abc_NtkObjNumMax(pNtk) )
        return NULL;
    return Abc_NtkObj( pNtk, ObjId );
}

static inline void Abc_ObjUpdateCutNetWithFanins( Abc_Obj_t * pObj )
{
    Abc_Obj_t * pObjR = Abc_ObjRegular( pObj );
    Abc_Obj_t * pFanin;
    int i;

    if ( pObjR == NULL )
        return;

    Abc_ObjUpdateCutNet( pObjR );
    Abc_ObjForEachFanin( pObjR, pFanin, i )
        Abc_ObjUpdateCutNet( pFanin );
}

Pdb * Abc_PdbAlloc()
{
    return new Pdb();
}

void Abc_PdbFree( Pdb * pPdb )
{
    delete pPdb;
}

part_id Abc_NtkGetPartId( Abc_Ntk_t * pNtk, int ObjId )
{
    if ( pNtk == NULL || pNtk->pPdb == NULL )
        return ABC_PART_ID_NONE;
    return pNtk->pPdb->get( ObjId );
}

void Abc_NtkSetPartId( Abc_Ntk_t * pNtk, int ObjId, part_id PartId )
{
    Abc_Obj_t * pObj = Abc_NtkPartObj( pNtk, ObjId );
    if ( pObj == NULL )
        return;
    if ( !Abc_PartIdIsValid( PartId ) )
    {
        if ( pNtk->pPdb )
            pNtk->pPdb->clear( ObjId );
        Abc_ObjUpdateCutNetWithFanins( pObj );
        return;
    }
    if ( pNtk->pPdb == NULL )
        pNtk->pPdb = Abc_PdbAlloc();
    pNtk->pPdb->set( ObjId, PartId );
    Abc_ObjUpdateCutNetWithFanins( pObj );
}

void Abc_NtkClearPartId( Abc_Ntk_t * pNtk, int ObjId )
{
    Abc_Obj_t * pObj = Abc_NtkPartObj( pNtk, ObjId );
    if ( pObj == NULL || pNtk->pPdb == NULL )
        return;
    pNtk->pPdb->clear( ObjId );
    Abc_ObjUpdateCutNetWithFanins( pObj );
}

void Abc_NtkClearPartIds( Abc_Ntk_t * pNtk )
{
    Abc_Obj_t * pObj;
    int i;

    if ( pNtk == NULL )
        return;
    if ( pNtk->pPdb )
    {
        Abc_PdbFree( pNtk->pPdb );
        pNtk->pPdb = NULL;
    }
    Abc_NtkForEachObj( pNtk, pObj, i )
        Abc_ObjClearCutNet( pObj );
}

void Abc_NtkUpdateCutNets( Abc_Ntk_t * pNtk )
{
    Abc_Obj_t * pObj;
    int i;

    if ( pNtk == NULL )
        return;
    Abc_NtkForEachObj( pNtk, pObj, i )
        Abc_ObjUpdateCutNet( pObj );
}

part_id Abc_ObjGetPartId( Abc_Obj_t * pObj )
{
    Abc_Obj_t * pObjR = Abc_ObjRegular( pObj );
    if ( pObjR == NULL )
        return ABC_PART_ID_NONE;
    return Abc_NtkGetPartId( pObjR->pNtk, pObjR->Id );
}

void Abc_ObjSetPartId( Abc_Obj_t * pObj, part_id PartId )
{
    Abc_Obj_t * pObjR = Abc_ObjRegular( pObj );
    if ( pObjR == NULL )
        return;
    Abc_NtkSetPartId( pObjR->pNtk, pObjR->Id, PartId );
}

void Abc_ObjClearPartId( Abc_Obj_t * pObj )
{
    Abc_Obj_t * pObjR = Abc_ObjRegular( pObj );
    if ( pObjR == NULL )
        return;
    Abc_NtkClearPartId( pObjR->pNtk, pObjR->Id );
}

void Abc_ObjUpdateCutNet( Abc_Obj_t * pObj )
{
    Abc_Obj_t * pObjR = Abc_ObjRegular( pObj );
    Abc_Obj_t * pFanout;
    part_id PartId;
    int i;

    if ( pObjR == NULL )
        return;

    Abc_ObjClearCutNet( pObjR );
    PartId = Abc_ObjGetPartId( pObjR );
    if ( !Abc_PartIdIsValid( PartId ) )
        return;

    Abc_ObjForEachFanout( pObjR, pFanout, i )
    {
        part_id FanoutPartId = Abc_ObjGetPartId( pFanout );
        if ( Abc_PartIdIsValid( FanoutPartId ) && FanoutPartId != PartId )
        {
            Abc_ObjSetCutNet( pObjR, 1 );
            return;
        }
    }
}

ABC_NAMESPACE_IMPL_END
