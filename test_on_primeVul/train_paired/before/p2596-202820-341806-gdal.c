OGRKMLLayer::OGRKMLLayer( const char * pszName,
                          OGRSpatialReference *poSRSIn, bool bWriterIn,
                          OGRwkbGeometryType eReqType,
                          OGRKMLDataSource *poDSIn ) :
    poDS_(poDSIn),
    poSRS_(poSRSIn ? new OGRSpatialReference(nullptr) : nullptr),
    poCT_(nullptr),
    poFeatureDefn_(new OGRFeatureDefn( pszName )),
    iNextKMLId_(0),
    nTotalKMLCount_(-1),
    bWriter_(bWriterIn),
    nLayerNumber_(0),
    nWroteFeatureCount_(0),
    bSchemaWritten_(false),
    pszName_(CPLStrdup(pszName)),
    nLastAsked(-1),
    nLastCount(-1)
{
    // KML should be created as WGS84.
    if( poSRSIn != nullptr )
    {
        poSRS_->SetWellKnownGeogCS( "WGS84" );
        if( !poSRS_->IsSame(poSRSIn) )
        {
            poCT_ = OGRCreateCoordinateTransformation( poSRSIn, poSRS_ );
            if( poCT_ == nullptr && poDSIn->IsFirstCTError() )
            {
                // If we can't create a transformation, issue a warning - but
                // continue the transformation.
                char *pszWKT = nullptr;

                poSRSIn->exportToPrettyWkt( &pszWKT, FALSE );

                CPLError(
                    CE_Warning, CPLE_AppDefined,
                    "Failed to create coordinate transformation between the "
                    "input coordinate system and WGS84.  This may be because "
                    "they are not transformable.  "
                    "KML geometries may not render correctly.  "
                    "This message will not be issued any more."
                    "\nSource:\n%s\n",
                    pszWKT );

                CPLFree( pszWKT );
                poDSIn->IssuedFirstCTError();
            }
        }
    }

    SetDescription( poFeatureDefn_->GetName() );
    poFeatureDefn_->Reference();
    poFeatureDefn_->SetGeomType( eReqType );
    if( poFeatureDefn_->GetGeomFieldCount() != 0 )
        poFeatureDefn_->GetGeomFieldDefn(0)->SetSpatialRef(poSRS_);

    OGRFieldDefn oFieldName( "Name", OFTString );
    poFeatureDefn_->AddFieldDefn( &oFieldName );

    OGRFieldDefn oFieldDesc( "Description", OFTString );
    poFeatureDefn_->AddFieldDefn( &oFieldDesc );

    bClosedForWriting = !bWriterIn;
}