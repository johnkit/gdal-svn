/******************************************************************************
 * $Id$
 *
 * Project:  Oracle Spatial Driver
 * Purpose:  Implementation of the OGROCITableLayer class.  This class provides
 *           layer semantics on a table, but utilizing alot of machinery from
 *           the OGROCILayer base class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2002, Frank Warmerdam <warmerdam@pobox.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 ******************************************************************************
 *
 * $Log$
 * Revision 1.34  2005/10/27 13:42:26  fwarmerdam
 * preliminary DeleteFeature implementation
 *
 * Revision 1.33  2005/06/23 16:07:55  fwarmerdam
 * Use polygon instead of rect object for sdo_filter.  Apparently this is needed
 * for queries against geodetic coordinates.  Patch provided by Lorenzo
 * Sassolini (http://www.maior.it).
 *
 * Revision 1.32  2005/02/22 12:57:51  fwarmerdam
 * use OGRLayer base spatial filter support
 *
 * Revision 1.31  2005/02/10 15:46:02  fwarmerdam
 * added GEOMETRY_NAME layer creation option
 *
 * Revision 1.30  2004/11/22 19:24:15  fwarmerdam
 * added support for a list of tables in the datasource name
 *
 * Revision 1.29  2004/11/09 02:49:12  fwarmerdam
 * Don't add user_sdo_geom_metadata record till finalize (layer load complete)
 * time as that is when we have all the diminfo details.  Inserts of metadata
 * records with NULL diminfos fails in Oracle 10 due to new constraints.
 *
 * Revision 1.28  2004/09/28 17:19:25  fwarmerdam
 * Added untested support for OCI_FID config variable for FID column name.
 *
 * Revision 1.27  2003/09/18 15:32:59  warmerda
 * Added view support as per bug 394
 *
 * Revision 1.26  2003/09/17 16:36:33  warmerda
 * fixed setting of dimension for point objects
 *
 * Revision 1.25  2003/09/13 03:48:36  warmerda
 * actually use the papaeFieldInd to write NULL fields properly in bound create
 *
 * Revision 1.24  2003/05/21 03:54:01  warmerda
 * expand tabs
 *
 * Revision 1.23  2003/04/22 19:37:51  warmerda
 * Added sync to disk
 *
 * Revision 1.22  2003/04/11 16:27:42  warmerda
 * add support for bound feature writing in OGROCITableLayer
 *
 * Revision 1.21  2003/04/04 06:18:08  warmerda
 * first pass implementation of loader support
 *
 * Revision 1.20  2003/02/06 21:16:37  warmerda
 * restructure to handle elem_info indirectly like ordinals
 *
 * Revision 1.19  2003/01/15 05:47:06  warmerda
 * fleshed out SRID writing support in newly created geometries
 *
 * Revision 1.18  2003/01/15 05:36:14  warmerda
 * start work on creating feature geometry with SRID
 *
 * Revision 1.17  2003/01/14 22:15:02  warmerda
 * added logic to order rings properly
 *
 * Revision 1.16  2003/01/14 16:59:03  warmerda
 * added field truncation support
 *
 * Revision 1.15  2003/01/14 15:31:08  warmerda
 * added fallback support if no spatial index available
 *
 * Revision 1.14  2003/01/14 15:11:00  warmerda
 * Added layer creation options caching on layer.
 * Set SRID in spatial query.
 * Support user override of DIMINFO bounds in FinalizeNewLayer().
 * Support user override of indexing options, or disabling of indexing.
 *
 * Revision 1.13  2003/01/13 13:50:13  warmerda
 * dont quote table names, it doesnt seem to work with userid.tablename
 *
 * Revision 1.12  2003/01/10 22:31:53  warmerda
 * no longer encode ordinates into SQL statement when creating features
 *
 * Revision 1.11  2003/01/09 21:19:12  warmerda
 * improved data type support, get/set precision
 *
 * Revision 1.10  2003/01/07 22:24:35  warmerda
 * added SRS support
 *
 * Revision 1.9  2003/01/07 21:14:20  warmerda
 * implement GetFeature() and SetFeature()
 *
 * Revision 1.8  2003/01/07 18:16:01  warmerda
 * implement spatial filtering in Oracle, re-enable index build
 *
 * Revision 1.7  2003/01/06 18:00:34  warmerda
 * Restructure geometry translation ... collections now supported.
 * Dimension is now a layer wide attribute.
 * Update dimension info in USER_SDO_GEOM_METADATA on close.
 *
 * Revision 1.6  2003/01/02 21:51:05  warmerda
 * fix quote escaping
 *
 * Revision 1.5  2002/12/29 19:43:59  warmerda
 * avoid some warnings
 *
 * Revision 1.4  2002/12/29 03:48:58  warmerda
 * fixed memory bug in CreateFeature()
 *
 * Revision 1.3  2002/12/28 20:06:31  warmerda
 * minor CreateFeature improvements
 *
 * Revision 1.2  2002/12/28 04:38:36  warmerda
 * converted to unix file conventions
 *
 * Revision 1.1  2002/12/28 04:07:27  warmerda
 * New
 *
 */

#include "ogr_oci.h"
#include "cpl_conv.h"
#include "cpl_string.h"

CPL_CVSID("$Id$");

static int nDiscarded = 0;
static int nHits = 0;

#define HSI_UNKNOWN  -2

/************************************************************************/
/*                          OGROCITableLayer()                          */
/************************************************************************/

OGROCITableLayer::OGROCITableLayer( OGROCIDataSource *poDSIn, 
                                    const char * pszTableName,
                                    int nSRIDIn, int bUpdate, int bNewLayerIn )

{
    poDS = poDSIn;

    pszQuery = NULL;
    pszWHERE = CPLStrdup( "" );
    pszQueryStatement = NULL;
    
    bUpdateAccess = bUpdate;
    bNewLayer = bNewLayerIn;

    iNextShapeId = 0;
    iNextFIDToWrite = 1;

    bValidTable = FALSE;
    if( bNewLayerIn )
        bHaveSpatialIndex = FALSE;
    else
        bHaveSpatialIndex = HSI_UNKNOWN;

    poFeatureDefn = ReadTableDefinition( pszTableName );

    nSRID = nSRIDIn;
    if( nSRID == -1 )
        nSRID = LookupTableSRID();
        
    poSRS = poDSIn->FetchSRS( nSRID );
    if( poSRS != NULL )
        poSRS->Reference();

    hOrdVARRAY = NULL;
    hElemInfoVARRAY = NULL;

    poBoundStatement = NULL;

    nWriteCacheMax = 0;
    nWriteCacheUsed = 0;
    pasWriteGeoms = NULL;
    papsWriteGeomMap = NULL;
    pasWriteGeomInd = NULL;
    papsWriteGeomIndMap = NULL;

    papWriteFields = NULL;
    papaeWriteFieldInd = NULL;

    panWriteFIDs = NULL;

    ResetReading();
}

/************************************************************************/
/*                         ~OGROCITableLayer()                          */
/************************************************************************/

OGROCITableLayer::~OGROCITableLayer()

{
    int   i;

    if( bNewLayer )
        FinalizeNewLayer();

    CPLFree( panWriteFIDs );
    if( papWriteFields != NULL )
    {
        for( i = 0; i < poFeatureDefn->GetFieldCount(); i++ )
        {
            CPLFree( papWriteFields[i] );
            CPLFree( papaeWriteFieldInd[i] );
        }
    }

    CPLFree( papWriteFields );
    CPLFree( papaeWriteFieldInd );

    if( poBoundStatement != NULL )
        delete poBoundStatement;

    CPLFree( pasWriteGeomInd );
    CPLFree( papsWriteGeomIndMap );
    
    CPLFree( papsWriteGeomMap );
    CPLFree( pasWriteGeoms );


    CPLFree( pszQuery );
    CPLFree( pszWHERE );

    if( poSRS != NULL && poSRS->Dereference() == 0 )
        delete poSRS;
}

/************************************************************************/
/*                        ReadTableDefinition()                         */
/*                                                                      */
/*      Build a schema from the named table.  Done by querying the      */
/*      catalog.                                                        */
/************************************************************************/

OGRFeatureDefn *OGROCITableLayer::ReadTableDefinition( const char * pszTable )

{
    OGROCISession      *poSession = poDS->GetSession();
    sword               nStatus;
    OGRFeatureDefn *poDefn = new OGRFeatureDefn( pszTable );

    poDefn->Reference();

/* -------------------------------------------------------------------- */
/*      Do a DescribeAll on the table.                                  */
/* -------------------------------------------------------------------- */
    OCIParam *hAttrParam = NULL;
    OCIParam *hAttrList = NULL;

    nStatus = 
        OCIDescribeAny( poSession->hSvcCtx, poSession->hError, 
                        (dvoid *) pszTable, strlen(pszTable), OCI_OTYPE_NAME, 
                        OCI_DEFAULT, OCI_PTYPE_TABLE, poSession->hDescribe );
    if( poSession->Failed( nStatus, "OCIDescribeAny" ) )
    {
        nStatus =
            OCIDescribeAny(poSession->hSvcCtx, poSession->hError,
                           (dvoid *)pszTable, strlen(pszTable), OCI_OTYPE_NAME,
                           OCI_DEFAULT, OCI_PTYPE_VIEW, poSession->hDescribe );
        if( poSession->Failed( nStatus, "OCIDescribeAny" ) )
            return poDefn;
    }
    
    if( poSession->Failed( 
        OCIAttrGet( poSession->hDescribe, OCI_HTYPE_DESCRIBE, 
                    &hAttrParam, 0, OCI_ATTR_PARAM, poSession->hError ),
        "OCIAttrGet(ATTR_PARAM)") )
        return poDefn;

    if( poSession->Failed( 
        OCIAttrGet( hAttrParam, OCI_DTYPE_PARAM, &hAttrList, 0, 
                    OCI_ATTR_LIST_COLUMNS, poSession->hError ),
        "OCIAttrGet(ATTR_LIST_COLUMNS)" ) )
        return poDefn;

/* -------------------------------------------------------------------- */
/*      What is the name of the column to use as FID?  This defaults    */
/*      to OGR_FID but we allow it to be overridden by a config         */
/*      variable.  Ideally we would identify a column that is a         */
/*      primary key and use that, but I'm not yet sure how to           */
/*      accomplish that.                                                */
/* -------------------------------------------------------------------- */
    const char *pszExpectedFIDName = 
        CPLGetConfigOption( "OCI_FID", "OGR_FID" );

/* -------------------------------------------------------------------- */
/*      Parse the returned table information.                           */
/* -------------------------------------------------------------------- */
    for( int iRawFld = 0; TRUE; iRawFld++ )
    {
        OGRFieldDefn    oField( "", OFTString);
        OCIParam     *hParmDesc;
        ub2          nOCIType;
        ub4          nOCILen;
        sword        nStatus;

        nStatus = OCIParamGet( hAttrList, OCI_DTYPE_PARAM,
                               poSession->hError, (dvoid**)&hParmDesc, 
                               (ub4) iRawFld+1 );
        if( nStatus != OCI_SUCCESS )
            break;

        if( poSession->GetParmInfo( hParmDesc, &oField, &nOCIType, &nOCILen )
            != CE_None )
            return poDefn;

        if( oField.GetType() == OFTBinary )
        {
            if( nOCIType == 108 && pszGeomName == NULL )
            {
                CPLFree( pszGeomName );
                pszGeomName = CPLStrdup( oField.GetNameRef() );
                iGeomColumn = iRawFld;
            }
            continue;                   
        }

        if( EQUAL(oField.GetNameRef(),pszExpectedFIDName) 
            && oField.GetType() == OFTInteger )
        {
            pszFIDName = CPLStrdup(oField.GetNameRef());
            continue;
        }

        poDefn->AddFieldDefn( &oField );
    }

    bValidTable = TRUE;

    return poDefn;
}

/************************************************************************/
/*                          SetSpatialFilter()                          */
/************************************************************************/

void OGROCITableLayer::SetSpatialFilter( OGRGeometry * poGeomIn )

{
    if( !InstallFilter( poGeomIn ) )
        return;

    BuildWhere();

    ResetReading();
}

/************************************************************************/
/*                        TestForSpatialIndex()                         */
/************************************************************************/

void OGROCITableLayer::TestForSpatialIndex( const char *pszSpatWHERE )

{
    OGROCIStringBuf oTestCmd;
    OGROCIStatement oTestStatement( poDS->GetSession() );
        
    oTestCmd.Append( "SELECT COUNT(*) FROM " );
    oTestCmd.Append( poFeatureDefn->GetName() );
    oTestCmd.Append( pszSpatWHERE );

    if( oTestStatement.Execute( oTestCmd.GetString() ) != CE_None )
        bHaveSpatialIndex = FALSE;
    else
        bHaveSpatialIndex = TRUE;
}

/************************************************************************/
/*                             BuildWhere()                             */
/*                                                                      */
/*      Build the WHERE statement appropriate to the current set of     */
/*      criteria (spatial and attribute queries).                       */
/************************************************************************/

void OGROCITableLayer::BuildWhere()

{
    OGROCIStringBuf oWHERE;

    CPLFree( pszWHERE );
    pszWHERE = NULL;

    if( m_poFilterGeom != NULL && bHaveSpatialIndex )
    {
        OGREnvelope  sEnvelope;

        m_poFilterGeom->getEnvelope( &sEnvelope );

        oWHERE.Append( " WHERE sdo_filter(" );
        oWHERE.Append( pszGeomName );
        oWHERE.Append( ", MDSYS.SDO_GEOMETRY(2003," );
        if( nSRID == -1 )
            oWHERE.Append( "NULL" );
        else
            oWHERE.Appendf( 15, "%d", nSRID );
        oWHERE.Append( ",NULL," );
        oWHERE.Append( "MDSYS.SDO_ELEM_INFO_ARRAY(1,1003,1)," );
        oWHERE.Append( "MDSYS.SDO_ORDINATE_ARRAY(" );
        oWHERE.Appendf( 600,
                "%.16g,%.16g,%.16g,%.16g,%.16g,%.16g,%.16g,%.16g,%.16g,%.16g",
                        sEnvelope.MinX, sEnvelope.MinY,
                        sEnvelope.MaxX, sEnvelope.MinY,
                        sEnvelope.MaxX, sEnvelope.MaxY,
                        sEnvelope.MinX, sEnvelope.MaxY,
                        sEnvelope.MinX, sEnvelope.MinY);
        oWHERE.Append( ")), 'querytype=window') = 'TRUE' " );
    }

    if( bHaveSpatialIndex == HSI_UNKNOWN )
    {
        TestForSpatialIndex( oWHERE.GetString() );
        if( !bHaveSpatialIndex )
            oWHERE.Clear();
    }

    if( pszQuery != NULL )
    {
        if( oWHERE.GetLast() == '\0' )
            oWHERE.Append( "WHERE " );
        else
            oWHERE.Append( "AND " );

        oWHERE.Append( pszQuery );
    }

    pszWHERE = oWHERE.StealString();
}

/************************************************************************/
/*                      BuildFullQueryStatement()                       */
/************************************************************************/

void OGROCITableLayer::BuildFullQueryStatement()

{
    if( pszQueryStatement != NULL )
    {
        CPLFree( pszQueryStatement );
        pszQueryStatement = NULL;
    }

    OGROCIStringBuf oCmd;
    char *pszFields = BuildFields();

    oCmd.Append( "SELECT " );
    oCmd.Append( pszFields );
    oCmd.Append( " FROM " );
    oCmd.Append( poFeatureDefn->GetName() );
    oCmd.Append( " " );
    oCmd.Append( pszWHERE );

    pszQueryStatement = oCmd.StealString();

    CPLFree( pszFields );
}

/************************************************************************/
/*                             GetFeature()                             */
/************************************************************************/

OGRFeature *OGROCITableLayer::GetFeature( long nFeatureId )

{

/* -------------------------------------------------------------------- */
/*      If we don't have an FID column scan for the desired feature.    */
/* -------------------------------------------------------------------- */
    if( pszFIDName == NULL )
        return OGROCILayer::GetFeature( nFeatureId );

/* -------------------------------------------------------------------- */
/*      Clear any existing query.                                       */
/* -------------------------------------------------------------------- */
    ResetReading();

/* -------------------------------------------------------------------- */
/*      Build query for this specific feature.                          */
/* -------------------------------------------------------------------- */
    OGROCIStringBuf oCmd;
    char *pszFields = BuildFields();

    oCmd.Append( "SELECT " );
    oCmd.Append( pszFields );
    oCmd.Append( " FROM " );
    oCmd.Append( poFeatureDefn->GetName() );
    oCmd.Append( " " );
    oCmd.Appendf( 50+strlen(pszFIDName), 
                  " WHERE \"%s\" = %ld ", 
                  pszFIDName, nFeatureId );

/* -------------------------------------------------------------------- */
/*      Execute the statement.                                          */
/* -------------------------------------------------------------------- */
    if( !ExecuteQuery( oCmd.GetString() ) )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Get the feature.                                                */
/* -------------------------------------------------------------------- */
    OGRFeature *poFeature;

    poFeature = GetNextRawFeature();
    
    if( poFeature != NULL && poFeature->GetGeometryRef() != NULL )
        poFeature->GetGeometryRef()->assignSpatialReference( poSRS );

/* -------------------------------------------------------------------- */
/*      Cleanup the statement.                                          */
/* -------------------------------------------------------------------- */
    ResetReading();

/* -------------------------------------------------------------------- */
/*      verify the FID.                                                 */
/* -------------------------------------------------------------------- */
    if( poFeature != NULL && poFeature->GetFID() != nFeatureId )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "OGROCITableLayer::GetFeature(%d) ... query returned feature %d instead!",
                  nFeatureId, poFeature->GetFID() );
        delete poFeature;
        return NULL;
    }
    else
        return poFeature;
}

/************************************************************************/
/*                           GetNextFeature()                           */
/*                                                                      */
/*      We override the next feature method because we know that we     */
/*      implement the attribute query within the statement and so we    */
/*      don't have to test here.   Eventually the spatial query will    */
/*      be fully tested within the statement as well.                   */
/************************************************************************/

OGRFeature *OGROCITableLayer::GetNextFeature()

{

    for( ; TRUE; )
    {
        OGRFeature      *poFeature;

        poFeature = GetNextRawFeature();
        if( poFeature == NULL )
        {
            CPLDebug( "OCI", "Query complete, got %d hits, and %d discards.",
                      nHits, nDiscarded );
            nHits = 0;
            nDiscarded = 0;
            return NULL;
        }

        if( m_poFilterGeom == NULL
            || FilterGeometry( poFeature->GetGeometryRef() ) )
        {
            nHits++;
            if( poFeature->GetGeometryRef() != NULL )
                poFeature->GetGeometryRef()->assignSpatialReference( poSRS );
            return poFeature;
        }

        if( m_poFilterGeom != NULL )
            nDiscarded++;

        delete poFeature;
    }
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGROCITableLayer::ResetReading()

{
    nHits = 0;
    nDiscarded = 0;

    FlushPendingFeatures();

    BuildFullQueryStatement();

    OGROCILayer::ResetReading();
}

/************************************************************************/
/*                            BuildFields()                             */
/*                                                                      */
/*      Build list of fields to fetch, performing any required          */
/*      transformations (such as on geometry).                          */
/************************************************************************/

char *OGROCITableLayer::BuildFields()

{
    int         i;
    OGROCIStringBuf oFldList;

    if( pszGeomName )                                                   
    {
        oFldList.Append( "\"" );
        oFldList.Append( pszGeomName );
        oFldList.Append( "\"" );
        iGeomColumn = 0;
    }

    for( i = 0; i < poFeatureDefn->GetFieldCount(); i++ )
    {
        const char *pszName = poFeatureDefn->GetFieldDefn(i)->GetNameRef();

        if( oFldList.GetLast() != '\0' )
            oFldList.Append( "," );

        oFldList.Append( "\"" );
        oFldList.Append( pszName );
        oFldList.Append( "\"" );
    }

    if( pszFIDName != NULL )
    {
        iFIDColumn = poFeatureDefn->GetFieldCount();
        oFldList.Append( ",\"" );
        oFldList.Append( pszFIDName );
        oFldList.Append( "\"" );
    }

    return oFldList.StealString();
}

/************************************************************************/
/*                         SetAttributeFilter()                         */
/************************************************************************/

OGRErr OGROCITableLayer::SetAttributeFilter( const char *pszQuery )

{
    if( (pszQuery == NULL && this->pszQuery == NULL)
        || (pszQuery != NULL && this->pszQuery != NULL
            && strcmp(pszQuery,this->pszQuery) == 0) )
        return OGRERR_NONE;
    
    CPLFree( this->pszQuery );

    if( pszQuery == NULL )
        this->pszQuery = NULL;
    else
        this->pszQuery = CPLStrdup( pszQuery );

    BuildWhere();

    ResetReading();

    return OGRERR_NONE;
}

/************************************************************************/
/*                             SetFeature()                             */
/*                                                                      */
/*      We implement SetFeature() by deleting the existing row (if      */
/*      it exists), and then using CreateFeature() to write it out      */
/*      tot he table normally.  CreateFeature() will preserve the       */
/*      existing FID if possible.                                       */
/************************************************************************/

OGRErr OGROCITableLayer::SetFeature( OGRFeature *poFeature )

{
/* -------------------------------------------------------------------- */
/*      Do some validation.                                             */
/* -------------------------------------------------------------------- */
    if( pszFIDName == NULL )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "OGROCITableLayer::SetFeature(%d) failed because there is "
                  "no apparent FID column on table %s.",
                  poFeature->GetFID(), 
                  poFeatureDefn->GetName() );

        return OGRERR_FAILURE;
    }

    if( poFeature->GetFID() == OGRNullFID )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "OGROCITableLayer::SetFeature(%d) failed because the feature "
                  "has no FID!", poFeature->GetFID() );

        return OGRERR_FAILURE;
    }

/* -------------------------------------------------------------------- */
/*      Prepare the delete command, and execute.  We don't check the    */
/*      error result of the execute, since attempting to Set a          */
/*      non-existing feature may be OK.                                 */
/* -------------------------------------------------------------------- */
    OGROCIStringBuf     oCmdText;
    OGROCIStatement     oCmdStatement( poDS->GetSession() );

    oCmdText.Appendf( strlen(poFeatureDefn->GetName())+strlen(pszFIDName)+100,
                      "DELETE FROM %s WHERE \"%s\" = %d",
                      poFeatureDefn->GetName(), 
                      pszFIDName, 
                      poFeature->GetFID() );

    oCmdStatement.Execute( oCmdText.GetString() );

    return CreateFeature( poFeature );
}

/************************************************************************/
/*                           DeleteFeature()                            */
/************************************************************************/

OGRErr OGROCITableLayer::DeleteFeature( long nFID )

{
/* -------------------------------------------------------------------- */
/*      Do some validation.                                             */
/* -------------------------------------------------------------------- */
    if( pszFIDName == NULL )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "OGROCITableLayer::DeleteFeature(%d) failed because there is "
                  "no apparent FID column on table %s.",
                  nFID, 
                  poFeatureDefn->GetName() );

        return OGRERR_FAILURE;
    }

    if( nFID == OGRNullFID )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "OGROCITableLayer::DeleteFeature(%d) failed for Null FID", 
                  nFID );

        return OGRERR_FAILURE;
    }

/* -------------------------------------------------------------------- */
/*      Prepare the delete command, and execute.  We don't check the    */
/*      error result of the execute, since attempting to Set a          */
/*      non-existing feature may be OK.                                 */
/* -------------------------------------------------------------------- */
    OGROCIStringBuf     oCmdText;
    OGROCIStatement     oCmdStatement( poDS->GetSession() );

    oCmdText.Appendf( strlen(poFeatureDefn->GetName())+strlen(pszFIDName)+100,
                      "DELETE FROM %s WHERE \"%s\" = %d",
                      poFeatureDefn->GetName(), 
                      pszFIDName, 
                      nFID );

    if( oCmdStatement.Execute( oCmdText.GetString() ) == CE_None )
        return OGRERR_NONE;
    else
        return OGRERR_FAILURE;
}

/************************************************************************/
/*                           CreateFeature()                            */
/************************************************************************/

OGRErr OGROCITableLayer::CreateFeature( OGRFeature *poFeature )

{
/* -------------------------------------------------------------------- */
/*      Add extents of this geometry to the existing layer extents.     */
/* -------------------------------------------------------------------- */
    if( poFeature->GetGeometryRef() != NULL )
    {
        OGREnvelope  sThisExtent;
        
        poFeature->GetGeometryRef()->getEnvelope( &sThisExtent );
        sExtent.Merge( sThisExtent );
    }

/* -------------------------------------------------------------------- */
/*      Do the actual creation.                                         */
/* -------------------------------------------------------------------- */
    if( CSLFetchBoolean( papszOptions, "MULTI_LOAD", bNewLayer ) )
        return BoundCreateFeature( poFeature );
    else
        return UnboundCreateFeature( poFeature );
}

/************************************************************************/
/*                        UnboundCreateFeature()                        */
/************************************************************************/

OGRErr OGROCITableLayer::UnboundCreateFeature( OGRFeature *poFeature )

{
    OGROCISession      *poSession = poDS->GetSession();
    char                *pszCommand;
    int                 i, bNeedComma = FALSE;
    unsigned int        nCommandBufSize;;

/* -------------------------------------------------------------------- */
/*      Prepare SQL statement buffer.                                   */
/* -------------------------------------------------------------------- */
    nCommandBufSize = 2000;
    pszCommand = (char *) CPLMalloc(nCommandBufSize);

/* -------------------------------------------------------------------- */
/*      Form the INSERT command.                                        */
/* -------------------------------------------------------------------- */
    sprintf( pszCommand, "INSERT INTO %s (", poFeatureDefn->GetName() );

    if( poFeature->GetGeometryRef() != NULL )
    {
        bNeedComma = TRUE;
        strcat( pszCommand, pszGeomName );
    }
    
    if( pszFIDName != NULL )
    {
        if( bNeedComma )
            strcat( pszCommand, ", " );
        
        strcat( pszCommand, pszFIDName );
        bNeedComma = TRUE;
    }
    

    for( i = 0; i < poFeatureDefn->GetFieldCount(); i++ )
    {
        if( !poFeature->IsFieldSet( i ) )
            continue;

        if( !bNeedComma )
            bNeedComma = TRUE;
        else
            strcat( pszCommand, ", " );

        sprintf( pszCommand + strlen(pszCommand), "\"%s\"",
                 poFeatureDefn->GetFieldDefn(i)->GetNameRef() );
    }

    strcat( pszCommand, ") VALUES (" );

    CPLAssert( strlen(pszCommand) < nCommandBufSize );

/* -------------------------------------------------------------------- */
/*      Set the geometry                                                */
/* -------------------------------------------------------------------- */
    bNeedComma = poFeature->GetGeometryRef() != NULL;
    if( poFeature->GetGeometryRef() != NULL)
    {
        OGRGeometry *poGeometry = poFeature->GetGeometryRef();
        char szSDO_GEOMETRY[512];
        char szSRID[128];

        if( nSRID == -1 )
            strcpy( szSRID, "NULL" );
        else
            sprintf( szSRID, "%d", nSRID );

        if( wkbFlatten(poGeometry->getGeometryType()) == wkbPoint )
        {
            OGRPoint *poPoint = (OGRPoint *) poGeometry;

            if( nDimension == 2 )
                sprintf( szSDO_GEOMETRY,
                         "%s(%d,%s,MDSYS.SDO_POINT_TYPE(%.16g,%.16g,0),NULL,NULL)",
                         SDO_GEOMETRY, 2001, szSRID, 
                         poPoint->getX(), poPoint->getY() );
            else
                sprintf( szSDO_GEOMETRY, 
                         "%s(%d,%s,MDSYS.SDO_POINT_TYPE(%.16g,%.16g,%.16g),NULL,NULL)",
                         SDO_GEOMETRY, 3001, szSRID, 
                         poPoint->getX(), poPoint->getY(), poPoint->getZ() );
        }
        else
        {
            int  nGType;

            if( TranslateToSDOGeometry( poFeature->GetGeometryRef(), &nGType )
                == OGRERR_NONE )
                sprintf( szSDO_GEOMETRY, 
                         "%s(%d,%s,NULL,:elem_info,:ordinates)", 
                         SDO_GEOMETRY, nGType, szSRID );
            else
                sprintf( szSDO_GEOMETRY, "NULL" );
        }

        if( strlen(pszCommand) + strlen(szSDO_GEOMETRY) 
            > nCommandBufSize - 50 )
        {
            nCommandBufSize = 
                strlen(pszCommand) + strlen(szSDO_GEOMETRY) + 10000;
            pszCommand = (char *) CPLRealloc(pszCommand, nCommandBufSize );
        }

        strcat( pszCommand, szSDO_GEOMETRY );
    }

/* -------------------------------------------------------------------- */
/*      Set the FID.                                                    */
/* -------------------------------------------------------------------- */
    int nOffset = strlen(pszCommand);

    if( pszFIDName != NULL )
    {
        long  nFID;

        if( bNeedComma )
            strcat( pszCommand+nOffset, ", " );
        bNeedComma = TRUE;

        nOffset += strlen(pszCommand+nOffset);

        nFID = poFeature->GetFID();
        if( nFID == -1 )
            nFID = iNextFIDToWrite++;

        sprintf( pszCommand+nOffset, "%ld", nFID );
    }

/* -------------------------------------------------------------------- */
/*      Set the other fields.                                           */
/* -------------------------------------------------------------------- */
    for( i = 0; i < poFeatureDefn->GetFieldCount(); i++ )
    {
        if( !poFeature->IsFieldSet( i ) )
            continue;

        OGRFieldDefn *poFldDefn = poFeatureDefn->GetFieldDefn(i);
        const char *pszStrValue = poFeature->GetFieldAsString(i);

        if( bNeedComma )
            strcat( pszCommand+nOffset, ", " );
        else
            bNeedComma = TRUE;

        if( strlen(pszStrValue) + strlen(pszCommand+nOffset) + nOffset 
            > nCommandBufSize-50 )
        {
            nCommandBufSize = strlen(pszCommand) + strlen(pszStrValue) + 10000;
            pszCommand = (char *) CPLRealloc(pszCommand, nCommandBufSize );
        }
        
        if( poFldDefn->GetType() == OFTInteger 
            || poFldDefn->GetType() == OFTReal )
        {
            if( poFldDefn->GetWidth() > 0 && bPreservePrecision
                && (int) strlen(pszStrValue) > poFldDefn->GetWidth() )
            {
                strcat( pszCommand+nOffset, "NULL" );
                ReportTruncation( poFldDefn );
            }
            else
                strcat( pszCommand+nOffset, pszStrValue );
        }
        else 
        {
            int         iChar;

            /* We need to quote and escape string fields. */
            strcat( pszCommand+nOffset, "'" );

            nOffset += strlen(pszCommand+nOffset);
            
            for( iChar = 0; pszStrValue[iChar] != '\0'; iChar++ )
            {
                if( poFldDefn->GetWidth() != 0 && bPreservePrecision
                    && iChar >= poFldDefn->GetWidth() )
                {
                    ReportTruncation( poFldDefn );
                    break;
                }

                if( pszStrValue[iChar] == '\'' )
                {
                    pszCommand[nOffset++] = '\'';
                    pszCommand[nOffset++] = pszStrValue[iChar];
                }
                else
                    pszCommand[nOffset++] = pszStrValue[iChar];
            }
            pszCommand[nOffset] = '\0';
            
            strcat( pszCommand+nOffset, "'" );
        }
        nOffset += strlen(pszCommand+nOffset);
    }

    strcat( pszCommand+nOffset, ")" );

/* -------------------------------------------------------------------- */
/*      Prepare statement.                                              */
/* -------------------------------------------------------------------- */
    OGROCIStatement oInsert( poSession );
    int  bHaveOrdinates = strstr(pszCommand,":ordinates") != NULL;
    int  bHaveElemInfo = strstr(pszCommand,":elem_info") != NULL;

    if( oInsert.Prepare( pszCommand ) != CE_None )
    {
        CPLFree( pszCommand );
        return OGRERR_FAILURE;
    }

    CPLFree( pszCommand );

/* -------------------------------------------------------------------- */
/*      Bind and translate the elem_info if we have some.               */
/* -------------------------------------------------------------------- */
    if( bHaveElemInfo )
    {
        OCIBind *hBindOrd = NULL;
        int i;
        OCINumber oci_number; 

        // Create or clear VARRAY 
        if( hElemInfoVARRAY == NULL )
        {
            if( poSession->Failed(
                OCIObjectNew( poSession->hEnv, poSession->hError, 
                              poSession->hSvcCtx, OCI_TYPECODE_VARRAY,
                              poSession->hElemInfoTDO, (dvoid *)NULL, 
                              OCI_DURATION_SESSION,
                              FALSE, (dvoid **)&hElemInfoVARRAY),
                "OCIObjectNew(hElemInfoVARRAY)") )
                return OGRERR_FAILURE;
        }
        else
        {
            sb4  nOldCount;

            OCICollSize( poSession->hEnv, poSession->hError, 
                         hElemInfoVARRAY, &nOldCount );
            OCICollTrim( poSession->hEnv, poSession->hError, 
                         nOldCount, hElemInfoVARRAY );
        }

        // Prepare the VARRAY of ordinate values. 
        for (i = 0; i < nElemInfoCount; i++)
        {
            if( poSession->Failed( 
                OCINumberFromInt( poSession->hError, 
                                  (dvoid *) (panElemInfo + i),
                                  (uword)sizeof(int),
                                  OCI_NUMBER_SIGNED,
                                  &oci_number),
                "OCINumberFromInt") )
                return OGRERR_FAILURE;

            if( poSession->Failed( 
                OCICollAppend( poSession->hEnv, poSession->hError,
                               (dvoid *) &oci_number,
                               (dvoid *)0, hElemInfoVARRAY),
                "OCICollAppend") )
                return OGRERR_FAILURE;
        }

        // Do the binding.
        if( poSession->Failed( 
            OCIBindByName( oInsert.GetStatement(), &hBindOrd, 
                           poSession->hError,
                           (text *) ":elem_info", (sb4) -1, (dvoid *) 0, 
                           (sb4) 0, SQLT_NTY, (dvoid *)0, (ub2 *)0, 
                           (ub2 *)0, (ub4)0, (ub4 *)0, 
                           (ub4)OCI_DEFAULT),
            "OCIBindByName(:elem_info)") )
            return OGRERR_FAILURE;

        if( poSession->Failed(
            OCIBindObject( hBindOrd, poSession->hError, 
                           poSession->hElemInfoTDO,
                           (dvoid **)&hElemInfoVARRAY, (ub4 *)0, 
                           (dvoid **)0, (ub4 *)0),
            "OCIBindObject(:elem_info)" ) )
            return OGRERR_FAILURE;
    }

/* -------------------------------------------------------------------- */
/*      Bind and translate the ordinates if we have some.               */
/* -------------------------------------------------------------------- */
    if( bHaveOrdinates )
    {
        OCIBind *hBindOrd = NULL;
        int i;
        OCINumber oci_number; 

        // Create or clear VARRAY 
        if( hOrdVARRAY == NULL )
        {
            if( poSession->Failed(
                OCIObjectNew( poSession->hEnv, poSession->hError, 
                              poSession->hSvcCtx, OCI_TYPECODE_VARRAY,
                              poSession->hOrdinatesTDO, (dvoid *)NULL, 
                              OCI_DURATION_SESSION,
                              FALSE, (dvoid **)&hOrdVARRAY),
                "OCIObjectNew(hOrdVARRAY)") )
                return OGRERR_FAILURE;
        }
        else
        {
            sb4  nOldCount;

            OCICollSize( poSession->hEnv, poSession->hError, 
                         hOrdVARRAY, &nOldCount );
            OCICollTrim( poSession->hEnv, poSession->hError, 
                         nOldCount, hOrdVARRAY );
        }

        // Prepare the VARRAY of ordinate values. 
        for (i = 0; i < nOrdinalCount; i++)
        {
            if( poSession->Failed( 
                OCINumberFromReal( poSession->hError, 
                                   (dvoid *) (padfOrdinals + i),
                                   (uword)sizeof(double),
                                   &oci_number),
                "OCINumberFromReal") )
                return OGRERR_FAILURE;

            if( poSession->Failed( 
                OCICollAppend( poSession->hEnv, poSession->hError,
                               (dvoid *) &oci_number,
                               (dvoid *)0, hOrdVARRAY),
                "OCICollAppend") )
                return OGRERR_FAILURE;
        }

        // Do the binding.
        if( poSession->Failed( 
            OCIBindByName( oInsert.GetStatement(), &hBindOrd, 
                           poSession->hError,
                           (text *) ":ordinates", (sb4) -1, (dvoid *) 0, 
                           (sb4) 0, SQLT_NTY, (dvoid *)0, (ub2 *)0, 
                           (ub2 *)0, (ub4)0, (ub4 *)0, 
                           (ub4)OCI_DEFAULT),
            "OCIBindByName(:ordinates)") )
            return OGRERR_FAILURE;

        if( poSession->Failed(
            OCIBindObject( hBindOrd, poSession->hError, 
                           poSession->hOrdinatesTDO,
                           (dvoid **)&hOrdVARRAY, (ub4 *)0, 
                           (dvoid **)0, (ub4 *)0),
            "OCIBindObject(:ordinates)" ) )
            return OGRERR_FAILURE;
    }

/* -------------------------------------------------------------------- */
/*      Execute the insert.                                             */
/* -------------------------------------------------------------------- */
    if( oInsert.Execute( NULL ) != CE_None )
        return OGRERR_FAILURE;
    else
        return OGRERR_NONE;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGROCITableLayer::TestCapability( const char * pszCap )

{
    if( EQUAL(pszCap,OLCSequentialWrite) 
             || EQUAL(pszCap,OLCRandomWrite) )
        return bUpdateAccess;

    else if( EQUAL(pszCap,OLCCreateField) )
        return bUpdateAccess;

    else 
        return OGROCILayer::TestCapability( pszCap );
}

/************************************************************************/
/*                          GetFeatureCount()                           */
/*                                                                      */
/*      If a spatial filter is in effect, we turn control over to       */
/*      the generic counter.  Otherwise we return the total count.      */
/*      Eventually we should consider implementing a more efficient     */
/*      way of counting features matching a spatial query.              */
/************************************************************************/

int OGROCITableLayer::GetFeatureCount( int bForce )

{
/* -------------------------------------------------------------------- */
/*      Use a more brute force mechanism if we have a spatial query     */
/*      in play.                                                        */
/* -------------------------------------------------------------------- */
    if( m_poFilterGeom != NULL )
        return OGROCILayer::GetFeatureCount( bForce );

/* -------------------------------------------------------------------- */
/*      In theory it might be wise to cache this result, but it         */
/*      won't be trivial to work out the lifetime of the value.         */
/*      After all someone else could be adding records from another     */
/*      application when working against a database.                    */
/* -------------------------------------------------------------------- */
    OGROCISession      *poSession = poDS->GetSession();
    OGROCIStatement    oGetCount( poSession );
    char               szCommand[1024];
    char               **papszResult;

    sprintf( szCommand, "SELECT COUNT(*) FROM %s %s", 
             poFeatureDefn->GetName(), pszWHERE );

    oGetCount.Execute( szCommand );

    papszResult = oGetCount.SimpleFetchRow();

    if( CSLCount(papszResult) < 1 )
    {
        CPLDebug( "OCI", "Fast get count failed, doing hard way." );
        return OGROCILayer::GetFeatureCount( bForce );
    }
    
    return atoi(papszResult[0]);
}

/************************************************************************/
/*                          FinalizeNewLayer()                          */
/*                                                                      */
/*      Our main job here is to update the USER_SDO_GEOM_METADATA       */
/*      table to include the correct array of dimension object with     */
/*      the appropriate extents for this layer.  We may also do         */
/*      spatial indexing at this point.                                 */
/************************************************************************/

void OGROCITableLayer::FinalizeNewLayer()

{
    OGROCIStringBuf  sDimUpdate;

    FlushPendingFeatures();

/* -------------------------------------------------------------------- */
/*      If the dimensions are degenerate (all zeros) then we assume     */
/*      there were no geometries, and we don't bother setting the       */
/*      dimensions.                                                     */
/* -------------------------------------------------------------------- */
    if( sExtent.MaxX == 0 && sExtent.MinX == 0
        && sExtent.MaxY == 0 && sExtent.MinY == 0 )
    {
        CPLError( CE_Warning, CPLE_AppDefined, 
                  "Layer %s appears to have no geometry ... not setting SDO DIMINFO metadata.", 
                  poFeatureDefn->GetName() );
        return;
                  
    }

/* -------------------------------------------------------------------- */
/*      Establish the extents and resolution to use.                    */
/* -------------------------------------------------------------------- */
    double           dfResSize;
    double           dfXMin, dfXMax, dfXRes;
    double           dfYMin, dfYMax, dfYRes;
    double           dfZMin, dfZMax, dfZRes;

    if( sExtent.MaxX - sExtent.MinX > 400 )
        dfResSize = 0.001;
    else
        dfResSize = 0.0000001;

    dfXMin = sExtent.MinX - dfResSize * 3;
    dfXMax = sExtent.MaxX + dfResSize * 3;
    dfXRes = dfResSize;
    ParseDIMINFO( "DIMINFO_X", &dfXMin, &dfXMax, &dfXRes );
    
    dfYMin = sExtent.MinY - dfResSize * 3;
    dfYMax = sExtent.MaxY + dfResSize * 3;
    dfYRes = dfResSize;
    ParseDIMINFO( "DIMINFO_Y", &dfYMin, &dfYMax, &dfYRes );
    
    dfZMin = -100000.0;
    dfZMax = 100000.0;
    dfZRes = 0.002;
    ParseDIMINFO( "DIMINFO_Z", &dfZMin, &dfZMax, &dfZRes );
    
/* -------------------------------------------------------------------- */
/*      Prepare dimension update statement.                             */
/* -------------------------------------------------------------------- */
    sDimUpdate.Append( "INSERT INTO USER_SDO_GEOM_METADATA VALUES " );
    sDimUpdate.Appendf( strlen(poFeatureDefn->GetName()) + 100,
                        "('%s', '%s', ",
                        poFeatureDefn->GetName(),
                        pszGeomName );

    sDimUpdate.Append( "MDSYS.SDO_DIM_ARRAY(" );
    sDimUpdate.Appendf(200,
                       "MDSYS.SDO_DIM_ELEMENT('X',%.16g,%.16g,%.12g)",
                       dfXMin, dfXMax, dfXRes );
    sDimUpdate.Appendf(200,
                       ",MDSYS.SDO_DIM_ELEMENT('Y',%.16g,%.16g,%.12g)",
                       dfYMin, dfYMax, dfYRes );

    if( nDimension == 3 )
    {
        sDimUpdate.Appendf(200,
                           ",MDSYS.SDO_DIM_ELEMENT('Z',%.16g,%.16g,%.12g)",
                           dfZMin, dfZMax, dfZRes );
    }

    if( nSRID == -1 )
        sDimUpdate.Append( "), NULL)" );
    else
        sDimUpdate.Appendf( 100, "), %d)", nSRID );

/* -------------------------------------------------------------------- */
/*      Execute the metadata update.                                    */
/* -------------------------------------------------------------------- */
    OGROCIStatement oExecStatement( poDS->GetSession() );

    if( oExecStatement.Execute( sDimUpdate.GetString() ) != CE_None )
        return;

/* -------------------------------------------------------------------- */
/*      If the user has disabled INDEX support then don't create the    */
/*      index.                                                          */
/* -------------------------------------------------------------------- */
    if( !CSLFetchBoolean( papszOptions, "INDEX", TRUE ) )
        return;

/* -------------------------------------------------------------------- */
/*      Establish an index name.  For some reason Oracle 8.1.7 does     */
/*      not support spatial index names longer than 18 characters so    */
/*      we magic up an index name if it would be too long.              */
/* -------------------------------------------------------------------- */
    char  szIndexName[20];

    if( strlen(poFeatureDefn->GetName()) < 15 )
        sprintf( szIndexName, "%s_idx", poFeatureDefn->GetName() );
    else if( strlen(poFeatureDefn->GetName()) < 17 )
        sprintf( szIndexName, "%si", poFeatureDefn->GetName() );
    else
    {
        int i, nHash = 0;
        const char *pszSrcName = poFeatureDefn->GetName();

        for( i = 0; pszSrcName[i] != '\0'; i++ )
            nHash = (nHash + i * pszSrcName[i]) % 987651;
        
        sprintf( szIndexName, "OSI_%d", nHash );
    }

    poDS->GetSession()->CleanName( szIndexName );

/* -------------------------------------------------------------------- */
/*      Try creating an index on the table now.  Use a simple 5         */
/*      level quadtree based index.  Would R-tree be a better default?  */
/* -------------------------------------------------------------------- */

// Disable for now, spatial index creation always seems to cause me to 
// lose my connection to the database!
    OGROCIStringBuf  sIndexCmd;

    sIndexCmd.Appendf( 10000, "CREATE INDEX \"%s\" ON %s(\"%s\") "
                       "INDEXTYPE IS MDSYS.SPATIAL_INDEX ",
                       szIndexName, 
                       poFeatureDefn->GetName(), 
                       pszGeomName );

    if( CSLFetchNameValue( papszOptions, "INDEX_PARAMETERS" ) != NULL )
    {
        sIndexCmd.Append( " PARAMETERS( '" );
        sIndexCmd.Append( CSLFetchNameValue(papszOptions,"INDEX_PARAMETERS") );
        sIndexCmd.Append( "' )" );
    }

    if( oExecStatement.Execute( sIndexCmd.GetString() ) != CE_None )
    {
        char szDropCommand[2000];
        sprintf( szDropCommand, "DROP INDEX \"%s\"", szIndexName );
        oExecStatement.Execute( szDropCommand );
    }
}

/************************************************************************/
/*                        AllocAndBindForWrite()                        */
/************************************************************************/

int OGROCITableLayer::AllocAndBindForWrite()

{
    OGROCISession      *poSession = poDS->GetSession();
    int i;

    CPLAssert( nWriteCacheMax == 0 );

/* -------------------------------------------------------------------- */
/*      Decide on the number of rows we want to be able to cache at     */
/*      a time.                                                         */
/* -------------------------------------------------------------------- */
    nWriteCacheMax = 100;

/* -------------------------------------------------------------------- */
/*      Collect the INSERT statement.                                   */
/* -------------------------------------------------------------------- */
    OGROCIStringBuf oCmdBuf;
    
    oCmdBuf.Append( "INSERT INTO " );
    oCmdBuf.Append( poFeatureDefn->GetName() );

    for( i = 0; i < poFeatureDefn->GetFieldCount(); i++ )
    {
        if( i == 0 )
            oCmdBuf.Append( " VALUES ( :fid, :geometry, " );
        else
            oCmdBuf.Append( ", " );

        oCmdBuf.Appendf( 20, " :field_%d", i );
    }
    
    oCmdBuf.Append( ") " );

/* -------------------------------------------------------------------- */
/*      Bind and Prepare it.                                            */
/* -------------------------------------------------------------------- */
    poBoundStatement = new OGROCIStatement( poSession );
    poBoundStatement->Prepare( oCmdBuf.GetString() );

/* -------------------------------------------------------------------- */
/*      Setup geometry indicator information.                           */
/* -------------------------------------------------------------------- */
    pasWriteGeomInd = (SDO_GEOMETRY_ind *) 
        CPLCalloc(sizeof(SDO_GEOMETRY_ind),nWriteCacheMax);
    
    papsWriteGeomIndMap = (SDO_GEOMETRY_ind **)
        CPLCalloc(sizeof(SDO_GEOMETRY_ind *),nWriteCacheMax);

    for( i = 0; i < nWriteCacheMax; i++ )
        papsWriteGeomIndMap[i] = pasWriteGeomInd + i;

/* -------------------------------------------------------------------- */
/*      Setup all the required geometry objects, and the                */
/*      corresponding indicator map.                                    */
/* -------------------------------------------------------------------- */
    pasWriteGeoms = (SDO_GEOMETRY_TYPE *) 
        CPLCalloc( sizeof(SDO_GEOMETRY_TYPE), nWriteCacheMax);
    papsWriteGeomMap = (SDO_GEOMETRY_TYPE **)
        CPLCalloc( sizeof(SDO_GEOMETRY_TYPE *), nWriteCacheMax );

    for( i = 0; i < nWriteCacheMax; i++ )
        papsWriteGeomMap[i] = pasWriteGeoms + i;

/* -------------------------------------------------------------------- */
/*      Allocate VARRAYs for the elem_info and ordinates.               */
/* -------------------------------------------------------------------- */
    for( i = 0; i < nWriteCacheMax; i++ )
    {
        if( poSession->Failed(
                OCIObjectNew( poSession->hEnv, poSession->hError, 
                              poSession->hSvcCtx, OCI_TYPECODE_VARRAY,
                              poSession->hElemInfoTDO, (dvoid *)NULL, 
                              OCI_DURATION_SESSION,
                              FALSE, 
                              (dvoid **) &(pasWriteGeoms[i].sdo_elem_info)),
                "OCIObjectNew(elem_info)") )
            return FALSE;

        if( poSession->Failed(
                OCIObjectNew( poSession->hEnv, poSession->hError, 
                              poSession->hSvcCtx, OCI_TYPECODE_VARRAY,
                              poSession->hOrdinatesTDO, (dvoid *)NULL, 
                              OCI_DURATION_SESSION,
                              FALSE, 
                              (dvoid **) &(pasWriteGeoms[i].sdo_ordinates)),
                "OCIObjectNew(ordinates)") )
            return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Bind the geometry column.                                       */
/* -------------------------------------------------------------------- */
    if( poBoundStatement->BindObject( 
            ":geometry", papsWriteGeomMap, poSession->hGeometryTDO, 
            (void**) papsWriteGeomIndMap) != CE_None )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Bind the FID column.                                            */
/* -------------------------------------------------------------------- */
    panWriteFIDs = (int *) CPLMalloc(sizeof(int) * nWriteCacheMax );
        
    if( poBoundStatement->BindScalar( ":fid", panWriteFIDs, sizeof(int), 
                                      SQLT_INT ) != CE_None )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Allocate each of the column data bind arrays.                   */
/* -------------------------------------------------------------------- */
    
    papWriteFields = (void **) 
        CPLMalloc(sizeof(void*) * poFeatureDefn->GetFieldCount() );
    papaeWriteFieldInd = (OCIInd **) 
        CPLCalloc(sizeof(OCIInd*),poFeatureDefn->GetFieldCount() );

    for( i = 0; i < poFeatureDefn->GetFieldCount(); i++ )
    {
        OGRFieldDefn *poFldDefn = poFeatureDefn->GetFieldDefn(i);
        char szFieldPlaceholderName[80];

        sprintf( szFieldPlaceholderName, ":field_%d", i );

        papaeWriteFieldInd[i] = (OCIInd *) 
            CPLCalloc(sizeof(OCIInd), nWriteCacheMax );

        if( poFldDefn->GetType() == OFTInteger )
        {
            papWriteFields[i] = 
                (void *) CPLCalloc( sizeof(int), nWriteCacheMax );

            if( poBoundStatement->BindScalar( 
                    szFieldPlaceholderName, papWriteFields[i],
                    sizeof(int), SQLT_INT, papaeWriteFieldInd[i] ) != CE_None )
                return FALSE;
        }
        else if( poFldDefn->GetType() == OFTReal )
        {
            papWriteFields[i] = (void *) CPLCalloc( sizeof(double), 
                                                    nWriteCacheMax );

            if( poBoundStatement->BindScalar( 
                    szFieldPlaceholderName, papWriteFields[i],
                    sizeof(double), SQLT_FLT, papaeWriteFieldInd[i] ) != CE_None )
                return FALSE;
        }
        else 
        {
            int nEachBufSize = 4001;

            if( poFldDefn->GetType() == OFTString
                && poFldDefn->GetWidth() != 0 )
                nEachBufSize = poFldDefn->GetWidth() + 1;

            papWriteFields[i] = 
                (void *) CPLCalloc( nEachBufSize, nWriteCacheMax );

            if( poBoundStatement->BindScalar( 
                    szFieldPlaceholderName, papWriteFields[i],
                    nEachBufSize, SQLT_STR, papaeWriteFieldInd[i]) != CE_None )
                return FALSE;
        }
    }

    return TRUE;
}

/************************************************************************/
/*                         BoundCreateFeature()                         */
/************************************************************************/

OGRErr OGROCITableLayer::BoundCreateFeature( OGRFeature *poFeature )

{
    OGROCISession      *poSession = poDS->GetSession();
    int                iCache, i;
    OGRErr             eErr;
    OCINumber          oci_number; 

    iCache = nWriteCacheUsed;

    if( nWriteCacheMax == 0 )
    {
        if( !AllocAndBindForWrite() )
            return OGRERR_FAILURE;
    }

/* -------------------------------------------------------------------- */
/*      Set the geometry                                                */
/* -------------------------------------------------------------------- */
    if( poFeature->GetGeometryRef() == NULL )
    {
        pasWriteGeomInd[iCache]._atomic = OCI_IND_NULL;
    }
    else
    {
        SDO_GEOMETRY_TYPE *psGeom = pasWriteGeoms + iCache;
        SDO_GEOMETRY_ind  *psInd  = pasWriteGeomInd + iCache;
        OGRGeometry *poGeometry = poFeature->GetGeometryRef();
        int nGType;

        psInd->_atomic = OCI_IND_NOTNULL;

        if( nSRID == -1 )
            psInd->sdo_srid = OCI_IND_NULL;
        else
        {
            psInd->sdo_srid = OCI_IND_NOTNULL;
            OCINumberFromInt( poSession->hError, &nSRID, 
                              (uword)sizeof(int), OCI_NUMBER_SIGNED,
                              &(psGeom->sdo_srid) );
        }

        /* special more efficient case for simple points */
        if( wkbFlatten(poGeometry->getGeometryType()) == wkbPoint )
        {
            OGRPoint *poPoint = (OGRPoint *) poGeometry;
            double dfValue;

            psInd->sdo_point._atomic = OCI_IND_NOTNULL;
            psInd->sdo_elem_info = OCI_IND_NULL;
            psInd->sdo_ordinates = OCI_IND_NULL;
            
            dfValue = poPoint->getX();
            OCINumberFromReal( poSession->hError, &dfValue, 
                               (uword)sizeof(double),
                               &(psGeom->sdo_point.x) );

            dfValue = poPoint->getY();
            OCINumberFromReal( poSession->hError, &dfValue, 
                               (uword)sizeof(double),
                               &(psGeom->sdo_point.y) );

            if( nDimension == 2 )
            {
                nGType = 2001;
                psInd->sdo_point.z = OCI_IND_NULL;
            }
            else
            {
                nGType = 3001;
                psInd->sdo_point.z = OCI_IND_NOTNULL;

                dfValue = poPoint->getZ();
                OCINumberFromReal( poSession->hError, &dfValue, 
                                   (uword)sizeof(double),
                                   &(psGeom->sdo_point.z) );
            }
        }
        else
        {
            psInd->sdo_point._atomic = OCI_IND_NULL;
            psInd->sdo_elem_info = OCI_IND_NOTNULL;
            psInd->sdo_ordinates = OCI_IND_NOTNULL;

            eErr = TranslateToSDOGeometry( poFeature->GetGeometryRef(), 
                                           &nGType );
            
            if( eErr != OGRERR_NONE )
                return eErr;

            /* Clear the existing eleminfo and ordinates arrays */
            sb4  nOldCount;

            OCICollSize( poSession->hEnv, poSession->hError, 
                         psGeom->sdo_elem_info, &nOldCount );
            OCICollTrim( poSession->hEnv, poSession->hError, 
                         nOldCount, psGeom->sdo_elem_info );

            OCICollSize( poSession->hEnv, poSession->hError, 
                         psGeom->sdo_ordinates, &nOldCount );
            OCICollTrim( poSession->hEnv, poSession->hError, 
                         nOldCount, psGeom->sdo_ordinates );

            // Prepare the VARRAY of element values. 
            for (i = 0; i < nElemInfoCount; i++)
            {
                OCINumberFromInt( poSession->hError, 
                                  (dvoid *) (panElemInfo + i),
                                  (uword)sizeof(int), OCI_NUMBER_SIGNED,
                                  &oci_number );

                OCICollAppend( poSession->hEnv, poSession->hError,
                               (dvoid *) &oci_number,
                               (dvoid *)0, psGeom->sdo_elem_info );
            }

            // Prepare the VARRAY of ordinate values. 
            for (i = 0; i < nOrdinalCount; i++)
            {
                OCINumberFromReal( poSession->hError, 
                                   (dvoid *) (padfOrdinals + i),
                                   (uword)sizeof(double), &oci_number );
                OCICollAppend( poSession->hEnv, poSession->hError,
                               (dvoid *) &oci_number,
                               (dvoid *)0, psGeom->sdo_ordinates );
            }
        }

        OCINumberFromInt( poSession->hError, &nGType,
                          (uword)sizeof(int), OCI_NUMBER_SIGNED,
                          &(psGeom->sdo_gtype) );
    }

/* -------------------------------------------------------------------- */
/*      Set the FID.                                                    */
/* -------------------------------------------------------------------- */
    if( poFeature->GetFID() == OGRNullFID )
        poFeature->SetFID( iNextFIDToWrite++ );

    panWriteFIDs[iCache] = poFeature->GetFID();

/* -------------------------------------------------------------------- */
/*      Set the other fields.                                           */
/* -------------------------------------------------------------------- */
    for( i = 0; i < poFeatureDefn->GetFieldCount(); i++ )
    {
        if( !poFeature->IsFieldSet( i ) )
        {
            papaeWriteFieldInd[i][iCache] = OCI_IND_NULL;
            continue;
        }

        papaeWriteFieldInd[i][iCache] = OCI_IND_NOTNULL;

        OGRFieldDefn *poFldDefn = poFeatureDefn->GetFieldDefn(i);

        if( poFldDefn->GetType() == OFTInteger )
            ((int *) (papWriteFields[i]))[iCache] = 
                poFeature->GetFieldAsInteger( i );

        else if( poFldDefn->GetType() == OFTReal )
            ((double *) (papWriteFields[i]))[iCache] = 
                poFeature->GetFieldAsDouble( i );

        else
        {
            int nEachBufSize = 4001, nLen;
            const char *pszStrValue = poFeature->GetFieldAsString(i);

            if( poFldDefn->GetType() == OFTString
                && poFldDefn->GetWidth() != 0 )
                nEachBufSize = poFldDefn->GetWidth() + 1;

            nLen = strlen(pszStrValue);
            if( nLen > nEachBufSize-1 )
                nLen = nEachBufSize-1;

            char *pszTarget = ((char*)papWriteFields[i]) + iCache*nEachBufSize;
            strncpy( pszTarget, pszStrValue, nLen );
            pszTarget[nLen] = '\0';
        }
    }

/* -------------------------------------------------------------------- */
/*      Do we need to flush out a full set of rows?                     */
/* -------------------------------------------------------------------- */
    nWriteCacheUsed++;

    if( nWriteCacheUsed == nWriteCacheMax )
        return FlushPendingFeatures();
    else
        return OGRERR_NONE;
}

/************************************************************************/
/*                        FlushPendingFeatures()                        */
/************************************************************************/

OGRErr OGROCITableLayer::FlushPendingFeatures()

{
    OGROCISession      *poSession = poDS->GetSession();

    if( nWriteCacheUsed > 0 )
    {
        CPLDebug( "OCI", "Flushing %d features on layer %s", 
                  nWriteCacheUsed, poFeatureDefn->GetName() );

        if( poSession->Failed( 
                OCIStmtExecute( poSession->hSvcCtx, 
                                poBoundStatement->GetStatement(), 
                                poSession->hError, (ub4) nWriteCacheUsed, 
                                (ub4) 0, 
                                (OCISnapshot *)NULL, (OCISnapshot *)NULL, 
                                (ub4) OCI_COMMIT_ON_SUCCESS ),
                "OCIStmtExecute" ) )
        {
            nWriteCacheUsed = 0;
            return OGRERR_FAILURE;
        }
        else
        {
            nWriteCacheUsed = 0;
            return OGRERR_NONE;
        }
    }
    else
        return OGRERR_NONE;
}

/************************************************************************/
/*                             SyncToDisk()                             */
/*                                                                      */
/*      Perhaps we should also be putting the metadata into a           */
/*      useable state?                                                  */
/************************************************************************/

OGRErr OGROCITableLayer::SyncToDisk()

{
    return FlushPendingFeatures();
}

