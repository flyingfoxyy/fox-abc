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

static inline int Abc_ObjIsPartStatVertex( Abc_Obj_t * pObj )
{
    return pObj
        && ( Abc_ObjIsPi( pObj )
          || Abc_ObjIsNode( pObj )
          || Abc_ObjType( pObj ) == ABC_OBJ_CONST1 );
}

static inline int Abc_ObjIsHopVertex( Abc_Obj_t * pObj )
{
    return Abc_ObjIsPartStatVertex( pObj );
}

static inline int Abc_ObjIsHopInterconnect( Abc_Obj_t * pObj )
{
    return pObj
        && ( Abc_ObjIsNet( pObj )
          || Abc_ObjIsBi( pObj )
          || Abc_ObjIsBo( pObj ) );
}

static int Abc_ObjRequirePartId( Abc_Obj_t * pObj, const char * pContext )
{
    part_id PartId = Abc_ObjGetPartId( pObj );
    if ( Abc_PartIdIsValid( PartId ) )
        return 1;
    Abc_Print( -1, "%s: object %d has invalid partition id\n", pContext, Abc_ObjId( pObj ) );
    return 0;
}

static int Abc_ObjCollectHopSources( Abc_Obj_t * pObj, Vec_Ptr_t * vSources, Vec_Str_t * vVisited )
{
    Abc_Obj_t * pObjR = Abc_ObjRegular( pObj );
    Abc_Obj_t * pFanin;
    int i;

    if ( pObjR == NULL )
        return 1;
    if ( pObjR->Id < 0 || pObjR->Id >= Vec_StrSize(vVisited) )
        return 1;
    if ( Vec_StrEntry(vVisited, pObjR->Id) )
        return 1;
    Vec_StrWriteEntry( vVisited, pObjR->Id, 1 );

    if ( Abc_ObjIsLatch( pObjR ) )
    {
        Abc_Print( -1, "Abc_NtkComputeHopNum: latch object %d is not supported\n", Abc_ObjId( pObjR ) );
        return 0;
    }
    if ( Abc_ObjIsHopVertex( pObjR ) )
    {
        Vec_PtrPush( vSources, pObjR );
        return 1;
    }
    if ( !Abc_ObjIsHopInterconnect( pObjR ) )
    {
        Abc_Print( -1, "Abc_NtkComputeHopNum: unsupported object type %u (obj id %d)\n", Abc_ObjType(pObjR), Abc_ObjId(pObjR) );
        return 0;
    }

    Abc_ObjForEachFanin( pObjR, pFanin, i )
        if ( !Abc_ObjCollectHopSources( pFanin, vSources, vVisited ) )
            return 0;
    return 1;
}

static int Abc_ObjCollectPartSinks( Abc_Obj_t * pObj, Vec_Ptr_t * vSinks, Vec_Str_t * vVisited, const char * pContext )
{
    Abc_Obj_t * pObjR = Abc_ObjRegular( pObj );
    Abc_Obj_t * pFanout;
    int i;

    if ( pObjR == NULL )
        return 1;
    if ( pObjR->Id < 0 || pObjR->Id >= Vec_StrSize(vVisited) )
        return 1;
    if ( Vec_StrEntry(vVisited, pObjR->Id) )
        return 1;
    Vec_StrWriteEntry( vVisited, pObjR->Id, 1 );

    if ( Abc_ObjIsPo( pObjR ) )
        return 1;
    if ( Abc_ObjIsLatch( pObjR ) )
    {
        Abc_Print( -1, "%s: latch object %d is not supported\n", pContext, Abc_ObjId( pObjR ) );
        return 0;
    }
    if ( Abc_ObjIsPartStatVertex( pObjR ) )
    {
        Vec_PtrPush( vSinks, pObjR );
        return 1;
    }
    if ( !Abc_ObjIsHopInterconnect( pObjR ) )
    {
        Abc_Print( -1, "%s: unsupported object type %u (obj id %d)\n", pContext, Abc_ObjType(pObjR), Abc_ObjId(pObjR) );
        return 0;
    }

    Abc_ObjForEachFanout( pObjR, pFanout, i )
        if ( !Abc_ObjCollectPartSinks( pFanout, vSinks, vVisited, pContext ) )
            return 0;
    return 1;
}

static int Abc_NtkComputeHopNum_rec( Abc_Obj_t * pObj, Vec_Int_t * vHopNums, Vec_Str_t * vState )
{
    Abc_Obj_t * pObjR = Abc_ObjRegular( pObj );
    Vec_Ptr_t * vSources;
    Vec_Str_t * vVisited;
    Abc_Obj_t * pSource;
    int i;
    int Best = 0;

    if ( pObjR == NULL )
        return 0;
    if ( !Abc_ObjIsHopVertex( pObjR ) )
    {
        Abc_Print( -1, "Abc_NtkComputeHopNum: object %d is not a hop vertex\n", Abc_ObjId( pObjR ) );
        return -1;
    }
    if ( !Abc_ObjRequirePartId( pObjR, "Abc_NtkComputeHopNum" ) )
        return -1;
    if ( Vec_StrEntry(vState, pObjR->Id) == 2 )
        return Vec_IntEntry( vHopNums, pObjR->Id );
    if ( Vec_StrEntry(vState, pObjR->Id) == 1 )
    {
        Abc_Print( -1, "Abc_NtkComputeHopNum: combinational cycle detected at object %d\n", Abc_ObjId( pObjR ) );
        return -1;
    }

    Vec_StrWriteEntry( vState, pObjR->Id, 1 );
    if ( Abc_ObjIsPi(pObjR) || Abc_ObjType(pObjR) == ABC_OBJ_CONST1 )
    {
        Vec_IntWriteEntry( vHopNums, pObjR->Id, 0 );
        Vec_StrWriteEntry( vState, pObjR->Id, 2 );
        return 0;
    }

    vSources = Vec_PtrAlloc( 4 );
    vVisited = Vec_StrStart( Abc_NtkObjNumMax(pObjR->pNtk) );
    Abc_ObjForEachFanin( pObjR, pSource, i )
        if ( !Abc_ObjCollectHopSources( pSource, vSources, vVisited ) )
        {
            Vec_PtrFree( vSources );
            Vec_StrFree( vVisited );
            Vec_StrWriteEntry( vState, pObjR->Id, 0 );
            return -1;
        }
    Vec_StrFree( vVisited );

    Vec_PtrSort( vSources, (int (*)(const void *, const void *))Vec_PtrSortComparePtr );
    Vec_PtrUniqify( vSources, NULL );
    Vec_PtrForEachEntry( Abc_Obj_t *, vSources, pSource, i )
    {
        int SourceHop = Abc_NtkComputeHopNum_rec( pSource, vHopNums, vState );
        part_id SourcePart;
        part_id ObjectPart;
        int Candidate;

        if ( SourceHop < 0 )
        {
            Vec_PtrFree( vSources );
            Vec_StrWriteEntry( vState, pObjR->Id, 0 );
            return -1;
        }
        if ( !Abc_ObjRequirePartId( pSource, "Abc_NtkComputeHopNum" ) )
        {
            Vec_PtrFree( vSources );
            Vec_StrWriteEntry( vState, pObjR->Id, 0 );
            return -1;
        }

        SourcePart = Abc_ObjGetPartId( pSource );
        ObjectPart = Abc_ObjGetPartId( pObjR );
        Candidate = SourceHop + (SourcePart != ObjectPart);
        if ( Candidate > Best )
            Best = Candidate;
    }
    Vec_PtrFree( vSources );

    Vec_IntWriteEntry( vHopNums, pObjR->Id, Best );
    Vec_StrWriteEntry( vState, pObjR->Id, 2 );
    return Best;
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

int Abc_NtkComputeCutSize( Abc_Ntk_t * pNtk )
{
    Vec_Ptr_t * vSinks;
    Vec_Str_t * vVisited;
    Abc_Obj_t * pObj;
    Abc_Obj_t * pSink;
    int i;
    int CutSize = 0;

    if ( pNtk == NULL )
        return -1;
    if ( Abc_NtkLatchNum(pNtk) > 0 )
    {
        Abc_Print( -1, "Abc_NtkComputeCutSize: latch objects are not supported\n" );
        return -1;
    }

    vSinks = Vec_PtrAlloc( 4 );
    vVisited = Vec_StrStart( Abc_NtkObjNumMax(pNtk) );
    Abc_NtkForEachObj( pNtk, pObj, i )
    {
        part_id PartId;
        int k;

        if ( !Abc_ObjIsPartStatVertex(pObj) )
            continue;
        if ( !Abc_ObjRequirePartId( pObj, "Abc_NtkComputeCutSize" ) )
        {
            Vec_PtrFree( vSinks );
            Vec_StrFree( vVisited );
            return -1;
        }

        Vec_PtrClear( vSinks );
        Vec_StrFill( vVisited, Vec_StrSize(vVisited), 0 );
        Abc_ObjForEachFanout( pObj, pSink, k )
            if ( !Abc_ObjCollectPartSinks( pSink, vSinks, vVisited, "Abc_NtkComputeCutSize" ) )
            {
                Vec_PtrFree( vSinks );
                Vec_StrFree( vVisited );
                return -1;
            }

        Vec_PtrSort( vSinks, (int (*)(const void *, const void *))Vec_PtrSortComparePtr );
        Vec_PtrUniqify( vSinks, NULL );
        PartId = Abc_ObjGetPartId( pObj );
        Vec_PtrForEachEntry( Abc_Obj_t *, vSinks, pSink, k )
        {
            if ( !Abc_ObjRequirePartId( pSink, "Abc_NtkComputeCutSize" ) )
            {
                Vec_PtrFree( vSinks );
                Vec_StrFree( vVisited );
                return -1;
            }
            if ( Abc_ObjGetPartId( pSink ) != PartId )
            {
                CutSize++;
                break;
            }
        }
    }

    Vec_PtrFree( vSinks );
    Vec_StrFree( vVisited );
    return CutSize;
}

int Abc_NtkComputeHopNum( Abc_Ntk_t * pNtk )
{
    Vec_Int_t * vHopNums;
    Vec_Str_t * vState;
    Abc_Obj_t * pObj;
    int i;
    int HopNum = 0;

    if ( pNtk == NULL )
        return -1;
    if ( Abc_NtkLatchNum(pNtk) > 0 )
    {
        Abc_Print( -1, "Abc_NtkComputeHopNum: latch objects are not supported\n" );
        return -1;
    }

    vHopNums = Vec_IntStartFull( Abc_NtkObjNumMax(pNtk) );
    vState = Vec_StrStart( Abc_NtkObjNumMax(pNtk) );
    Abc_NtkForEachObj( pNtk, pObj, i )
    {
        int ObjHop;

        if ( !Abc_ObjIsHopVertex(pObj) )
            continue;
        ObjHop = Abc_NtkComputeHopNum_rec( pObj, vHopNums, vState );
        if ( ObjHop < 0 )
        {
            Vec_IntFree( vHopNums );
            Vec_StrFree( vState );
            return -1;
        }
        if ( ObjHop > HopNum )
            HopNum = ObjHop;
    }

    Vec_IntFree( vHopNums );
    Vec_StrFree( vState );
    return HopNum;
}

void Abc_NtkSetPartStats( Abc_Ntk_t * pNtk, int NumParts, int CutSize, int HopNum )
{
    if ( pNtk == NULL || pNtk->pPdb == NULL )
        return;
    pNtk->pPdb->set_stats( NumParts, CutSize, HopNum );
}

int Abc_NtkGetPartStats( Abc_Ntk_t * pNtk, int * pNumParts, int * pCutSize, int * pHopNum, float * pAvgSize, int * pMinSize, int * pMaxSize )
{
    int Counts[ABC_PART_ID_NONE] = {0};
    int nAssigned = 0;
    int nDistinct = 0;
    int nParts = 0;
    int MinSize = 0;
    int MaxSize = 0;
    int CutSize = 0;
    int HopNum = 0;
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
        CutSize = Abc_NtkComputeCutSize( pNtk );
        if ( CutSize < 0 )
            return 0;
    }
    if ( pNtk->pPdb->hop_num() >= 0 )
        HopNum = pNtk->pPdb->hop_num();
    else
    {
        HopNum = Abc_NtkComputeHopNum( pNtk );
        if ( HopNum < 0 )
            return 0;
    }

    if ( pNumParts )
        *pNumParts = nParts;
    if ( pCutSize )
        *pCutSize = CutSize;
    if ( pHopNum )
        *pHopNum = HopNum;
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
    Vec_Ptr_t * vSinks;
    Vec_Str_t * vVisited;
    int i;

    if ( pObjR == NULL )
        return;

    Abc_ObjClearCutNet( pObjR );
    if ( !Abc_ObjIsPartStatVertex( pObjR ) )
        return;
    PartId = Abc_ObjGetPartId( pObjR );
    if ( !Abc_PartIdIsValid( PartId ) )
        return;

    vSinks = Vec_PtrAlloc( 4 );
    vVisited = Vec_StrStart( Abc_NtkObjNumMax(pObjR->pNtk) );
    Abc_ObjForEachFanout( pObjR, pFanout, i )
    {
        Abc_Obj_t * pSink;
        int k;

        if ( !Abc_ObjCollectPartSinks( pFanout, vSinks, vVisited, "Abc_ObjUpdateCutNet" ) )
            break;
        Vec_PtrSort( vSinks, (int (*)(const void *, const void *))Vec_PtrSortComparePtr );
        Vec_PtrUniqify( vSinks, NULL );
        Vec_PtrForEachEntry( Abc_Obj_t *, vSinks, pSink, k )
        {
            part_id SinkPartId = Abc_ObjGetPartId( pSink );
            if ( Abc_PartIdIsValid( SinkPartId ) && SinkPartId != PartId )
            {
                Abc_ObjSetCutNet( pObjR, 1 );
                Vec_PtrFree( vSinks );
                Vec_StrFree( vVisited );
                return;
            }
        }
        Vec_PtrClear( vSinks );
        Vec_StrFill( vVisited, Vec_StrSize(vVisited), 0 );
    }

    Vec_PtrFree( vSinks );
    Vec_StrFree( vVisited );
}

ABC_NAMESPACE_IMPL_END
