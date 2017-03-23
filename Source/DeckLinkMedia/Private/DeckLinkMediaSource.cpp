// Copyright 2017 The Mill, Inc. All Rights Reserved.

#include "DeckLinkMediaPrivate.h"
#include "DeckLinkMediaSource.h"

#include "Misc/Paths.h"


/* UDeckLinkMediaSource structors
 *****************************************************************************/

UDeckLinkMediaSource::UDeckLinkMediaSource()
	: DeviceId( 1 )
{ }

/* UDeckLinkMediaSource interface
 *****************************************************************************/

void UDeckLinkMediaSource::SetDeviceId( const uint8& Id )
{
	DeviceId = Id;
}

/* UMediaSource interface
 *****************************************************************************/

FString UDeckLinkMediaSource::GetUrl() const
{
	return FString::Printf( TEXT( "sdi://device%d" ), DeviceId );
}


bool UDeckLinkMediaSource::Validate() const
{
	return true;
}
