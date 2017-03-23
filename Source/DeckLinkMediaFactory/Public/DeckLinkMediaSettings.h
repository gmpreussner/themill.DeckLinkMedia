// Copyright 2017 The Mill, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"

#include "DeckLinkMediaSettings.generated.h"

UCLASS(config=Engine)
class DECKLINKMEDIAFACTORY_API UDeckLinkMediaSettings
	: public UObject
{
	GENERATED_BODY()

public:
	 
	/** Default constructor. */
	UDeckLinkMediaSettings();
};
