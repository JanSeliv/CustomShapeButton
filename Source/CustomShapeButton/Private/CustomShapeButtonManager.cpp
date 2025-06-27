// Copyright (c) Yevhenii Selivanov

#include "CustomShapeButtonManager.h"
//---
#include "CustomShapeButton.h"
#include "SCustomShapeButton.h"
//---
#include "Engine/Engine.h"
#include "Input/Reply.h"
//---
#include UE_INLINE_GENERATED_CPP_BY_NAME(CustomShapeButtonManager)

// Returns the manager instance, or crash if can't be obtained
UCustomShapeButtonManager& UCustomShapeButtonManager::Get()
{
	UCustomShapeButtonManager* CustomShapeButtonManager = GetCustomShapeButtonManager();
	checkf(CustomShapeButtonManager, TEXT("ERROR: [%i] %hs:\n'CustomShapeButtonManager' is null!"), __LINE__, __FUNCTION__);
	return *CustomShapeButtonManager;
}

// Returns the manager instance, or nullptr if can't be obtained
UCustomShapeButtonManager* UCustomShapeButtonManager::GetCustomShapeButtonManager()
{
	return GEngine ? GEngine->GetEngineSubsystem<UCustomShapeButtonManager>() : nullptr;
}

// Registers a button for event redirection
void UCustomShapeButtonManager::RegisterButton(UCustomShapeButton* Button)
{
	const TSharedPtr<SCustomShapeButton> SButton = Button ? Button->GetSlateCustomShapeButton() : nullptr;
	if (!ensureMsgf(SButton, TEXT("ASSERT: [%i] %hs:\n'SButton' is not valid!"), __LINE__, __FUNCTION__)
		|| !CanRegisterButton(Button))
	{
		// Button can't handle events if the world is not playing or button itself if not valid
		return;
	}

	// Find insertion point to maintain descending order of OverlapOrder
	const int32 InsertIndex = RegisteredButtons.IndexOfByPredicate(
		[ZOrder = Button->OverlapOrder](const TSoftObjectPtr<UCustomShapeButton>& It)
		{
			const UCustomShapeButton* OtherButton = It.Get();
			return OtherButton && ZOrder > OtherButton->OverlapOrder;
		});

	if (InsertIndex != INDEX_NONE)
	{
		RegisteredButtons.Insert(Button, InsertIndex);
	}
	else
	{
		RegisteredButtons.Add(Button);
	}
}

// Unregisters a button when it is destroyed
void UCustomShapeButtonManager::UnregisterButton(UCustomShapeButton* Button)
{
	const int32 Index = Button ? RegisteredButtons.IndexOfByKey(Button) : INDEX_NONE;
	if (Index == INDEX_NONE)
	{
		// Is already removed or was not even registered
		return;
	}

	RegisteredButtons.RemoveAtSwap(Index);
}

// Returns true is button is initialized and ready to handle events
bool UCustomShapeButtonManager::CanRegisterButton(const UCustomShapeButton* Button)
{
#if !WITH_EDITOR
	// Manager is always ready to handle events in builds
	return true;
#else
	// Don't register preview buttons in editor (not -game)
	const bool bIsMinusGame = !GIsEditor;
	const UWorld* World = Button ? Button->GetWorld() : nullptr;
	const bool bIsGameWorld = World && World->IsGameWorld();
	return bIsMinusGame || bIsGameWorld;
#endif // WITH_EDITOR
}

// Handles any mouse event using a delegate
FReply UCustomShapeButtonManager::HandleEvent(const FPointerEvent& Event, const TFunctionRef<FReply(const TSharedRef<SCustomShapeButton>&)>& Callback)
{
	FReply FinalReply = FReply::Unhandled();

	for (const TSoftObjectPtr<UCustomShapeButton>& It : RegisteredButtons)
	{
		const UCustomShapeButton* Button = It.Get();
		SCustomShapeButton* SButton = Button ? Button->GetSlateCustomShapeButton().Get() : nullptr;
		if (SButton)
		{
			SButton->HandleEvent(/*out*/FinalReply, Event, Callback);
		}
	}

	return FinalReply;
}

/*********************************************************************************************
 * Overrides
 ********************************************************************************************* */

// Called when this subsystem is initialized to perform initial setup
void UCustomShapeButtonManager::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	FWorldDelegates::OnWorldCleanup.AddUObject(this, &ThisClass::OnEndPlay);
}

// Is called on the game world end play to cleanup data
void UCustomShapeButtonManager::OnEndPlay(UWorld* World, bool bArg, bool bCond)
{
	RegisteredButtons.Empty();
}
