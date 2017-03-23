// Copyright 2017 The Mill, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MediaSource.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ScriptMacros.h"

#include "DeckLinkMediaSource.generated.h"


/**
 * Media source for EXR image sequences.
 */
UCLASS(BlueprintType, hidecategories=(Overrides, Playback))
class DECKLINKMEDIA_API UDeckLinkMediaSource
	: public UMediaSource
{
	GENERATED_BODY()

public:
	/** Default constructor. */
	UDeckLinkMediaSource();

	/**
	 * Get the sdi device id.
	 *
	 * @return The file path.
	 * @see SetSourceDirectory
	 */
	const uint8& GetDeviceId() const
	{
		return DeviceId;
	}

	/**
	 * Set the sdi device id this source represents.
	 *
	 * @param Path The path to set.
	 * @see SourceDirectory
	 */
	UFUNCTION(BlueprintCallable, Category="DeckLinkMedia|DeckLinkMediaSource")
	void SetDeviceId(const uint8& Id);
public:

	//~ UMediaSource interface

	virtual FString GetUrl() const override;
	virtual bool Validate() const override;

protected:

	/** Sdi device id, starting at 1. */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category=SDI, meta=(ClampMin = "1.0", ClampMax = "8.0", UIMin = "1.0", UIMax = "8.0") )
	uint8 DeviceId;
};
