//---------------------------------------------------------------------------

#ifndef mosaicH
#define mosaicH

//---------------------------------------------------------------------------
enum tTVPMosaicMode
{
    tmmMosaic,
    tmmMosaicIn,
    tmmMosaicOut
};
void RegisterMosaicTransHandlerProvider();
void UnregisterMosaicTransHandlerProvider();
void RegisterMosaicInOutTransHandlerProviders();
void UnregisterMosaicInOutTransHandlerProviders();
//---------------------------------------------------------------------------
#endif
