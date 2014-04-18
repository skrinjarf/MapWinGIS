#include "stdafx.h"
#include "map.h"
#include "Tiles.h"

#pragma region REGION SnapShots

// *********************************************************
//		SnapShot()
// *********************************************************
LPDISPATCH CMapView::SnapShot(LPDISPATCH BoundBox)
{
	if( BoundBox == NULL )
	{	
		ErrorMessage(tkUNEXPECTED_NULL_PARAMETER);
		return NULL;
	}

	IExtents * box = NULL;
	BoundBox->QueryInterface(IID_IExtents,(void**)&box);

	if( box == NULL )
	{	
		ErrorMessage(tkINTERFACE_NOT_SUPPORTED);
		return NULL;
	}

	double left, right, bottom, top, nv;
	box->GetBounds(&left,&bottom,&nv,&right,&top,&nv);
	box->Release();
	box = NULL;
	
	return SnapShotCore(left, right, bottom, top, m_viewWidth, m_viewHeight);
}

// *********************************************************
//		SnapShot2()
// *********************************************************
// use the indicated layer and zoom/width to determine the output size and clipping
IDispatch* CMapView::SnapShot2(LONG ClippingLayerNbr, DOUBLE Zoom, long pWidth)
{   
	AFX_MANAGE_STATE(AfxGetStaticModuleState());

	long Width, Height;
	double left, right, bottom, top;

	Layer * l = m_allLayers[ClippingLayerNbr];
	if( !IS_VALID_PTR(l) )
	{
		if( m_globalCallback != NULL )
			m_globalCallback->Error(m_key.AllocSysString(),A2BSTR("Cannot clip to selected layer"));
		return NULL;
	}
	else
	{	
		this->AdjustLayerExtents(ClippingLayerNbr);
		left = l->extents.left;
		right = l->extents.right;
		top = l->extents.top;
		bottom = l->extents.bottom;

		if( l->type == ShapefileLayer )
		{
			double ar = (right-left)/(top-bottom);
			Width = (long) (pWidth == 0 ? ((right - left) * Zoom) : pWidth);
			Height = (long)((double)pWidth / ar);
		}
		else if(l->type == ImageLayer)
		{
			Width = (long)(right - left);
			Height = (long)(top - bottom);
			if (Zoom > 0)
			{
				Width *= (long)Zoom;
				Height *= (long)Zoom;
			}
		}
		else
		{
			if( m_globalCallback != NULL )
				m_globalCallback->Error(m_key.AllocSysString(),A2BSTR("Cannot clip to selected layer type"));
			return NULL;
		}
	}

	if (Width <= 0 || Height <= 0)
	{
		if( m_globalCallback != NULL )
			m_globalCallback->Error(m_key.AllocSysString(),A2BSTR("Invalid Width and/or Zoom"));
		return NULL;
	}

	return this->SnapShotCore(left, right, top, bottom, Width, Height);
}

// *********************************************************
//		SnapShot3()
// *********************************************************
//A new snapshot method which works a bit better specifically for the printing engine
//1. Draw to a back buffer, 2. Populate an Image object
LPDISPATCH CMapView::SnapShot3(double left, double right, double top, double bottom, long Width)
{   
	AFX_MANAGE_STATE(AfxGetStaticModuleState());

	long Height = (long)((double)Width / ((right-left)/(top-bottom)));
	if (Width <= 0 || Height <= 0)
	{
		if( m_globalCallback != NULL )
			m_globalCallback->Error(m_key.AllocSysString(),A2BSTR("Invalid Width and/or Zoom"));
		return NULL;
	}

	return this->SnapShotCore(left, right, top, bottom, Width, Height);
}

// *********************************************************************
//    TilesAreInCache()
// *********************************************************************
INT CMapView::TilesAreInCache(IExtents* Extents, LONG WidthPixels, tkTileProvider provider)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());
	USES_CONVERSION;
	
	if (Extents)
	{
		// Get the image height based on the box aspect ratio
		double xMin, xMax, yMin, yMax, zMin, zMax;
		Extents->GetBounds(&xMin, &yMin, &zMin, &xMax, &yMax, &zMax);
		
		// Make sure that the width and height are valid
		long Height = static_cast<long>((double)WidthPixels *(yMax - yMin) / (xMax - xMin));
		if (WidthPixels <= 0 || Height <= 0)
		{
			if( m_globalCallback != NULL )
				m_globalCallback->Error(m_key.AllocSysString(), A2BSTR("Invalid Width and/or Zoom"));
		}
		else
		{
			SetTempExtents(xMin, xMax, yMin, yMax, WidthPixels, Height);
			bool tilesInCache =((CTiles*)m_tiles)->TilesAreInCache((void*)this, provider);
			RestoreExtents();
			return tilesInCache ? 1 : 0;
		}
	}
	return -1;	// error
}

// *********************************************************************
//    LoadTiles()
// *********************************************************************
// Loads tiles for specified extents
void CMapView::LoadTilesForSnapshot(IExtents* Extents, LONG WidthPixels, LPCTSTR Key, tkTileProvider provider)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());
	USES_CONVERSION;
	
	if (Extents)
	{
		// Get the image height based on the box aspect ratio
		double xMin, xMax, yMin, yMax, zMin, zMax;
		Extents->GetBounds(&xMin, &yMin, &zMin, &xMax, &yMax, &zMax);
		
		// Make sure that the width and height are valid
		long Height = static_cast<long>((double)WidthPixels *(yMax - yMin) / (xMax - xMin));
		if (WidthPixels <= 0 || Height <= 0)
		{
			if( m_globalCallback != NULL )
				m_globalCallback->Error(m_key.AllocSysString(), A2BSTR("Invalid Width and/or Zoom"));
		}
		else
		{
			CString key = (char*)Key;
			SetTempExtents(xMin, xMax, yMin, yMax, WidthPixels, Height);
			bool tilesInCache =((CTiles*)m_tiles)->TilesAreInCache((void*)this, provider);
			if (!tilesInCache)
			{
				((CTiles*)m_tiles)->LoadTiles((void*)this, true, (int)provider, key);
				RestoreExtents();
			}
			else
			{
				// they are already here, no loading is needed
				RestoreExtents();
				FireTilesLoaded(m_tiles, NULL, true, key);
			}
		}
	}
}

// *********************************************************************
//    SnapShotToDC2()
// *********************************************************************
BOOL CMapView::SnapShotToDC2(PVOID hdc, IExtents* Extents, LONG Width, float OffsetX, float OffsetY,
							 float ClipX, float ClipY, float clipWidth, float clipHeight)
{
	if(!Extents || !hdc) 
	{
		ErrorMessage(tkUNEXPECTED_NULL_PARAMETER);
		return FALSE;
	}
	// getting DC to draw
	HDC dc = reinterpret_cast<HDC>(hdc);
	CDC * tempDC = CDC::FromHandle(dc);

	// Get the image height based on the box aspect ration
	double xMin, xMax, yMin, yMax, zMin, zMax;
	Extents->GetBounds(&xMin, &yMin, &zMin, &xMax, &yMax, &zMax);
	
	// Make sure that the width and height are valid
	long Height = static_cast<long>((double)Width *(yMax - yMin) / (xMax - xMin));
	if (Width <= 0 || Height <= 0)
	{
		if( m_globalCallback != NULL )
			m_globalCallback->Error(m_key.AllocSysString(), A2BSTR("Invalid Width and/or Zoom"));
		return FALSE;
	}
	
	SnapShotCore(xMin, xMax, yMin, yMax, Width, Height, tempDC, OffsetX, OffsetY, ClipX, ClipY, clipWidth, clipHeight);
	return TRUE;
}

// *********************************************************************
//    SnapShotToDC()
// *********************************************************************
// Draws the specified extents of map at given DC.
BOOL CMapView::SnapShotToDC(PVOID hdc, IExtents* Extents, LONG Width)
{
	return this->SnapShotToDC2(hdc, Extents, Width, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
}

// *********************************************************************
//    SetTempExtents()
// *********************************************************************
void CMapView::SetTempExtents(double left, double right, double top, double bottom, long Width, long Height)
{
	mm_viewWidth = m_viewWidth;
	mm_viewHeight = m_viewHeight;
	mm_pixelPerProjectionX = m_pixelPerProjectionX;
	mm_pixelPerProjectionY = m_pixelPerProjectionY;
	mm_inversePixelPerProjectionX = m_inversePixelPerProjectionX;
	mm_inversePixelPerProjectionY = m_inversePixelPerProjectionY;
	mm_aspectRatio = m_aspectRatio;
	mm_left = extents.left;
	mm_right = extents.right;
	mm_bottom = extents.bottom;
	mm_top = extents.top;

	mm_newExtents = (Width != m_viewWidth || Height != m_viewWidth ||
						left != extents.left || right !=  extents.right ||
						top != extents.top || bottom != extents.bottom);

	if (mm_newExtents)
	{
		m_viewWidth=Width;
		m_viewHeight=Height;
		//ResizeBuffers(m_viewWidth, m_viewHeight);
		m_aspectRatio = (double)Width / (double)Height; 

		double xrange = right - left;
		double yrange = top - bottom;
		m_pixelPerProjectionX = m_viewWidth/xrange;
		m_inversePixelPerProjectionX = 1.0/m_pixelPerProjectionX;
		m_pixelPerProjectionY = m_viewHeight/yrange;
		m_inversePixelPerProjectionY = 1.0/m_pixelPerProjectionY;
		
		extents.left = left;
		extents.right = right - m_inversePixelPerProjectionX;
		extents.bottom = bottom;
		extents.top = top - m_inversePixelPerProjectionY;

		CalculateVisibleExtents(Extent(left,right,bottom,top));
	}
}

// *********************************************************************
//    RestoreExtents()
// *********************************************************************
void CMapView::RestoreExtents()
{
	if (mm_newExtents)
	{
		m_viewWidth = mm_viewWidth;
		m_viewHeight = mm_viewHeight;
		//ResizeBuffers(m_viewWidth, m_viewHeight);
		m_aspectRatio = mm_aspectRatio; 
		m_pixelPerProjectionX = mm_pixelPerProjectionX;
		m_pixelPerProjectionY = mm_pixelPerProjectionY;
		m_inversePixelPerProjectionX = mm_inversePixelPerProjectionX;
		m_inversePixelPerProjectionY = mm_inversePixelPerProjectionY;
		extents.left = mm_left;
		extents.right = mm_right;
		extents.bottom = mm_bottom;
		extents.top = mm_top;
	}
}

// *********************************************************
//		SnapShotCore()
// *********************************************************
// first 4 paramters - extents in map units; last 2 - the size of bitmap to draw this extents on
IDispatch* CMapView::SnapShotCore(double left, double right, double top, double bottom, long Width, long Height, CDC* snapDC,
								  float offsetX, float offsetY, float clipX, float clipY, float clipWidth, float clipHeight)
{
	bool createDC = (snapDC == NULL);
	CBitmap * bmp = NULL;
	
	if (createDC)
	{
		bmp = new CBitmap();
		if (!bmp->CreateDiscardableBitmap(GetDC(), Width, Height))
		{
			delete bmp;
			if( m_globalCallback != NULL )
				m_globalCallback->Error(m_key.AllocSysString(),A2BSTR("Failed to create bitmap; not enough memory?"));
			return NULL;
		}
	}

	LockWindow( lmLock );

	SetTempExtents(left, right, top, bottom, Width, Height);

	if (mm_newExtents)
	{
		ReloadImageBuffers();
		((CTiles*)m_tiles)->MarkUndrawn();		// otherwise they will be taken from screen buffer
	}

	IImage * iimg = NULL;
	bool tilesInCache = false;

	// create canvas
	CBitmap * oldBMP = NULL;
	if (createDC)
	{
		snapDC = new CDC();
		snapDC->CreateCompatibleDC(GetDC());
		oldBMP = snapDC->SelectObject(bmp);
	}
	
	// do the drawing
	_canUseLayerBuffer=FALSE;
	m_isSnapshot = true;

	
	tilesInCache =((CTiles*)m_tiles)->TilesAreInCache((void*)this);
	if (tilesInCache)
	{
		((CTiles*)m_tiles)->LoadTiles((void*)this, true);		// simply move the to the screen buffer (is performed synchronously)
	}

	CRect rcBounds(0,0,m_viewWidth,m_viewHeight);
	CRect rcClip((int)clipX, (int)clipY, (int)clipWidth, (int)clipHeight);
	CRect* r = clipWidth != 0.0 && clipHeight != 0.0 ? &rcClip : &rcBounds;
	
	// draws to output canvas directly because of m_isSnapshot parameter
	HandleNewDrawing(snapDC, rcBounds, *r, true, offsetX, offsetY);
	
	// drawing on the output canvas atop the previous drawing, naturally we don't care about flickering here
	DrawMouseMoves(snapDC, rcBounds, *r, false, offsetX, offsetY);

	_canUseLayerBuffer=FALSE;
	m_isSnapshot = false;

	if (createDC)
	{
		// create output
		VARIANT_BOOL retval;
		CoCreateInstance(CLSID_Image,NULL,CLSCTX_INPROC_SERVER,IID_IImage,(void**)&iimg);
		iimg->SetImageBitsDC((long)snapDC->m_hDC,&retval);

		double dx = (right-left)/(double)(m_viewWidth);
		double dy = (top-bottom)/(double)(m_viewHeight);
		iimg->put_dX(dx);
		iimg->put_dY(dy);
		iimg->put_XllCenter(left + dx*.5);
		iimg->put_YllCenter(bottom + dy*.5);
	
		// dispose the canvas
		snapDC->SelectObject(oldBMP);
		bmp->DeleteObject();
		snapDC->DeleteDC();
		delete bmp;
		delete snapDC;
	}

	RestoreExtents();

	if (mm_newExtents)
	{
		this->ReloadImageBuffers();
		mm_newExtents = false;
	}

	if (tilesInCache)
	{
		// restore former list of tiles in the buffer
		((CTiles*)m_tiles)->LoadTiles((void*)this, false);	  
	}

	LockWindow( lmUnlock );
	return iimg;
}

// ********************************************************************
//		DrawBackBuffer()
// ********************************************************************
// Draws the backbuffer to the specified DC (probably external)
void CMapView::DrawBackBuffer(int** hdc, int ImageWidth, int ImageHeight)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState())
	if (!hdc)
	{
		m_lastErrorCode = tkUNEXPECTED_NULL_PARAMETER;
		return;
	}
	
	CDC* dc = CDC::FromHandle((HDC)hdc);
	CRect rect(0,0, ImageWidth, ImageHeight);
	OnDraw(dc, rect, rect);
}