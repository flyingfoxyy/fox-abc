/**CFile****************************************************************

  FileName    [abcPdb.cpp]

  SystemName  [ABC: Logic synthesis and verification system.]

  PackageName [Network and node package.]

  Synopsis    [Physical partition database and APIs.]

***********************************************************************/

#include "abcPdb.hpp"
#include "abcInt.h"

ABC_NAMESPACE_IMPL_START

static inline void Abc_NtkInvalidatePartStats( Abc_Ntk_t * pNtk )
{
    if ( pNtk && pNtk->pPdb )
        pNtk->pPdb->invalidate_stats();
}

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
        {
            pNtk->pPdb->clear( ObjId );
            pNtk->pPdb->invalidate_stats();
        }
        Abc_ObjUpdateCutNetWithFanins( pObj );
        return;
    }
    if ( pNtk->pPdb == NULL )
        pNtk->pPdb = Abc_PdbAlloc();
    else
        pNtk->pPdb->invalidate_stats();
    pNtk->pPdb->set( ObjId, PartId );
    Abc_ObjUpdateCutNetWithFanins( pObj );
}

void Abc_NtkClearPartId( Abc_Ntk_t * pNtk, int ObjId )
{
    Abc_Obj_t * pObj = Abc_NtkPartObj( pNtk, ObjId );
    if ( pObj == NULL || pNtk->pPdb == NULL )
        return;
    pNtk->pPdb->clear( ObjId );
    pNtk->pPdb->invalidate_stats();
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
    Abc_NtkInvalidatePartStats( pNtk );
}

void Abc_NtkSetPartStats( Abc_Ntk_t * pNtk, int NumParts, int CutSize )
{
    if ( pNtk == NULL || pNtk->pPdb == NULL )
        return;
    pNtk->pPdb->set_stats( NumParts, CutSize );
}

int Abc_NtkGetPartStats( Abc_Ntk_t * pNtk, int * pNumParts, int * pCutSize, float * pAvgSize, int * pMinSize, int * pMaxSize )
{
    int Counts[ABC_PART_ID_NONE] = {0};
    int nAssigned = 0;
    int nDistinct = 0;
    int nParts = 0;
    int MinSize = 0;
    int MaxSize = 0;
    int CutSize = 0;
    int i;
    Abc_Obj_t * pObj;

    if ( pNtk == NULL || pNtk->pPdb == NULL )
        return 0;

    Abc_NtkForEachObj( pNtk, pObj, i )
    {
        part_id PartId = Abc_ObjGetPartId( pObj );
        if ( !Abc_PartIdIsValid( PartId ) )
            continue;
        Counts[PartId]++;
        nAssigned++;
    }

    for ( i = 0; i < ABC_PART_ID_NONE; ++i )
        nDistinct += Counts[i] > 0;

    nParts = pNtk->pPdb->num_parts() > 0 ? pNtk->pPdb->num_parts() : nDistinct;
    if ( nParts <= 0 )
        return 0;

    if ( pNtk->pPdb->num_parts() > 0 )
    {
        MinSize = Counts[0];
        MaxSize = Counts[0];
        for ( i = 1; i < nParts; ++i )
        {
            if ( Counts[i] < MinSize )
                MinSize = Counts[i];
            if ( Counts[i] > MaxSize )
                MaxSize = Counts[i];
        }
    }
    else
    {
        for ( i = 0; i < ABC_PART_ID_NONE; ++i )
            if ( Counts[i] > 0 )
            {
                MinSize = MaxSize = Counts[i];
                break;
            }
        for ( ; i < ABC_PART_ID_NONE; ++i )
            if ( Counts[i] > 0 )
            {
                if ( Counts[i] < MinSize )
                    MinSize = Counts[i];
                if ( Counts[i] > MaxSize )
                    MaxSize = Counts[i];
            }
    }

    if ( pNtk->pPdb->cut_size() >= 0 )
        CutSize = pNtk->pPdb->cut_size();
    else
    {
        Abc_NtkForEachObj( pNtk, pObj, i )
            CutSize += Abc_ObjIsCutNet( pObj );
    }

    if ( pNumParts )
        *pNumParts = nParts;
    if ( pCutSize )
        *pCutSize = CutSize;
    if ( pAvgSize )
        *pAvgSize = nParts > 0 ? (float)nAssigned / (float)nParts : 0.0f;
    if ( pMinSize )
        *pMinSize = MinSize;
    if ( pMaxSize )
        *pMaxSize = MaxSize;
    return 1;
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
