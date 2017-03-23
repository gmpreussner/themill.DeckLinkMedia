// Copyright 2017 The Mill, Inc. All Rights Reserved.

#include "DeckLinkMediaPrivate.h"

#include "IDeckLinkMediaModule.h"
#include "Modules/ModuleManager.h"
#include "DeckLinkMediaPlayer.h"

#include "HAL/PlatformProcess.h"
#include "IPluginManager.h"
#include "Paths.h"

#include "DeckLink/DecklinkDevice.h"

DEFINE_LOG_CATEGORY( LogDeckLinkMedia );

#define LOCTEXT_NAMESPACE "FDeckLinkMediaModule"

/**
 * Implements the AVFMedia module.
 */
class FDeckLinkMediaModule
	: public IDeckLinkMediaModule
{
public:
	/** Default constructor. */
	FDeckLinkMediaModule() { }

	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	void DeviceArrived( IDeckLink* decklink, size_t id );
	virtual TSharedPtr<IMediaPlayer> CreatePlayer() override;
private:
	TSharedPtr<DeckLinkDeviceDiscovery> DeviceDiscovery;
	TMap<uint8, TUniquePtr<DeckLinkDevice>> DeviceMap;
};

IMPLEMENT_MODULE(FDeckLinkMediaModule, DeckLinkMedia);

void FDeckLinkMediaModule::StartupModule()
{
	using namespace std::placeholders;
	auto DeviceCallback = std::bind( &FDeckLinkMediaModule::DeviceArrived, this, _1, _2 );
	DeviceDiscovery.Reset();
	DeviceDiscovery = MakeShareable( new DeckLinkDeviceDiscovery( DeviceCallback ) );
}

void FDeckLinkMediaModule::ShutdownModule()
{
	DeviceMap.Empty();
	DeviceDiscovery.Reset();
}

void FDeckLinkMediaModule::DeviceArrived( IDeckLink* decklink, size_t id )
{
	uint8 DeviceId = static_cast<uint8>(id);
	DeviceMap.Emplace( DeviceId, MakeUnique<DeckLinkDevice>( DeviceDiscovery.Get(), decklink ) );
	UE_LOG( LogDeckLinkMedia, Log, TEXT( "Device %d arrived." ), id );

	DeviceMap.KeySort( []( uint8 A, uint8 B ) {
		return A < B;
	} );
}

TSharedPtr<IMediaPlayer> FDeckLinkMediaModule::CreatePlayer()
{
	return MakeShared<FDeckLinkMediaPlayer>( &DeviceMap );
}


#undef LOCTEXT_NAMESPACE
