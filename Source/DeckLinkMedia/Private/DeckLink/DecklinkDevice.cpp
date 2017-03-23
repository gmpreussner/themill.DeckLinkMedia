
#include "DeckLinkMediaPrivate.h"
#include "DecklinkDevice.h"

#include <string>
#include <locale>
#include <codecvt>

#include "Runtime/Core/Public/Windows/AllowWindowsPlatformTypes.h"
#include "Runtime/Core/Public/Windows/AllowWindowsPlatformAtomics.h"
#include <comdef.h>

#pragma warning( disable: 4800 )

std::string ws2s( const std::wstring& wstr )
{
	using convert_typeX = std::codecvt_utf8<wchar_t>;
	std::wstring_convert<convert_typeX, wchar_t> converterX;

	return converterX.to_bytes( wstr );
}

DeckLinkDeviceDiscovery::DeckLinkDeviceDiscovery( std::function<void( IDeckLink*, size_t )> deviceCallback )
	: mDeviceArrivedCallback{ deviceCallback }, m_deckLinkDiscovery( NULL ), m_refCount( 1 ), mVideoConverter{ NULL }
{
	if( CoCreateInstance( CLSID_CDeckLinkDiscovery, NULL, CLSCTX_ALL, IID_IDeckLinkDiscovery, (void**)&m_deckLinkDiscovery ) != S_OK ) {
		m_deckLinkDiscovery = NULL;
		UE_LOG( LogDeckLinkMedia, Warning, TEXT( "Failed to create decklink discovery instance." ) );
		return;
	}

	if( CoCreateInstance( CLSID_CDeckLinkVideoConversion, NULL, CLSCTX_ALL, IID_IDeckLinkVideoConversion, (void**)&mVideoConverter ) != S_OK ) {
		UE_LOG( LogDeckLinkMedia, Error, TEXT( "Failed to create video converter." ) );
		mVideoConverter = NULL;
	}

	m_deckLinkDiscovery->InstallDeviceNotifications( this );
}


DeckLinkDeviceDiscovery::~DeckLinkDeviceDiscovery()
{
	if( m_deckLinkDiscovery != NULL )
	{
		// Uninstall device arrival notifications and release discovery object
		m_deckLinkDiscovery->UninstallDeviceNotifications();
		m_deckLinkDiscovery->Release();
		m_deckLinkDiscovery = NULL;
	}
}

std::string DeckLinkDeviceDiscovery::GetDeviceName( IDeckLink * device )
{
	if( device == nullptr ) {
		UE_LOG( LogDeckLinkMedia, Error, TEXT( "No device." ) );
		return "";
	}
	
	BSTR cfStrName;
	// Get the name of this device
	if( device->GetDisplayName( &cfStrName ) == S_OK ) {
		check( cfStrName != NULL );
		std::wstring ws( cfStrName, SysStringLen( cfStrName ) );
		return ws2s( ws );
	}

	UE_LOG( LogDeckLinkMedia, Warning, TEXT( "No device." ) );
	return "DeckLink";
}

HRESULT     DeckLinkDeviceDiscovery::DeckLinkDeviceArrived( IDeckLink* decklink )
{
	IDeckLinkAttributes* deckLinkAttributes = NULL;
	LONGLONG index = 0;
	if( decklink->QueryInterface( IID_IDeckLinkAttributes, (void**)&deckLinkAttributes ) == S_OK ) {
		if( deckLinkAttributes->GetInt( BMDDeckLinkSubDeviceIndex, &index ) != S_OK ) {
			UE_LOG( LogDeckLinkMedia, Error, TEXT( "Cannot read device index." ) );
		}
		deckLinkAttributes->Release();
	}

	mDeviceArrivedCallback( decklink, static_cast<size_t>( index ) );
	return S_OK;
}

HRESULT     DeckLinkDeviceDiscovery::DeckLinkDeviceRemoved(/* in */ IDeckLink* decklink )
{
	UE_LOG( LogDeckLinkMedia, Log, TEXT( "DeckLink device %s removed." ), GetDeviceName( decklink ).c_str() );
	return S_OK;
}

HRESULT	STDMETHODCALLTYPE DeckLinkDeviceDiscovery::QueryInterface( REFIID iid, LPVOID *ppv )
{
	HRESULT			result = E_NOINTERFACE;

	if( ppv == NULL )
		return E_INVALIDARG;

	// Initialise the return result
	*ppv = NULL;

	// Obtain the IUnknown interface and compare it the provided REFIID
	if( iid == IID_IUnknown )
	{
		*ppv = this;
		AddRef();
		result = S_OK;
	}
	else if( iid == IID_IDeckLinkDeviceNotificationCallback )
	{
		*ppv = (IDeckLinkDeviceNotificationCallback*)this;
		AddRef();
		result = S_OK;
	}

	return result;
}

ULONG STDMETHODCALLTYPE DeckLinkDeviceDiscovery::AddRef( void )
{
	return InterlockedIncrement( (LONG*)&m_refCount );
}

ULONG STDMETHODCALLTYPE DeckLinkDeviceDiscovery::Release( void )
{
	ULONG		newRefValue;

	newRefValue = InterlockedDecrement( (LONG*)&m_refCount );
	if( newRefValue == 0 )
	{
		delete this;
		return 0;
	}

	return newRefValue;
}

DeckLinkDevice::DeckLinkDevice( DeckLinkDeviceDiscovery * manager, IDeckLink * device )
: mDeviceDiscovery( manager )
, mDecklink( device )
, mDecklinkInput( NULL )
, mSupportsFormatDetection( 0 )
, mCurrentlyCapturing( false )
, mNewFrame{ false }
, mReadSurface{ false }
, m_refCount{ 1 }
, mVideoFrame{}
, mCurrentSize{ 1920, 1080 }
, mCurrentFps{ 23.98f }
, mReadFrameCallback{}
{
	mDecklink->AddRef();

	IDeckLinkAttributes* deckLinkAttributes = NULL;
	IDeckLinkDisplayModeIterator* displayModeIterator = NULL;
	IDeckLinkDisplayMode* displayMode = NULL;
	bool result = false;

	while( mModesList.size() > 0 ) {
		mModesList.back()->Release();
		mModesList.pop_back();
	}

	// Get the IDeckLinkInput for the selected device
	if( mDecklink->QueryInterface( IID_IDeckLinkInput, (void**)&mDecklinkInput ) != S_OK ) {
		mDecklinkInput = NULL;
		throw std::exception{ "This application was unable to obtain IDeckLinkInput for the selected device." };
	}

	// Retrieve and cache mode list
	if( mDecklinkInput->GetDisplayModeIterator( &displayModeIterator ) == S_OK ) {
		while( displayModeIterator->Next( &displayMode ) == S_OK )
			mModesList.push_back( displayMode );

		displayModeIterator->Release();
	}

	//
	// Check if input mode detection format is supported.

	mSupportsFormatDetection = false; // assume unsupported until told otherwise
	if( device->QueryInterface( IID_IDeckLinkAttributes, (void**)&deckLinkAttributes ) == S_OK ) {
		BOOL support = 0;
		if( deckLinkAttributes->GetFlag( BMDDeckLinkSupportsInputFormatDetection, &support ) == S_OK )
			mSupportsFormatDetection = ( support != 0 );
		deckLinkAttributes->Release();
	}
}

DeckLinkDevice::~DeckLinkDevice()
{
	Stop();
	Cleanup();
}

void DeckLinkDevice::Cleanup()
{
	if( mDecklinkInput != NULL ) {
		mDecklinkInput->Release();
		mDecklinkInput = NULL;
	}

	if( mDecklink != NULL ) {
		mDecklink->Release();
		mDecklink = NULL;
	}
}

void DeckLinkDevice::ReadFrameCallback( std::function<void( VideoFrameBGRA * frame )> callback )
{
	mReadFrameCallback = callback;
}

std::vector<std::string> DeckLinkDevice::GetDisplayModeNames() {
	std::vector<std::string> modeNames;
	int modeIndex;
	BSTR modeName;

	for( modeIndex = 0; modeIndex < mModesList.size(); modeIndex++ ) {
		if( mModesList[modeIndex]->GetName( &modeName ) == S_OK ) {
			check( modeName != NULL );
			std::wstring ws( modeName, SysStringLen( modeName ) );
			modeNames.push_back( ws2s( ws ) );
		}
		else {
			modeNames.push_back( "Unknown mode" );
		}
	}

	return modeNames;
}

bool DeckLinkDevice::IsFormatDetectionEnabled()
{
	return mSupportsFormatDetection;
}

bool DeckLinkDevice::IsCapturing()
{
	return mCurrentlyCapturing;
}

std::string DeckLinkDevice::GetDisplayModeString( BMDDisplayMode mode )
{
	switch( mode ) {
	case bmdModeNTSC:			return "Mode NTSC";
	case bmdModeNTSC2398:		return "Mode NTSC2398";
	case bmdModePAL:			return "Mode PAL";
	case bmdModeNTSCp:			return "Mode NTSCp";
	case bmdModePALp:			return "Mode PALp";
	case bmdModeHD1080p2398:	return "Mode HD1080p2398";
	case bmdModeHD1080p24:		return "Mode HD1080p24";
	case bmdModeHD1080p25:		return "Mode HD1080p25";
	case bmdModeHD1080p2997:	return "Mode HD1080p2997";
	case bmdModeHD1080p30:		return "Mode HD1080p30";
	case bmdModeHD1080i50:		return "Mode HD1080i50";
	case bmdModeHD1080i5994:	return "Mode HD1080i5994";
	case bmdModeHD1080i6000:	return "Mode HD1080i6000";
	case bmdModeHD1080p50:		return "Mode HD1080p50";
	case bmdModeHD1080p5994:	return "Mode HD1080p5994";
	case bmdModeHD1080p6000:	return "Mode HD1080p6000";
	case bmdModeHD720p50:		return "Mode HD720p50";
	case bmdModeHD720p5994:		return "Mode HD720p5994";
	case bmdModeHD720p60:		return "Mode HD720p60";
	case bmdMode2k2398:			return "Mode 2k2398";
	case bmdMode2k24:			return "Mode 2k24";
	case bmdMode2k25:			return "Mode 2k25";
	case bmdMode2kDCI2398:		return "Mode 2kDCI2398";
	case bmdMode2kDCI24:		return "Mode 2kDCI24";
	case bmdMode2kDCI25:		return "Mode 2kDCI25";
	case bmdMode4K2160p2398:	return "Mode 4K2160p2398";
	case bmdMode4K2160p24:		return "Mode 4K2160p24";
	case bmdMode4K2160p25:		return "Mode 4K2160p25";
	case bmdMode4K2160p2997:	return "Mode 4K2160p2997";
	case bmdMode4K2160p30:		return "Mode 4K2160p30";
	case bmdMode4K2160p50:		return "Mode 4K2160p50";
	case bmdMode4K2160p5994:	return "Mode 4K2160p5994";
	case bmdMode4K2160p60:		return "Mode 4K2160p60";
	case bmdMode4kDCI2398:		return "Mode 4kDCI2398";
	case bmdMode4kDCI24:		return "Mode 4kDCI24";
	case bmdMode4kDCI25:		return "Mode 4kDCI25";
	case bmdModeUnknown:		return "Mode Unknown";
	default:					return "";
	}
}

FIntPoint DeckLinkDevice::GetDisplayModeBufferSize( BMDDisplayMode mode )
{
	for( const auto& decklinkMode : mModesList ) {
		if( decklinkMode->GetDisplayMode() == mode ) {
			return FIntPoint( (int32)decklinkMode->GetWidth(), (int32)decklinkMode->GetHeight() );
		}
	}
	UE_LOG( LogDeckLinkMedia, Error, TEXT( "No corresponding display mode found, returning zero resolution." ) );
	return FIntPoint( 0, 0 );
}

float DeckLinkDevice::GetDisplayModeBufferFps( BMDDisplayMode mode )
{
	if( mode == bmdModeNTSC2398
		|| mode == bmdModeNTSC
		|| mode == bmdModeNTSCp
		|| mode == bmdModePAL
		|| mode == bmdModePALp
		|| mode == bmdModeHD1080p2398 
		|| mode == bmdMode2k2398
		|| mode == bmdMode2kDCI2398
		|| mode == bmdMode4K2160p2398
		|| mode == bmdMode4kDCI2398 )
	{
		return 23.98f;
	}
	else if( mode == bmdModeHD1080p24
		|| mode == bmdMode2k24
		|| mode == bmdMode2kDCI24 
		|| mode == bmdMode4K2160p24 
		|| mode == bmdMode4kDCI24 )
	{
		return 24.0f;
	}
	else if( mode == bmdModeHD1080p25
		|| mode == bmdMode2k25
		|| mode == bmdMode2kDCI25
		|| mode == bmdMode4K2160p25
		|| mode == bmdMode4kDCI25 )
	{
		return 25.0f;
	}
	else if( mode == bmdModeHD1080p30
		|| mode == bmdMode4K2160p30 )
	{
		return 30.0f;
	}
	else if( mode == bmdModeHD720p50
		|| mode == bmdModeHD1080i50
		|| mode == bmdModeHD1080p50 )
	{
		return 50.0f;
	}
	else if( mode == bmdModeHD720p5994
		|| mode == bmdModeHD1080i5994
		|| mode == bmdModeHD1080p5994 )
	{
		return 59.94f;
	}
	else if( mode == bmdModeHD720p60
		|| mode == bmdModeHD1080i6000
		|| mode == bmdModeHD1080p6000 
		|| mode == bmdMode4K2160p60 )
	{
		return 60.0f;
	}
	else if( mode == bmdMode4K2160p2997 ){
		return 29.97f;
	}

	return 0.0f;
}

bool DeckLinkDevice::Start( int videoModeIndex ) {
	// Get the IDeckLinkDisplayMode from the given index
	if( (videoModeIndex < 0) || (videoModeIndex >= mModesList.size()) ) {
		UE_LOG( LogDeckLinkMedia, Error, TEXT( "An invalid display mode was selected." ) );
		return false;
	}

	return Start( mModesList[videoModeIndex]->GetDisplayMode() );
}

bool DeckLinkDevice::Start( BMDDisplayMode videoMode )
{
	BMDVideoInputFlags videoInputFlags = bmdVideoInputFlagDefault;
	if( mSupportsFormatDetection )
		videoInputFlags |= bmdVideoInputEnableFormatDetection;

	// Set the video input mode
	if( mDecklinkInput->EnableVideoInput( videoMode, bmdFormat8BitYUV, videoInputFlags ) != S_OK ) {
		UE_LOG( LogDeckLinkMedia, Error, TEXT( "This application was unable to select the chosen video mode. Perhaps, the selected device is currently in-use." ) );
		return false;
	}

	// Start the capture
	if( mDecklinkInput->StartStreams() != S_OK ) {
		UE_LOG( LogDeckLinkMedia, Error, TEXT( "This application was unable to start the capture. Perhaps, the selected device is currently in-use." ) );
		return false;
	}

	// Set capture callback
	mDecklinkInput->SetCallback( this );

	mCurrentFps = GetDisplayModeBufferFps( videoMode );
	mCurrentSize = GetDisplayModeBufferSize( videoMode );
	mCurrentlyCapturing = true;
	return true;
}

void DeckLinkDevice::Stop()
{
	if( ! mCurrentlyCapturing )
		return;

	if( mDecklinkInput != NULL ) {
		mDecklinkInput->StopStreams();
		mDecklinkInput->SetCallback( NULL );
	}

	mCurrentlyCapturing = false;
}

HRESULT DeckLinkDevice::VideoInputFormatChanged(/* in */ BMDVideoInputFormatChangedEvents notificationEvents, /* in */ IDeckLinkDisplayMode *newMode, /* in */ BMDDetectedVideoInputFormatFlags detectedSignalFlags ) {

	QUICK_SCOPE_CYCLE_COUNTER( STAT_DeckLinkDevice_VideoInputFormatChanged );

	unsigned int	modeIndex = 0;
	BMDPixelFormat	pixelFormat = bmdFormat10BitYUV;

	// Restart capture with the new video mode if told to
	if( ! mSupportsFormatDetection )
		return S_OK;

	if( detectedSignalFlags & bmdDetectedVideoInputRGB444 )
		pixelFormat = bmdFormat10BitRGB;

	// Stop the capture
	mDecklinkInput->StopStreams();

	// Set the video input mode
	if( mDecklinkInput->EnableVideoInput( newMode->GetDisplayMode(), pixelFormat, bmdVideoInputEnableFormatDetection ) != S_OK )
	{
		// Let the UI know we couldnt restart the capture with the detected input mode
		UE_LOG( LogDeckLinkMedia, Error, TEXT( "This application was unable to select the new video mode." ) );
		return S_OK;
	}

	// Start the capture
	if( mDecklinkInput->StartStreams() != S_OK )
	{
		// Let the UI know we couldnt restart the capture with the detected input mode
		UE_LOG( LogDeckLinkMedia, Error, TEXT( "This application was unable to start the capture on the selected device." ) );
		return S_OK;
	}

	mCurrentFps = GetDisplayModeBufferFps( newMode->GetDisplayMode() );
	mCurrentSize = FIntPoint( newMode->GetWidth(), newMode->GetHeight() );
	return S_OK;
}

HRESULT DeckLinkDevice::VideoInputFrameArrived( IDeckLinkVideoInputFrame* frame, IDeckLinkAudioInputPacket* audioPacket )
{
	QUICK_SCOPE_CYCLE_COUNTER( STAT_DeckLinkDevice_VideoInputFrameArrived );

	if( frame == NULL )
		return S_OK;

	if( ( frame->GetFlags() & bmdFrameHasNoInputSource ) == 0 ) {

		std::lock_guard<std::mutex> lock( mMutex );

		// Get the various timecodes and userbits for this frame
		//GetAncillaryDataFromFrame( frame, bmdTimecodeVITC, mTimecode.vitcF1Timecode, mTimecode.vitcF1UserBits );
		//GetAncillaryDataFromFrame( frame, bmdTimecodeVITCField2, mTimecode.vitcF2Timecode, mTimecode.vitcF2UserBits );
		//GetAncillaryDataFromFrame( frame, bmdTimecodeRP188VITC1, mTimecode.rp188vitc1Timecode, mTimecode.rp188vitc1UserBits );
		//GetAncillaryDataFromFrame( frame, bmdTimecodeRP188LTC, mTimecode.rp188ltcTimecode, mTimecode.rp188ltcUserBits );
		//GetAncillaryDataFromFrame( frame, bmdTimecodeRP188VITC2, mTimecode.rp188vitc2Timecode, mTimecode.rp188vitc2UserBits );

		mVideoFrame = VideoFrameBGRA{ frame->GetWidth(), frame->GetHeight() };
		mDeviceDiscovery->GetConverter()->ConvertFrame( frame, &mVideoFrame );
		if( mReadFrameCallback ) {
			mReadFrameCallback( &mVideoFrame );
		}
		mNewFrame = true;
		return S_OK;
	}
	return S_FALSE;
}

void DeckLinkDevice::GetAncillaryDataFromFrame( IDeckLinkVideoInputFrame* videoFrame, BMDTimecodeFormat timecodeFormat, std::string& timecodeString, std::string& userBitsString ) {
	IDeckLinkTimecode* timecode = NULL;
	BSTR timecodeCFString;
	BMDTimecodeUserBits userBits = 0;

	if( (videoFrame != NULL)
		&& (videoFrame->GetTimecode( timecodeFormat, &timecode ) == S_OK) ) {
		if( timecode->GetString( &timecodeCFString ) == S_OK ) {
			check( timecodeCFString != NULL );
			timecodeString = ws2s( std::wstring( timecodeCFString, SysStringLen( timecodeCFString ) ) );
		}
		else {
			timecodeString = "";
		}

		timecode->GetTimecodeUserBits( &userBits );
		userBitsString = "0x" + userBits ;
		timecode->Release();
	}
	else {
		timecodeString = "";
		userBitsString = "";
	}
}

DeckLinkDevice::Timecodes DeckLinkDevice::GetTimecode() const
{
	std::lock_guard<std::mutex> lock( mMutex );
	return mTimecode;
}

bool DeckLinkDevice::GetFrame( VideoFrameBGRA* frame, Timecodes * timecodes )
{
	if( ! mNewFrame )
		return false;

	mNewFrame = false;

	std::lock_guard<std::mutex> lock( mMutex );
	*frame = mVideoFrame;
	
	if( timecodes )
		*timecodes = mTimecode;

	return true;
}

HRESULT	STDMETHODCALLTYPE DeckLinkDevice::QueryInterface( REFIID iid, LPVOID *ppv )
{
	HRESULT			result = E_NOINTERFACE;

	if( ppv == NULL )
		return E_INVALIDARG;

	// Initialise the return result
	*ppv = NULL;

	// Obtain the IUnknown interface and compare it the provided REFIID
	if( iid == IID_IUnknown )
	{
		*ppv = this;
		AddRef();
		result = S_OK;
	}
	else if( iid == IID_IDeckLinkInputCallback )
	{
		*ppv = (IDeckLinkInputCallback*)this;
		AddRef();
		result = S_OK;
	}
	else if( iid == IID_IDeckLinkNotificationCallback )
	{
		*ppv = (IDeckLinkNotificationCallback*)this;
		AddRef();
		result = S_OK;
	}

	return result;
}

ULONG STDMETHODCALLTYPE DeckLinkDevice::AddRef( void )
{
	return InterlockedIncrement( (LONG*)&m_refCount );
}

ULONG STDMETHODCALLTYPE DeckLinkDevice::Release( void )
{
	int		newRefValue;

	newRefValue = InterlockedDecrement( (LONG*)&m_refCount );
	if( newRefValue == 0 )
	{
		delete this;
		return 0;
	}

	return newRefValue;
}

#include "Runtime/Core/Public/Windows/HideWindowsPlatformAtomics.h"
#include "Runtime/Core/Public/Windows/HideWindowsPlatformTypes.h"
