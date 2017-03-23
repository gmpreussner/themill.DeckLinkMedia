// Copyright 2017 The Mill, Inc. All Rights Reserved.

#include "DeckLinkMediaSourceFactory.h"
#include "Misc/Paths.h"
#include "DeckLinkMediaSource.h"
#include "Developer/AssetTools/Public/AssetTypeCategories.h"


/* UDeckLinkMediaSourceFactoryNew structors
*****************************************************************************/

UDeckLinkMediaSourceFactory::UDeckLinkMediaSourceFactory( const FObjectInitializer& ObjectInitializer )
	: Super( ObjectInitializer )
{
	SupportedClass = UDeckLinkMediaSource::StaticClass();
	bCreateNew = true;
	bEditAfterNew = true;
}


/* UFactory overrides
*****************************************************************************/

UObject* UDeckLinkMediaSourceFactory::FactoryCreateNew( UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn )
{
	return NewObject<UDeckLinkMediaSource>( InParent, InClass, InName, Flags );
}


uint32 UDeckLinkMediaSourceFactory::GetMenuCategories() const
{
	return EAssetTypeCategories::Media;
}


bool UDeckLinkMediaSourceFactory::ShouldShowInNewMenu() const
{
	return true;
}

