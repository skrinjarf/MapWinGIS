/**************************************************************************************
 * File name: Tiles.h
 *
 * Project: MapWindow Open Source (MapWinGis ActiveX control) 
 * Description: implementation of CTiles
 *
 **************************************************************************************
 * The contents of this file are subject to the Mozilla Public License Version 1.1
 * (the "License"); you may not use this file except in compliance with 
 * the License. You may obtain a copy of the License at http://www.mozilla.org/mpl/ 
 * See the License for the specific language governing rights and limitations
 * under the License.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 ************************************************************************************** 
 * Contributor(s): 
 * (Open source contributors should list themselves and their modifications here). */
 
#include "stdafx.h"
#include "Tiles.h"
#include "SqliteCache.h"
#include "RamCache.h"
#include "map.h"
#include "DiskCache.h"
#include "TileHelper.h"
#include "LoadingTask.h"
#include "CustomTileProvider.h"

::CCriticalSection m_tilesBufferSection;

// ************************************************************
//		Stop()
// ************************************************************
void CTiles::Stop() 
{
	_manager.get_Loader()->Stop();

	put_Visible(VARIANT_FALSE);   // will prevent reloading tiles after remove all layers in map destructor
}

// ************************************************************
//		ClearPrefetchErrors()
// ************************************************************
STDMETHODIMP CTiles::ClearPrefetchErrors()
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState())

	get_Prefetcher()->m_errorCount = 0;
	get_Prefetcher()->m_sumCount = 0;

	return S_OK;
}

// ************************************************************
//		get_PrefetchErrorCount()
// ************************************************************
STDMETHODIMP CTiles::get_PrefetchTotalCount(int *retVal)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState())
	*retVal = get_Prefetcher()->m_sumCount;
	return S_OK;
}

// ************************************************************
//		get_PrefetchErrorCount()
// ************************************************************
STDMETHODIMP CTiles::get_PrefetchErrorCount(int *retVal)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState())
	*retVal = get_Prefetcher()->m_errorCount;
	return S_OK;
}

// ************************************************************
//		get_GlobalCallback()
// ************************************************************
STDMETHODIMP CTiles::get_GlobalCallback(ICallback **pVal)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState())

	*pVal = _globalCallback;

	if( _globalCallback != NULL )
		_globalCallback->AddRef();

	return S_OK;
}

// ************************************************************
//		put_GlobalCallback()
// ************************************************************
STDMETHODIMP CTiles::put_GlobalCallback(ICallback *newVal)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState())
	ComHelper::SetRef(newVal, (IDispatch**)&_globalCallback);
	return S_OK;
}

// *****************************************************************
//	   get_ErrorMsg()
// *****************************************************************
STDMETHODIMP CTiles::get_ErrorMsg(long ErrorCode, BSTR *pVal)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState())

	USES_CONVERSION;
	*pVal = A2BSTR(ErrorMsg(ErrorCode));

	return S_OK;
}

// ************************************************************
//		get_LastErrorCode()
// ************************************************************
STDMETHODIMP CTiles::get_LastErrorCode(long *pVal)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState())

	*pVal = _lastErrorCode;
	_lastErrorCode = tkNO_ERROR;

	return S_OK;
}

// **************************************************************
//		ErrorMessage()
// **************************************************************
void CTiles::ErrorMessage(long ErrorCode)
{
	_lastErrorCode = ErrorCode;
	CallbackHelper::ErrorMsg("Tiles", _globalCallback, _key, ErrorMsg(_lastErrorCode));
}

// ************************************************************
//		get/put_Key()
// ************************************************************
STDMETHODIMP CTiles::get_Key(BSTR *pVal)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState())

	USES_CONVERSION;
	*pVal = OLE2BSTR(_key);

	return S_OK;
}
STDMETHODIMP CTiles::put_Key(BSTR newVal)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState())

	::SysFreeString(_key);
	_key = OLE2BSTR(newVal);

	return S_OK;
}

// *********************************************************
//	     SleepBeforeRequestTimeout()
// *********************************************************
STDMETHODIMP CTiles::get_SleepBeforeRequestTimeout(long *pVal)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState())

	*pVal = get_Prefetcher()->m_sleepBeforeRequestTimeout;

	return S_OK;
}
STDMETHODIMP CTiles::put_SleepBeforeRequestTimeout(long newVal)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState())

	if (newVal > 10000) newVal = 10000;
	if (newVal < 0) newVal = 0;
	get_Prefetcher()->m_sleepBeforeRequestTimeout = newVal;

	return S_OK;
}

// *********************************************************
//	     ScalingRatio()
// *********************************************************
STDMETHODIMP CTiles::get_ScalingRatio(double *pVal)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState())
	*pVal = _manager.scalingRatio();;
	return S_OK;
}

STDMETHODIMP CTiles::put_ScalingRatio(double newVal)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState())

	if (newVal < 0.5 || newVal > 4.0)
	{
		ErrorMessage(tkINVALID_PARAMETER_VALUE);
		return S_OK;
	}

	_manager.scalingRatio(newVal);

	return S_OK;
}

// *********************************************************
//	     AutodetectProxy()
// *********************************************************
STDMETHODIMP CTiles::AutodetectProxy(VARIANT_BOOL* retVal)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());
	
	*retVal = _provider->AutodetectProxy();
	return S_OK;
}

// *********************************************************
//	     SetProxy()
// *********************************************************
STDMETHODIMP CTiles::SetProxy(BSTR address, int port, VARIANT_BOOL* retVal)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());
	USES_CONVERSION;
	*retVal = _provider->SetProxy(OLE2A(address), port);
	return S_OK;
}

// *********************************************************
//	     SetProxyAuthorization()
// *********************************************************
STDMETHODIMP CTiles::SetProxyAuthentication(BSTR username, BSTR password, BSTR domain, VARIANT_BOOL* retVal)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());
	USES_CONVERSION;
	*retVal = _provider->SetProxyAuthorization(username, password, domain) ? VARIANT_TRUE : VARIANT_FALSE;
	return S_OK;
}

// *********************************************************
//	     ClearProxyAuthorization()
// *********************************************************
STDMETHODIMP CTiles::ClearProxyAuthorization()
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());
	_provider->ClearProxyAuthorization();
	return S_OK;
}

// *********************************************************
//	     get_Proxy()
// *********************************************************
STDMETHODIMP CTiles::get_Proxy(BSTR* retVal)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());

	USES_CONVERSION;
	CString s;
	s = _provider->get_ProxyAddress();
	if (s.GetLength() == 0)
	{
		*retVal = A2BSTR("");
	}
	else
	{
		CString format = s + ":%d";
		short num = _provider->get_ProxyPort();
		s.Format(format, num);
		*retVal = A2BSTR(s);
	}

	return S_OK;
}

// ************************************************************
//		get_CurrentZoom()
// ************************************************************
STDMETHODIMP CTiles::get_CurrentZoom(int* retVal)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());

	*retVal = -1;

	IMapViewCallback* map = _manager.get_MapCallback();
	if (map) {
		*retVal = map->_ChooseZoom(_provider, _manager.scalingRatio(), false);
	}

	return S_OK;
}

// ************************************************************
//		TilesAreInScreenBuffer()
// ************************************************************
bool CTiles::TilesAreInScreenBuffer(IMapViewCallback* map)
{
	if (!_visible || !_provider) {
		return true;		// true because no tiles are actually needed for drawing, hence we need no tiles and yes we "have" them
	}

	int zoom;
	CRect indices;
	
	if (!map->_GetTilesForMap(_provider, _manager.scalingRatio(), indices, zoom)) {
		return true;
	}
	
	for (int x = indices.left; x <= indices.right; x++)
	{
		for (int y = indices.bottom; y <= indices.top; y++)
		{
			if (!_manager.TileIsInBuffer(_provider->Id, zoom, x, y)) {
				return false;
			};
		}
	}

	return true;
}

// *********************************************************
//	     TilesAreInCache()
// *********************************************************
// checks whether all the tiles are present in cache
bool CTiles::TilesAreInCache(IMapViewCallback* map, tkTileProvider providerId)
{
	BaseProvider* provider = providerId == ProviderNone ? _provider : ((CTileProviders*)_providers)->get_Provider(providerId);

	if (!_visible || !provider) {
		// there is no valid provider, so no need to schedule download
		return true;
	}

	CRect indices;
	int zoom;

	if (!map->_GetTilesForMap(_provider, _manager.scalingRatio(), indices, zoom)) {
		return true;
	}
	
	for (int x = indices.left; x <= indices.right; x++)
	{
		for (int y = indices.bottom; y <= indices.top; y++)
		{
			bool found = _manager.TileIsInBuffer(provider->Id, zoom, x, y);

			if (!found && _manager.useRamCache())
			{
				TileCore* tile = RamCache::get_Tile(provider->Id, zoom, x, y);
				if (tile)	found = true;
			}

			if (!found && _manager.useDiskCache())
			{
				TileCore* tile = SQLiteCache::get_Tile(provider, zoom, x, y);
				if (tile)	found = true;
			}

			if (!found)
				return false;
		}
	}
	return true;
}

// *********************************************************
//	     LoadTiles()
// *********************************************************
void CTiles::LoadTiles(bool isSnapshot, CString key)
{
	// current provider will be used
	LoadTiles(isSnapshot, _provider->Id, key);
}

void CTiles::LoadTiles(bool isSnapshot, int providerId, CString key)
{
	// any provider can be passed (for caching or snapshot)
	if (!_visible) return;
	
	BaseProvider* provider = ((CTileProviders*)_providers)->get_Provider(providerId);
	if (provider) {
		_manager.LoadTiles(provider, isSnapshot, key);
	}
}

// *********************************************************
//	     Provider
// *********************************************************
STDMETHODIMP CTiles::get_Provider(tkTileProvider* pVal)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());

	CustomTileProvider* p = dynamic_cast<CustomTileProvider*>(_provider);
	*pVal = p ? tkTileProvider::ProviderCustom : (tkTileProvider)_provider->Id;

	return S_OK;
}

STDMETHODIMP CTiles::put_Provider(tkTileProvider newVal)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());

	if (newVal < 0 || newVal >= tkTileProvider::ProviderCustom) {
		return S_OK;
	}

	if (_provider->Id != newVal && newVal != tkTileProvider::ProviderCustom) 
	{
		put_ProviderId((int)newVal);
	}

	return S_OK;
}

// *********************************************************
//	     get_ProviderName
// *********************************************************
STDMETHODIMP CTiles::get_ProviderName(BSTR* retVal)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());

	USES_CONVERSION;
	*retVal = _provider ? A2BSTR(_provider->Name) : A2BSTR("");

	return S_OK;
}

// *********************************************************
//	     GridLinesVisible
// *********************************************************
STDMETHODIMP CTiles::get_GridLinesVisible(VARIANT_BOOL* pVal)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());
	*pVal = _gridLinesVisible;
	return S_OK;
}
STDMETHODIMP CTiles::put_GridLinesVisible(VARIANT_BOOL newVal)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());
	_gridLinesVisible = newVal ? true: false;
	return S_OK;
}

// *********************************************************
//	     MinScaleToCache
// *********************************************************
STDMETHODIMP CTiles::get_MinScaleToCache(int* pVal)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());
	*pVal = _minScaleToCache;		// TODO: use in caching process
	return S_OK;
}
STDMETHODIMP CTiles::put_MinScaleToCache(int newVal)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());
	_minScaleToCache = newVal;
	return S_OK;
}

// *********************************************************
//	     MaxScaleToCache
// *********************************************************
STDMETHODIMP CTiles::get_MaxScaleToCache(int* pVal)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());
	*pVal = _maxScaleToCache;		// TODO: use in caching process
	return S_OK;
}
STDMETHODIMP CTiles::put_MaxScaleToCache(int newVal)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());
	_maxScaleToCache = newVal;
	return S_OK;
}

// *********************************************************
//	     Visible
// *********************************************************
STDMETHODIMP CTiles::get_Visible(VARIANT_BOOL* pVal)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());
	*pVal = _visible;
	return S_OK;
}

STDMETHODIMP CTiles::put_Visible(VARIANT_BOOL newVal)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());
	_visible = newVal != 0;
	return S_OK;
}

// *********************************************************
//	     CustomProviders
// *********************************************************
STDMETHODIMP CTiles::get_Providers(ITileProviders** retVal)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());
	if (_providers)
		_providers->AddRef();
	*retVal = _providers;
	return S_OK;
}

// *********************************************************
//	     DoCaching
// *********************************************************
STDMETHODIMP CTiles::get_DoCaching(tkCacheType type, VARIANT_BOOL* pVal)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());
	switch(type)
	{
		case tkCacheType::RAM:
			*pVal = _manager.doRamCaching();
			break;
		case tkCacheType::Disk:
			*pVal = _manager.doDiskCaching();
			break;
		case tkCacheType::Both:
			*pVal = _manager.doRamCaching() || _manager.doDiskCaching();
			break;
		default:
			*pVal = VARIANT_FALSE;
			break;
	}
	return S_OK;
}
STDMETHODIMP CTiles::put_DoCaching(tkCacheType type, VARIANT_BOOL newVal)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());
	switch(type)
	{
		case tkCacheType::RAM:
			_manager.doRamCaching(newVal != 0);
			break;
		case tkCacheType::Disk:
			_manager.doDiskCaching(newVal != 0);
			break;
		case tkCacheType::Both:
			bool val = newVal != 0;
			_manager.doDiskCaching(val);
			_manager.doRamCaching(val);
			break;
	}
	return S_OK;
}

// *********************************************************
//	     UseCache
// *********************************************************
STDMETHODIMP CTiles::get_UseCache(tkCacheType type, VARIANT_BOOL* pVal)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());
	switch(type)
	{
		case tkCacheType::RAM:
			*pVal = _manager.useRamCache();
			break;
		case tkCacheType::Disk:
			*pVal = _manager.useDiskCache();
			break;
		case tkCacheType::Both:
			*pVal = _manager.useRamCache() || _manager.useDiskCache();
			break;
		default:
			*pVal = VARIANT_FALSE;
			break;
	}
	return S_OK;
}
STDMETHODIMP CTiles::put_UseCache(tkCacheType type, VARIANT_BOOL newVal)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());
	switch(type)
	{
		case tkCacheType::RAM:
			_manager.useRamCache(newVal != 0);
			break;
		case tkCacheType::Disk:
			_manager.useDiskCache(newVal != 0);
			break;
		case tkCacheType::Both:
			bool val = newVal != 0;
			_manager.useRamCache(val);
			_manager.useDiskCache(val);
			break;
	}
	return S_OK;
}

// *********************************************************
//	     UseServer
// *********************************************************
STDMETHODIMP CTiles::get_UseServer(VARIANT_BOOL* pVal)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());
	*pVal = _manager.useServer();
	return S_OK;
}
STDMETHODIMP CTiles::put_UseServer(VARIANT_BOOL newVal)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());
	_manager.useServer(newVal != 0);
	return S_OK;
}

// *********************************************************
//	     get_DiskCacheFilename
// *********************************************************
STDMETHODIMP CTiles::get_DiskCacheFilename(BSTR* retVal)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());
	USES_CONVERSION;
	*retVal = W2BSTR(SQLiteCache::get_DbName());
	return S_OK;
}

// *********************************************************
//	     put_DiskCacheFilename
// *********************************************************
STDMETHODIMP CTiles::put_DiskCacheFilename(BSTR pVal)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());
	USES_CONVERSION;
	SQLiteCache::set_DbName(OLE2W(pVal));
	return S_OK;
}

// *********************************************************
//	     MaxCacheSize
// *********************************************************
STDMETHODIMP CTiles::get_MaxCacheSize(tkCacheType type, double* pVal)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());
	switch(type)
	{
		case tkCacheType::RAM:
			*pVal =  RamCache::m_maxSize;
			break;
		case tkCacheType::Disk:
			*pVal = SQLiteCache::maxSizeDisk;
			break;
		default:
			*pVal = 0;
			break;
	}
	return S_OK;
}
STDMETHODIMP CTiles::put_MaxCacheSize(tkCacheType type, double newVal)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());
	switch(type)
	{
		case tkCacheType::RAM:
			RamCache::m_maxSize = newVal;
			break;
		case tkCacheType::Disk:
			SQLiteCache::maxSizeDisk = newVal;
			break;
		case tkCacheType::Both:
			RamCache::m_maxSize = SQLiteCache::maxSizeDisk = newVal;
			break;
	}
	return S_OK;
}

// *********************************************************
//	     ClearCache()
// *********************************************************
STDMETHODIMP CTiles::ClearCache(tkCacheType type)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());
	switch(type)
	{
		case tkCacheType::RAM:
			RamCache::ClearByProvider(tkTileProvider::ProviderNone, 0, 100);
			break;
		case tkCacheType::Disk:
			SQLiteCache::Clear(tkTileProvider::ProviderNone, 0, 100);
			break;
		case tkCacheType::Both:
			RamCache::ClearAll(0, 100);
			SQLiteCache::Clear(tkTileProvider::ProviderNone, 0, 100);
			break;
	}
	return S_OK;
}

// *********************************************************
//	     ClearCache2()
// *********************************************************
STDMETHODIMP CTiles::ClearCache2(tkCacheType type, tkTileProvider provider, int fromScale, int toScale)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());
	switch(type)
	{
		case tkCacheType::RAM:
			RamCache::ClearByProvider(provider, fromScale, toScale);
			break;
		case tkCacheType::Disk:
			SQLiteCache::Clear(provider, fromScale, toScale);
			break;
		case tkCacheType::Both:
			SQLiteCache::Clear(provider, fromScale, toScale);
			RamCache::ClearByProvider(provider, fromScale, toScale);
			break;
	}
	return S_OK;
}

// *********************************************************
//	     get_CacheSize()
// *********************************************************
STDMETHODIMP CTiles::get_CacheSize(tkCacheType type, double* retVal)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());
	*retVal = 0.0;
	switch(type)
	{
		case tkCacheType::RAM:
			*retVal = RamCache::get_MemorySize();
			break;
		case tkCacheType::Disk:
			*retVal = SQLiteCache::get_FileSize();
			break;
		case tkCacheType::Both:
			*retVal = 0.0;		// it hardly makes sense to show sum of both values or any of it separately
			break;
	}
	return S_OK;
}

// *********************************************************
//	     get_CacheSize3()
// *********************************************************
// TODO: use provider id rather than provider enumeration.
STDMETHODIMP CTiles::get_CacheSize2(tkCacheType type, tkTileProvider provider, int scale, double* retVal)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());
	*retVal = 0.0;
	switch(type)
	{
		case tkCacheType::RAM:
			*retVal = RamCache::get_MemorySize(provider, scale);
			break;
		case tkCacheType::Disk:
			*retVal = SQLiteCache::get_FileSize(provider, scale);
			break;
		case tkCacheType::Both:
			*retVal = 0.0;	// it doesn't make sense to return any of the two
			break;
	}
	return S_OK;
}

// ********************************************************
//     Serialize()
// ********************************************************
STDMETHODIMP CTiles::Serialize(BSTR* retVal)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState())
	CPLXMLNode* psTree = this->SerializeCore("TilesClass");
	Utility::SerializeAndDestroyXmlTree(psTree, retVal);
	return S_OK;
}

// ********************************************************
//     SerializeCore()
// ********************************************************
CPLXMLNode* CTiles::SerializeCore(CString ElementName)
{
	USES_CONVERSION;
	CPLXMLNode* psTree = CPLCreateXMLNode( NULL, CXT_Element, ElementName);
	
	if (!_visible)
		Utility::CPLCreateXMLAttributeAndValue(psTree, "Visible", CPLString().Printf("%d", (int)_visible));

	if (_gridLinesVisible)
		Utility::CPLCreateXMLAttributeAndValue(psTree, "GridLinesVisible", CPLString().Printf("%d", (int)_gridLinesVisible));

	if (_provider->Id != 0)
		Utility::CPLCreateXMLAttributeAndValue(psTree, "Provider", CPLString().Printf("%d", (int)_provider->Id));

	if (!_manager.doRamCaching())
		Utility::CPLCreateXMLAttributeAndValue(psTree, "DoRamCaching", CPLString().Printf("%d", (int)_manager.doRamCaching()));

	if (_manager.doDiskCaching())
		Utility::CPLCreateXMLAttributeAndValue(psTree, "DoDiskCaching", CPLString().Printf("%d", (int)_manager.doDiskCaching()));

	if (!_manager.useRamCache())
		Utility::CPLCreateXMLAttributeAndValue(psTree, "UseRamCache", CPLString().Printf("%d", (int)_manager.useRamCache()));

	if (!_manager.useDiskCache())
		Utility::CPLCreateXMLAttributeAndValue(psTree, "UseDiskCache", CPLString().Printf("%d", (int)_manager.useDiskCache()));

	if (!_manager.useServer())
		Utility::CPLCreateXMLAttributeAndValue(psTree, "UseServer", CPLString().Printf("%d", (int)_manager.useServer()));

	if (RamCache::m_maxSize != 100.0)
		Utility::CPLCreateXMLAttributeAndValue(psTree, "MaxRamCacheSize", CPLString().Printf("%f", RamCache::m_maxSize));

	if (SQLiteCache::maxSizeDisk != 100.0)
		Utility::CPLCreateXMLAttributeAndValue(psTree, "MaxDiskCacheSize", CPLString().Printf("%f", SQLiteCache::maxSizeDisk));

	if (_minScaleToCache != 0)
		Utility::CPLCreateXMLAttributeAndValue(psTree, "MinScaleToCache", CPLString().Printf("%d", _minScaleToCache));

	if (_maxScaleToCache != 100)
		Utility::CPLCreateXMLAttributeAndValue(psTree, "MaxScaleToCache", CPLString().Printf("%d", _maxScaleToCache));
	
	CStringW dbName = SQLiteCache::get_DbName();
	if (dbName.GetLength() != 0)
		Utility::CPLCreateXMLAttributeAndValue(psTree, "DiskCacheFilename", dbName);
	
	// serialization of custom providers
	CPLXMLNode* psProviders = CPLCreateXMLNode( NULL, CXT_Element, "TileProviders");
	if (psProviders)
	{
		vector<BaseProvider*>* providers = ((CTileProviders*)_providers)->GetList();
		for(size_t i = 0; i < providers->size(); i++)
		{
			CustomTileProvider* cp = dynamic_cast<CustomTileProvider*>(providers->at(i));
			if (cp)
			{
				CPLXMLNode* psCustom = CPLCreateXMLNode( NULL, CXT_Element, "TileProvider");
				Utility::CPLCreateXMLAttributeAndValue(psCustom, "Id", CPLString().Printf("%d", cp->Id));
				Utility::CPLCreateXMLAttributeAndValue(psCustom, "Name", cp->Name);
				Utility::CPLCreateXMLAttributeAndValue(psCustom, "Url", cp->get_UrlFormat());
				Utility::CPLCreateXMLAttributeAndValue(psCustom, "Projection", CPLString().Printf("%d", (int)cp->get_Projection()));
				Utility::CPLCreateXMLAttributeAndValue(psCustom, "MinZoom", CPLString().Printf("%d", cp->get_MinZoom()));
				Utility::CPLCreateXMLAttributeAndValue(psCustom, "MaxZoom", CPLString().Printf("%d", cp->get_MaxZoom()));
				CPLAddXMLChild(psProviders, psCustom);
			}
		}
		CPLAddXMLChild(psTree, psProviders);
	}
	return psTree;
}

// ********************************************************
//     Deserialize()
// ********************************************************
STDMETHODIMP CTiles::Deserialize(BSTR newVal)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState())
	USES_CONVERSION;

	CString s = OLE2CA(newVal);
	CPLXMLNode* node = CPLParseXMLString(s.GetString());
	if (node)
	{
		CPLXMLNode* nodeTiles = CPLGetXMLNode(node, "=TilesClass");
		if (nodeTiles)
		{
			DeserializeCore(nodeTiles);
		}
		CPLDestroyXMLNode(node);
	}
	return S_OK;
}

void setBoolean(CPLXMLNode *node, CString name, bool& value)
{
	CString s = CPLGetXMLValue( node, name, NULL );
	if (s != "") value = atoi( s ) == 0? false : true;
}

void setInteger(CPLXMLNode *node, CString name, int& value)
{
	CString s = CPLGetXMLValue( node, name, NULL );
	if (s != "") value = atoi( s );
}

void setDouble(CPLXMLNode *node, CString name, double& value)
{
	CString s = CPLGetXMLValue( node, name, NULL );
	if (s != "") value = Utility::atof_custom( s );
}

// ********************************************************
//     DeserializeCore()
// ********************************************************
bool CTiles::DeserializeCore(CPLXMLNode* node)
{
	if (!node)
		return false;

	this->SetDefaults();

	setBoolean(node, "Visible", _visible);
	setBoolean(node, "GridLinesVisible", _gridLinesVisible);

	bool temp;
	setBoolean(node, "DoRamCaching", temp);
	_manager.doRamCaching(temp);

	setBoolean(node, "DoDiskCaching", temp);
	_manager.doDiskCaching(temp);

	setBoolean(node, "UseRamCache", temp);
	_manager.useRamCache(temp);

	setBoolean(node, "UseDiskCache", temp);
	_manager.useDiskCache(temp);

	setBoolean(node, "UseServer", temp);
	_manager.useServer(temp);

	CString s = CPLGetXMLValue( node, "Provider", NULL );
	if (s != "") this->put_ProviderId(atoi( s ));

	setDouble(node, "MaxRamCacheSize", RamCache::m_maxSize);
	setDouble(node, "MaxDiskCacheSize", SQLiteCache::maxSizeDisk);
	setInteger(node, "MinScaleToCache", _minScaleToCache);
	setInteger(node, "MaxScaleToCache", _maxScaleToCache);
	
	USES_CONVERSION;
	s = CPLGetXMLValue( node, "DiskCacheFilename", NULL );
	if (s != "") SQLiteCache::set_DbName(Utility::ConvertFromUtf8(s));

	// custom providers
	CPLXMLNode* nodeProviders = CPLGetXMLNode(node, "TileProviders");
	if (nodeProviders)
	{
		// don't clear providers as it will clear the cache as well,
		// if provider with certain id exists, it simply won't be added twice
		vector<BaseProvider*>* providers = ((CTileProviders*)_providers)->GetList();

		CPLXMLNode* nodeProvider = nodeProviders->psChild;
		while (nodeProvider)
		{
			if (strcmp(nodeProvider->pszValue, "TileProvider") == 0)
			{
				int id, minZoom, maxZoom, projection;
				CString url, name;
				
				s = CPLGetXMLValue( nodeProvider, "Id", NULL );
				if (s != "") id = atoi(s);

				s = CPLGetXMLValue( nodeProvider, "MinZoom", NULL );
				if (s != "") minZoom = atoi(s);

				s = CPLGetXMLValue( nodeProvider, "MaxZoom", NULL );
				if (s != "") maxZoom = atoi(s);

				s = CPLGetXMLValue( nodeProvider, "Projection", NULL );
				if (s != "") projection = atoi(s);

				s = CPLGetXMLValue( nodeProvider, "Url", NULL );
				if (s != "") url = s;

				s = CPLGetXMLValue( nodeProvider, "Name", NULL );
				if (s != "") name = s;

				VARIANT_BOOL vb;
				CComBSTR bstrName(name);
				CComBSTR bstrUrl(url);

				_providers->Add(id, bstrName, bstrUrl, (tkTileProjection)projection, minZoom, maxZoom, &vb);
			}
			nodeProvider = nodeProvider->psNext;
		}
	}
	return true;
}

// *********************************************************
//	     CustomProviderId
// *********************************************************
STDMETHODIMP CTiles::get_ProviderId(int* retVal)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());

	if (!_visible)
	{
		*retVal = -1;
		return S_OK;
	}

	*retVal = _provider->Id;
	return S_OK;
}

STDMETHODIMP CTiles::put_ProviderId(int providerId)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());

	put_Visible(providerId == -1 ? VARIANT_FALSE: VARIANT_TRUE);

	BaseProvider* provider = ((CTileProviders*)_providers)->get_Provider(providerId);

	if (provider) {
		_provider = provider;
	}
	return S_OK;
}

// *********************************************************
//	     GetTilesIndices
// *********************************************************
STDMETHODIMP CTiles::GetTilesIndices(IExtents* boundsDegrees, int zoom, int providerId, IExtents** retVal)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());
	if (!boundsDegrees)
	{
		ErrorMessage(tkUNEXPECTED_NULL_PARAMETER);
		return S_OK;
	}
	
	double xMin, xMax, yMin, yMax, zMin, zMax;
	boundsDegrees->GetBounds(&xMin, &yMin, &zMin, &xMax, &yMax, &zMax);
	
	BaseProvider* provider = ((CTileProviders*)_providers)->get_Provider(providerId);
	if (provider)
	{
		CPoint p1;
		provider->get_Projection()->FromLatLngToXY(PointLatLng(yMax, xMin), zoom, p1);

		CPoint p2;
		provider->get_Projection()->FromLatLngToXY(PointLatLng(yMin, xMax), zoom, p2);

		IExtents* ext = NULL;
		ComHelper::CreateExtents(&ext);
		ext->SetBounds(p1.x, p1.y, 0.0, p2.x, p2.y, 0.0);
		*retVal = ext;
	}
	else {
		*retVal = NULL;
	}
	return S_OK;
}

// *********************************************************
//	     Prefetch
// *********************************************************
STDMETHODIMP CTiles::Prefetch(double minLat, double maxLat, double minLng, double maxLng, int zoom, int providerId, 
							  IStopExecution* stop, LONG* retVal)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());
	
	BaseProvider* p = ((CTileProviders*)_providers)->get_Provider(providerId);
	if (!p)
	{
		ErrorMessage(tkINVALID_PROVIDER_ID);
		return S_OK;
	}
	else
	{
		CPoint p1;
		p->get_Projection()->FromLatLngToXY(PointLatLng(minLat, minLng), zoom, p1);

		CPoint p2;
		p->get_Projection()->FromLatLngToXY(PointLatLng(maxLat, maxLng), zoom, p2);

		this->Prefetch2(p1.x, p2.x, MIN(p1.y, p2.y) , MAX(p1.y, p2.y), zoom, providerId, stop, retVal);
		return S_OK;
	}
}

// *********************************************************
//	     Prefetch2
// *********************************************************
STDMETHODIMP CTiles::Prefetch2(int minX, int maxX, int minY, int maxY, int zoom, int providerId, IStopExecution* stop, LONG* retVal)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());
	*retVal = PrefetchCore(minX, maxX, minY, maxY, zoom, providerId, m_globalSettings.emptyBstr, m_globalSettings.emptyBstr, stop);
	return S_OK;
}

// *********************************************************
//	     StartLogRequests()
// *********************************************************
STDMETHODIMP CTiles::StartLogRequests(BSTR filename, VARIANT_BOOL errorsOnly, VARIANT_BOOL* retVal)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());
	USES_CONVERSION;
	CStringW path = OLE2W(filename);
	tilesLogger.Open(path);
	tilesLogger.errorsOnly = errorsOnly ? true: false;
	*retVal = tilesLogger.IsOpened();
	return S_OK;
}

// *********************************************************
//	     StopLogRequests()
// *********************************************************
STDMETHODIMP CTiles::StopLogRequests()
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());
	tilesLogger.Close();
	return S_OK;
}

// *********************************************************
//	     LogIsOpened()
// *********************************************************
STDMETHODIMP CTiles::get_LogIsOpened(VARIANT_BOOL* retVal)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());
	*retVal = tilesLogger.IsOpened();
	return S_OK;
}

// *********************************************************
//	     LogFilename()
// *********************************************************
STDMETHODIMP CTiles::get_LogFilename(BSTR* retVal)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());
	USES_CONVERSION;
	*retVal = W2BSTR(tilesLogger.GetFilename());
	return S_OK;
}

// *********************************************************
//	     LogErrorsOnly()
// *********************************************************
STDMETHODIMP CTiles::get_LogErrorsOnly(VARIANT_BOOL *retVal)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState())
	*retVal = tilesLogger.errorsOnly;
	return S_OK;
}
STDMETHODIMP CTiles::put_LogErrorsOnly(VARIANT_BOOL newVal)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState())
	tilesLogger.errorsOnly = newVal ? true: false;
	return S_OK;
}

// *********************************************************
//	     PrefetchToFolder()
// *********************************************************
// Writes tiles to the specified folder
STDMETHODIMP CTiles::PrefetchToFolder(IExtents* ext, int zoom, int providerId, BSTR savePath, BSTR fileExt, IStopExecution* stop, LONG* retVal)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());
	*retVal = 0;
	
	USES_CONVERSION;
	CStringW path = OLE2W(savePath);
	if (!Utility::DirExists(path))
	{
		ErrorMessage(tkFOLDER_NOT_EXISTS);
		*retVal = -1;
		return S_OK;
	}

	if (tilesLogger.IsOpened())
	{
		tilesLogger.out() << "\n";
		tilesLogger.out() << "PREFETCHING TILES:\n";
		tilesLogger.out() << "ZOOM " << zoom << endl;
		tilesLogger.out() << "---------------------" << endl;
	}

	BaseProvider* p = ((CTileProviders*)_providers)->get_Provider(providerId);
	if (!p)
	{
		ErrorMessage(tkINVALID_PROVIDER_ID);
		*retVal = -1;
		return S_OK;
	}
	else
	{
		double xMin, xMax, yMin, yMax, zMin, zMax;
		ext->GetBounds(&xMin, &yMin, &zMin, &xMax, &yMax, &zMax);

		CPoint p1;
		p->get_Projection()->FromLatLngToXY(PointLatLng(yMin, xMin), zoom, p1);

		CPoint p2;
		p->get_Projection()->FromLatLngToXY(PointLatLng(yMax, xMax), zoom, p2);

		*retVal = this->PrefetchCore(p1.x, p2.x, MIN(p1.y, p2.y) , MAX(p1.y, p2.y), zoom, providerId, savePath, fileExt, stop);
	}
	return S_OK;
}

// *********************************************************
//	     PrefetchCore()
// *********************************************************
long CTiles::PrefetchCore(int minX, int maxX, int minY, int maxY, int zoom, int providerId, 
								  BSTR savePath, BSTR fileExt, IStopExecution* stop)
{
	BaseProvider* provider = ((CTileProviders*)_providers)->get_Provider(providerId);
	if (!provider)
	{
		ErrorMessage(tkINVALID_PROVIDER_ID);
		return 0;
	}

	if (provider->get_MaxZoom() < zoom) 
	{
		ErrorMessage(tkINVALID_ZOOM_LEVEL);
		return 0;
	}
	
	CSize size1, size2;
	provider->get_Projection()->GetTileMatrixMinXY(zoom, size1);
	provider->get_Projection()->GetTileMatrixMaxXY(zoom, size2);

	minX = (int)BaseProjection::Clip(minX, size1.cx, size2.cx);
	maxX = (int)BaseProjection::Clip(maxX, size1.cy, size2.cy);
	minY = (int)BaseProjection::Clip(minY, size1.cx, size2.cx);
	maxY = (int)BaseProjection::Clip(maxY, size1.cy, size2.cy);
		
	int centX = (maxX + minX)/2;
	int centY = (maxY + minY)/2;

	USES_CONVERSION;
	CStringW path = OLE2W(savePath);
	if (path.GetLength() > 0 && path.GetAt(path.GetLength() - 1) != L'\\')
	{
		path += L"\\";
	}
	CacheType type = path.GetLength() > 0 ? CacheType::DiskCache : CacheType::SqliteCache;

	if (type == CacheType::DiskCache)
	{
		CString ext = OLE2A(fileExt);
		DiskCache::ext = ext;
		DiskCache::rootPath = path;
		DiskCache::encoder = "image/png";	// default
			
		if (ext.GetLength() >= 4)
		{
			CStringW s = ext.Mid(0, 4).MakeLower(); // try to guess it from input
			if (s == ".png")
			{
				DiskCache::encoder = "image/png";
			}
			else if (s == ".jpg")
			{
				DiskCache::encoder = "image/jpeg";
			}
		}
	}

	std::vector<CTilePoint*> points;
	for (int x = minX; x <= maxX; x++)
	{
		for (int y = minY; y <= maxY; y++)
		{
			if ((type == CacheType::SqliteCache && !SQLiteCache::get_Exists(provider, zoom, x, y)) || 
					type == CacheType::DiskCache && !DiskCache::get_TileExists(zoom, x, y))
			{
				CTilePoint* pnt = new CTilePoint(x, y);
				pnt->dist = sqrt(pow((pnt->x - centX), 2.0) + pow((pnt->y - centY), 2.0));
				points.push_back(pnt);
			}
		}
	}

	if (points.size() > 0)
	{
		get_Prefetcher()->doCacheSqlite = type == CacheType::SqliteCache;
		get_Prefetcher()->doCacheDisk = type == CacheType::DiskCache;
			
		if (type == CacheType::SqliteCache)
		{
			SQLiteCache::Initialize(SqliteOpenMode::OpenIfExists);
		}
		else 
		{
			DiskCache::CreateFolders(zoom, points);
		}
		get_Prefetcher()->m_stopCallback = stop;

		if (_globalCallback)
		{
			get_Prefetcher()->m_callback = this->_globalCallback;
		}
			
		// actual call to do the job
		get_Prefetcher()->Load(points, zoom, provider, (void*)this, false, "", 0, true);

		for (size_t i = 0; i < points.size(); i++)
		{
			delete points[i];
		}
		return points.size();
	}
	return 0;
}

// *********************************************************
//	     get_DiskCacheCount
// *********************************************************
STDMETHODIMP CTiles::get_DiskCacheCount(int provider, int zoom, int xMin, int xMax, int yMin, int yMax, LONG* retVal)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());
	*retVal = SQLiteCache::get_TileCount(provider, zoom, xMin, xMax, yMin, yMax);
	return S_OK;
}

// *********************************************************
//	     CheckConnection
// *********************************************************
STDMETHODIMP CTiles::CheckConnection(BSTR url, VARIANT_BOOL* retVal)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());
	if (_provider != NULL)
	{
		USES_CONVERSION;
		*retVal = _provider->CheckConnection(OLE2A(url)) ? VARIANT_TRUE: VARIANT_FALSE;
	}
	else {
		*retVal = VARIANT_FALSE;
	}
	return S_OK;
}

// *********************************************************
//	     get_TileBounds
// *********************************************************
STDMETHODIMP CTiles::GetTileBounds(int provider, int zoom, int tileX, int tileY, IExtents** retVal)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());

	*retVal = VARIANT_FALSE;

	BaseProvider* prov = ((CTileProviders*)_providers)->get_Provider(provider);
	if (!prov)
	{
		ErrorMessage(tkINVALID_PROVIDER_ID);
		return S_OK;
	}
	
	CSize size;
	prov->get_Projection()->GetTileMatrixSizeXY(zoom, size);

	if (tileX < 0 || tileX > size.cx - 1 || tileY < 0 || tileY > size.cy - 1)
	{
		ErrorMessage(tkINDEX_OUT_OF_BOUNDS);
		return S_OK;
	}
	
	CPoint pnt1(tileX, tileY);
	CPoint pnt2(tileX + 1, tileY + 1);
	PointLatLng p1, p2;

	prov->get_Projection()->FromXYToLatLng(pnt1, zoom, p1);
	prov->get_Projection()->FromXYToLatLng(pnt2, zoom, p2);

	IExtents* ext = NULL;
	ComHelper::CreateExtents(&ext);

	ext->SetBounds(p1.Lng, p1.Lat, 0.0, p2.Lng, p2.Lat, 0.0);

	*retVal = ext;
	
	return S_OK;
}

// ************************************************************
//		get_MaxZoom
// ************************************************************
STDMETHODIMP CTiles::get_MaxZoom(int* retVal)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());
	*retVal = _provider->get_MaxZoom();
	return S_OK;
}

// ************************************************************
//		put_MinZoom
// ************************************************************
STDMETHODIMP CTiles::get_MinZoom(int* retVal)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());
	*retVal = _provider->get_MinZoom();
	return S_OK;
}

// ************************************************************
//		ServerProjection
// ************************************************************
STDMETHODIMP CTiles::get_ServerProjection(tkTileProjection* retVal)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());

	BaseProjection* p = _provider->get_Projection();

	*retVal = p ? p->get_ServerProjection() : tkTileProjection::SphericalMercator;

	return S_OK;
}

// ************************************************************
//		ProjectionStatus
// ************************************************************
STDMETHODIMP CTiles::get_ProjectionStatus(tkTilesProjectionStatus* retVal)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());

	*retVal = tkTilesProjectionStatus::tpsEmptyOrInvalid;

	IMapViewCallback* map = _manager.get_MapCallback();
	if (!map) {
		return S_OK;
	}

	IGeoProjection* gp = map->_GetMapProjection();
	CComPtr<IGeoProjection> gpServer = NULL;

	// TODO: store GeoProjection instance directly
	GetUtils()->TileProjectionToGeoProjection(_provider->get_Projection()->get_ServerProjection(), &gpServer);

	if (gp && gpServer)
	{
		VARIANT_BOOL vb;
		gp->get_IsSame(gpServer, &vb);

		if (vb) {
			*retVal = tkTilesProjectionStatus::tpsNative;
		}
		else
		{
			gpServer->StartTransform(gp, &vb);
			if (vb)
			{
				*retVal = tkTilesProjectionStatus::tpsCompatible;
				gpServer->StopTransform();
			}
		}
	}

	return S_OK;
}

// ************************************************************
//		ProxyAuthenticationScheme
// ************************************************************
STDMETHODIMP CTiles::get_ProxyAuthenticationScheme(tkProxyAuthentication* pVal)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());
	*pVal  = m_globalSettings.proxyAuthentication;
	return S_OK;
}
STDMETHODIMP CTiles::put_ProxyAuthenticationScheme(tkProxyAuthentication newVal)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());
	m_globalSettings.proxyAuthentication = newVal;
	return S_OK;
}
