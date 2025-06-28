// Copyright (c) Yevhenii Selivanov

#pragma once

#include "Subsystems/EngineSubsystem.h"
//---
#include "CustomShapeButtonManager.generated.h"

class SCustomShapeButton;
class UCustomShapeButton;
class FReply;

/**
 * Manages all Custom Shape Buttons during the game.
 * Each button automatically registers itself in the manager when created and unregisters when destroyed.
 * One of the main purposes of this manager is to handle overlap events and redirect them to underlying buttons.
 */
UCLASS()
class CUSTOMSHAPEBUTTON_API UCustomShapeButtonManager : public UEngineSubsystem
{
	GENERATED_BODY()

public:
	/** Returns the manager instance, or crash if can't be obtained. */
	static UCustomShapeButtonManager& Get();

	/** Returns the manager instance, or nullptr if can't be obtained. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "C++")
	static UCustomShapeButtonManager* GetCustomShapeButtonManager();

	/** Registers a button for event redirection */
	UFUNCTION(BlueprintCallable, Category = "C++")
	void RegisterButton(UCustomShapeButton* Button);

	/** Unregisters a button when it is destroyed */
	UFUNCTION(BlueprintCallable, Category = "C++")
	void UnregisterButton(UCustomShapeButton* Button);

	/** Returns true is button is initialized and ready to handle events. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "C++")
	static bool CanRegisterButton(const UCustomShapeButton* Button);

	/** Redirects given event with its callback to appropriate button.
	 * @param Event The pointer event to handle.
	 * @param Callback The callback function to call on the button.
	 * @return Returns FReply::Handled() if the event was handled, otherwise FReply::Unhandled(). */
	FReply HandleEvent(const struct FPointerEvent& Event, const TFunctionRef<FReply(const TSharedRef<SCustomShapeButton>&)>& Callback);

	/*********************************************************************************************
	 * Data
	 ********************************************************************************************* */
protected:
	/** Stores all active buttons.
	 * Is Soft to allow garbage collection. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadWrite, Transient, AdvancedDisplay, meta = (BlueprintProtected, DisplayName = "All Hidden Widgets"))
	TArray<TSoftObjectPtr<UCustomShapeButton>> RegisteredButtons;

	/*********************************************************************************************
	 * Overrides
	 ********************************************************************************************* */
protected:
	/** Called when this subsystem is initialized to perform initial setup. */
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/** Is called on the game world end play to cleanup data. */
	void OnEndPlay(UWorld* World, bool bArg, bool bCond);
};

/** 
 * Forwards an input event to the correct button via manager routing.
 * @param CallExpr Full parent call
 * e.g: HANDLE_EVENT(SButton::OnMouseMove(MyGeometry, MouseEvent))
 */
#define HANDLE_EVENT(CallExpr) \
	UCustomShapeButtonManager::Get().HandleEvent(MouseEvent, \
		[&](const TSharedRef<SCustomShapeButton>& SButton) \
		{ \
			return SButton->CallExpr; \
		})
