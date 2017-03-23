// Copyright 2017 The Mill, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Templates/SharedPointer.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"

class IMediaPlayer;

/**
 * Interface for the DeckLinkMedia module.
 */
class IDeckLinkMediaModule
	: public IModuleInterface
{
public:

	static inline IDeckLinkMediaModule& Get()
	{
		return FModuleManager::LoadModuleChecked<IDeckLinkMediaModule>( "DeckLinkMedia" );
	}

	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded( "DeckLinkMedia" );
	}

	/**
	 * Creates a media player for SDI video feeds.
	 *
	 * @return A new media player, or nullptr if a player couldn't be created.
	 */
	virtual TSharedPtr<IMediaPlayer> CreatePlayer() = 0;
public:

	/** Virtual destructor. */
	virtual ~IDeckLinkMediaModule() { }
};
