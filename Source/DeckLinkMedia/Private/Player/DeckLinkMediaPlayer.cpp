// Copyright 2017 The Mill, Inc. All Rights Reserved.

#include "DeckLinkMediaPrivate.h"
#include "DeckLinkMediaPlayer.h"

#include "HAL/FileManager.h"
#include "IMediaOptions.h"
#include "IMediaTextureSink.h"
#include "IMediaBinarySink.h"
#include "Misc/ScopeLock.h"

#include "DeckLink/DecklinkDevice.h"


#define LOCTEXT_NAMESPACE "FDeckLinkMediaPlayer"


/* FDeckLinkMediaPlayer structors
 *****************************************************************************/

FDeckLinkMediaPlayer::FDeckLinkMediaPlayer( const TMap<uint8, TUniquePtr<DeckLinkDevice>>* Devices )
	: VideoSink( nullptr )
	, BinarySink( nullptr )
	, SelectedAudioTrack( INDEX_NONE )
	, SelectedMetadataTrack( INDEX_NONE )
	, SelectedVideoTrack( INDEX_NONE )
	, CurrentDim( FIntPoint::ZeroValue )
	, CurrentFps( 0.0 )
	, DeviceMap( Devices )
	, CurrentDeviceIndex( 0 )
	, Paused( false )
{
	
}


FDeckLinkMediaPlayer::~FDeckLinkMediaPlayer()
{
	Close();
}


/* IMediaControls interface
 *****************************************************************************/

FTimespan FDeckLinkMediaPlayer::GetDuration() const
{
	return (CurrentState == EMediaState::Playing) ? FTimespan::MaxValue() : FTimespan::Zero();
}


float FDeckLinkMediaPlayer::GetRate() const
{
	return (CurrentState == EMediaState::Playing) ? 1.0f : 0.0f;
}


EMediaState FDeckLinkMediaPlayer::GetState() const
{
	return CurrentState;
}


TRange<float> FDeckLinkMediaPlayer::GetSupportedRates(EMediaPlaybackDirections Direction, bool Unthinned) const
{
	return TRange<float>( 1.0f );
}


FTimespan FDeckLinkMediaPlayer::GetTime() const
{
	return CurrentTime;
}


bool FDeckLinkMediaPlayer::IsLooping() const
{
	return false; // not supported
}


bool FDeckLinkMediaPlayer::Seek(const FTimespan& Time)
{
	return false; // not supported
}


bool FDeckLinkMediaPlayer::SetLooping(bool Looping)
{
	return false; // not supported
}


bool FDeckLinkMediaPlayer::SetRate(float Rate)
{
	if( Rate == 0.0f )
	{
		Paused = true;
	}
	else if( Rate == 1.0f )
	{
		Paused = false;
	}
	else
	{
		return false;
	}

	return true;
}


bool FDeckLinkMediaPlayer::SupportsRate(float Rate, bool Unthinned) const
{
	return (Rate == 1.0f);
}


bool FDeckLinkMediaPlayer::SupportsScrubbing() const
{
	return false;
}


bool FDeckLinkMediaPlayer::SupportsSeeking() const
{
	return false;
}


/* IMediaPlayer interface
 *****************************************************************************/

void FDeckLinkMediaPlayer::Close()
{
	{
		FScopeLock Lock(&CriticalSection);
		const auto& Device = (*DeviceMap)[CurrentDeviceIndex];
		if( Device && Device->IsCapturing() ) {
			Device->Stop();
			Device->ReadFrameCallback( nullptr );
		}

		CurrentFps = 0.0f;
		CurrentState = EMediaState::Closed;
		CurrentUrl.Empty();
		CurrentDim = FIntPoint::ZeroValue;

		SelectedAudioTrack = INDEX_NONE;
		SelectedMetadataTrack = INDEX_NONE;
		SelectedVideoTrack = INDEX_NONE;
	}

	MediaEvent.Broadcast(EMediaEvent::TracksChanged);
	MediaEvent.Broadcast(EMediaEvent::MediaClosed);
}


IMediaControls& FDeckLinkMediaPlayer::GetControls()
{
	return *this;
}


FString FDeckLinkMediaPlayer::GetInfo() const
{
	return Info;
}


FName FDeckLinkMediaPlayer::GetName() const
{
	static FName PlayerName(TEXT("DeckLinkMedia"));
	return PlayerName;
}


IMediaOutput& FDeckLinkMediaPlayer::GetOutput()
{
	return *this;
}


FString FDeckLinkMediaPlayer::GetStats() const
{
	FString StatsString;
	{
		StatsString += TEXT("not implemented yet");
		StatsString += TEXT("\n");
	}

	return StatsString;
}


IMediaTracks& FDeckLinkMediaPlayer::GetTracks()
{
	return *this;
}


FString FDeckLinkMediaPlayer::GetUrl() const
{
	return CurrentUrl;
}


bool FDeckLinkMediaPlayer::Open( const FString& Url, const IMediaOptions& Options )
{
	if( Url.IsEmpty() || !Url.StartsWith( TEXT( "sdi://" ) ) )
	{
		return false;
	}

	const TCHAR* DeviceNumberStr = &Url[12];
	auto NewIndex = FCString::Atoi( DeviceNumberStr ) - 1;
	if( NewIndex < 0 || NewIndex >= DeviceMap->Num() ) {
		UE_LOG( LogDeckLinkMedia, Error, TEXT( "Invalid device id." ) );
		return false;
	}
	CurrentDeviceIndex = NewIndex;

	Close();

	const auto& Device = (*DeviceMap)[CurrentDeviceIndex];
	auto Mode = BMDDisplayMode::bmdModeHD1080p2398;
	Device->Start( Mode );

	// finalize
	{
		FScopeLock Lock(&CriticalSection);
		CurrentDim = Device->GetCurrentSize();
		CurrentFps = Device->GetCurrentFps();
		CurrentState = EMediaState::Stopped;
		CurrentUrl = Url;
	}

	// notify listeners
	MediaEvent.Broadcast(EMediaEvent::TracksChanged);
	MediaEvent.Broadcast(EMediaEvent::MediaOpened);

	return true;
}


bool FDeckLinkMediaPlayer::Open(const TSharedRef<FArchive, ESPMode::ThreadSafe>& Archive, const FString& OriginalUrl, const IMediaOptions& Options)
{
	return false; // not supported
}


void FDeckLinkMediaPlayer::TickPlayer(float DeltaTime)
{
}


void FDeckLinkMediaPlayer::TickVideo(float DeltaTime)
{
	QUICK_SCOPE_CYCLE_COUNTER( STAT_DeckLinkMediaPlayer_TickVideo );

	if( Paused || ! VideoSink )
		return;

	DeckLinkDevice::VideoFrameBGRA frame;
	auto newFrame = (*DeviceMap)[CurrentDeviceIndex]->GetFrame( &frame );
	if( newFrame ) {
		auto LastBufferDim = FIntPoint( frame.GetWidth(), frame.GetHeight() );
		auto LastVideoDim = LastBufferDim;
		FScopeLock Lock( &CriticalSection );
		if( VideoSink->GetTextureSinkDimensions() != LastVideoDim ) {
			if( !VideoSink->InitializeTextureSink( LastVideoDim, LastBufferDim, EMediaTextureSinkFormat::CharBGRA, EMediaTextureSinkMode::Unbuffered ) ) {
				return;
			}
		}
		VideoSink->UpdateTextureSinkBuffer( frame.data(), frame.GetRowBytes() );
		VideoSink->DisplayTextureSinkBuffer( FTimespan{} );
	}

	CurrentTime += DeltaTime;
}


/* IMediaOutput interface
 *****************************************************************************/

void FDeckLinkMediaPlayer::SetAudioSink(IMediaAudioSink* Sink)
{
	// not supported
}


void FDeckLinkMediaPlayer::SetMetadataSink(IMediaBinarySink* Sink)
{
	if( Sink == BinarySink )
	{
		return;
	}

	FScopeLock Lock( &CriticalSection );

	if( BinarySink != nullptr )
	{
		BinarySink->ShutdownBinarySink();
	}

	BinarySink = Sink;

	if( Sink != nullptr )
	{
		Sink->InitializeBinarySink();
	}
}


void FDeckLinkMediaPlayer::SetOverlaySink(IMediaOverlaySink* Sink)
{
	// not supported
}


void FDeckLinkMediaPlayer::SetVideoSink(IMediaTextureSink* Sink)
{
	if (Sink == VideoSink)
	{
		return;
	}

	FScopeLock Lock(&CriticalSection);

	if (VideoSink != nullptr)
	{
		VideoSink->ShutdownTextureSink();
	}

	VideoSink = Sink;

	if (Sink != nullptr)
	{
		Sink->InitializeTextureSink(CurrentDim, CurrentDim, EMediaTextureSinkFormat::CharBGRA, EMediaTextureSinkMode::Unbuffered);
	}
}


/* IMediaTracks interface
 *****************************************************************************/

uint32 FDeckLinkMediaPlayer::GetAudioTrackChannels(int32 TrackIndex) const
{
	return 0; // not supported
}


uint32 FDeckLinkMediaPlayer::GetAudioTrackSampleRate(int32 TrackIndex) const
{
	return 0; // not supported
}


int32 FDeckLinkMediaPlayer::GetNumTracks(EMediaTrackType TrackType) const
{
	if ( (*DeviceMap)[CurrentDeviceIndex] != nullptr ) // @todo fixme
	{
		if (TrackType == EMediaTrackType::Video)
		{
			return 1;
		}
	}

	return 0;
}


int32 FDeckLinkMediaPlayer::GetSelectedTrack(EMediaTrackType TrackType) const
{
	if( (*DeviceMap)[CurrentDeviceIndex] == nullptr )
	{
		return INDEX_NONE;
	}

	switch( TrackType )
	{
	case EMediaTrackType::Audio:
	case EMediaTrackType::Metadata:
	case EMediaTrackType::Video:
		return 0;

	default:
		return INDEX_NONE;
	}
}


FText FDeckLinkMediaPlayer::GetTrackDisplayName(EMediaTrackType TrackType, int32 TrackIndex) const
{
	if( (*DeviceMap)[CurrentDeviceIndex] == nullptr || (TrackIndex != 0) )
	{
		return FText::GetEmpty();
	}

	switch( TrackType )
	{
	case EMediaTrackType::Audio:
		return LOCTEXT( "DefaultAudioTrackName", "Audio Track" );

	case EMediaTrackType::Metadata:
		return LOCTEXT( "DefaultMetadataTrackName", "Metadata Track" );

	case EMediaTrackType::Video:
		return LOCTEXT( "DefaultVideoTrackName", "Video Track" );

	default:
		return FText::GetEmpty();
	}
}


FString FDeckLinkMediaPlayer::GetTrackLanguage(EMediaTrackType TrackType, int32 TrackIndex) const
{
	// @todo fixme
	if ((true) || (TrackIndex != 0))
	{
		return FString();
	}

	return TEXT("und");
}


FString FDeckLinkMediaPlayer::GetTrackName(EMediaTrackType TrackType, int32 TrackIndex) const
{
	return FString();
}


uint32 FDeckLinkMediaPlayer::GetVideoTrackBitRate(int32 TrackIndex) const
{
	return 0; // @todo fixme
}


FIntPoint FDeckLinkMediaPlayer::GetVideoTrackDimensions(int32 TrackIndex) const
{
	// @todo fixme
	if ((true) || (TrackIndex != 0))
	{
		return FIntPoint::ZeroValue;
	}

	return CurrentDim;
}


float FDeckLinkMediaPlayer::GetVideoTrackFrameRate(int32 TrackIndex) const
{
	// @todo fixme
	if ((true) || (TrackIndex != 0))
	{
		return 0;
	}

	return CurrentFps;
}


bool FDeckLinkMediaPlayer::SelectTrack(EMediaTrackType TrackType, int32 TrackIndex)
{
	if ((TrackIndex != INDEX_NONE) && (TrackIndex != 0))
	{
		return false;
	}

	if (TrackType == EMediaTrackType::Video)
	{
		SelectedVideoTrack = TrackIndex;
	}
	else
	{
		return false;
	}

	return true;
}

#undef LOCTEXT_NAMESPACE
