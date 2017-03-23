// Copyright 2017 The Mill, Inc. All Rights Reserved.

#include "DeckLinkMediaSourceFactory.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"


/**
 * Implements the DeckLinkMediaEditor module.
 */
class FDeckLinkMediaEditorModule
	: public IModuleInterface
{
public:

	//~ IModuleInterface interface

	virtual void StartupModule() override { }
	virtual void ShutdownModule() override { }
};


IMPLEMENT_MODULE(FDeckLinkMediaEditorModule, DeckLinkMediaEditor);
