// Copyright 2017 The Mill, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IMediaControls.h"
#include "IMediaPlayer.h"
#include "IMediaOutput.h"
#include "IMediaTracks.h"

class DeckLinkDevice;

/**
 * Implements a media player EXR image sequences.
 */
class FDeckLinkMediaPlayer
	: public IMediaControls
	, public IMediaPlayer
	, public IMediaOutput
	, public IMediaTracks
{
public:

	/** Default constructor. */
	FDeckLinkMediaPlayer( const TMap<uint8, TUniquePtr<DeckLinkDevice>>* Devices );

	/** Destructor. */
	~FDeckLinkMediaPlayer();

public:
	//~ IMediaControls interface

	virtual FTimespan GetDuration() const override;
	virtual float GetRate() const override;
	virtual EMediaState GetState() const override;
	virtual TRange<float> GetSupportedRates(EMediaPlaybackDirections Direction, bool Unthinned) const override;
	virtual FTimespan GetTime() const override;
	virtual bool IsLooping() const override;
	virtual bool Seek(const FTimespan& Time) override;
	virtual bool SetLooping(bool Looping) override;
	virtual bool SetRate(float Rate) override;
	virtual bool SupportsRate(float Rate, bool Unthinned) const override;
	virtual bool SupportsScrubbing() const override;
	virtual bool SupportsSeeking() const override;

public:

	//~ IMediaPlayer interface

	virtual void Close() override;
	virtual IMediaControls& GetControls() override;
	virtual FString GetInfo() const override;
	virtual FName GetName() const override;
	virtual IMediaOutput& GetOutput() override;
	virtual FString GetStats() const override;
	virtual IMediaTracks& GetTracks() override;
	virtual FString GetUrl() const override;
	virtual bool Open(const FString& Url, const IMediaOptions& Options) override;
	virtual bool Open(const TSharedRef<FArchive, ESPMode::ThreadSafe>& Archive, const FString& OriginalUrl, const IMediaOptions& Options) override;
	virtual void TickPlayer(float DeltaTime) override;
	virtual void TickVideo(float DeltaTime) override;

	DECLARE_DERIVED_EVENT( FMfMediaPlayer, IMediaPlayer::FOnMediaEvent, FOnMediaEvent);
	virtual FOnMediaEvent& OnMediaEvent() override
	{
		return MediaEvent;
	}

public:

	//~ IMediaOutput interface

	virtual void SetAudioSink(IMediaAudioSink* Sink) override;
	virtual void SetMetadataSink(IMediaBinarySink* Sink) override;
	virtual void SetOverlaySink(IMediaOverlaySink* Sink) override;
	virtual void SetVideoSink(IMediaTextureSink* Sink) override;

public:

	//~ IMediaTracks interface

	virtual uint32 GetAudioTrackChannels(int32 TrackIndex) const override;
	virtual uint32 GetAudioTrackSampleRate(int32 TrackIndex) const override;
	virtual int32 GetNumTracks(EMediaTrackType TrackType) const override;
	virtual int32 GetSelectedTrack(EMediaTrackType TrackType) const override;
	virtual FText GetTrackDisplayName(EMediaTrackType TrackType, int32 TrackIndex) const override;
	virtual FString GetTrackLanguage(EMediaTrackType TrackType, int32 TrackIndex) const override;
	virtual FString GetTrackName(EMediaTrackType TrackType, int32 TrackIndex) const override;
	virtual uint32 GetVideoTrackBitRate(int32 TrackIndex) const override;
	virtual FIntPoint GetVideoTrackDimensions(int32 TrackIndex) const override;
	virtual float GetVideoTrackFrameRate(int32 TrackIndex) const override;
	virtual bool SelectTrack(EMediaTrackType TrackType, int32 TrackIndex) override;

private:

	/** The currently used video sink. */
	IMediaBinarySink* BinarySink;

	IMediaTextureSink* VideoSink;
private:

	/** Index of the selected audio track. */
	int32 SelectedAudioTrack;

	/** Index of the selected metadata track. */
	int32 SelectedMetadataTrack;

	/** Index of the selected video track. */
	int32 SelectedVideoTrack;

private:

	/** Critical section for synchronizing access to receiver and sinks. */
	FCriticalSection CriticalSection;

	/** Video dimensions of the last processed frame image. */
	FIntPoint CurrentDim;

	/** Frames per second of the currently opened sequence. */
	float CurrentFps;

	/** The current playback rate. */
	float CurrentRate;

	/** Current state of the media player. */
	EMediaState CurrentState;

	/** The current time of the playback. */
	FTimespan CurrentTime;

	/** The URL of the currently opened media. */
	FString CurrentUrl;

	/** The duration of the media. */
    FTimespan Duration;

	/** Media information string. */
	FString Info;

	/** Whether the player is paused. */
	bool Paused;

	/** Holds an event delegate that is invoked when a media event occurred. */
	FOnMediaEvent MediaEvent;
	
	/** Media playback state. */
	EMediaState State;

	const TMap<uint8, TUniquePtr<DeckLinkDevice>> *			DeviceMap;
	int32													CurrentDeviceIndex;
};
