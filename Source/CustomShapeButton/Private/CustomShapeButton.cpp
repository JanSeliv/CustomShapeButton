﻿// Copyright (c) Yevhenii Selivanov.

#include "CustomShapeButton.h"
//---
#include "SCustomShapeButton.h"
//---
#include "Components/ButtonSlot.h"
#include "Framework/SlateDelegates.h"
//---
#include UE_INLINE_GENERATED_CPP_BY_NAME(CustomShapeButton)

// Default constructor
UCustomShapeButton::UCustomShapeButton()
{
	SetClickMethod(EButtonClickMethod::PreciseClick);
}

// Returns the slate shape button
TSharedPtr<SCustomShapeButton> UCustomShapeButton::GetSlateCustomShapeButton() const
{
	return StaticCastSharedPtr<SCustomShapeButton>(MyButton);
}

// Forces to update the Raw Colors (pixels data) about current image
void UCustomShapeButton::ForceUpdateImage()
{
	if (const TSharedPtr<SCustomShapeButton> CustomShapeButton = GetSlateCustomShapeButton())
	{
		CustomShapeButton->ForceUpdateImage();
	}
}

// Is called when the underlying SWidget needs to be constructed
TSharedRef<SWidget> UCustomShapeButton::RebuildWidget()
{
	const TSharedRef<SCustomShapeButton> NewButtonRef = SNew(SCustomShapeButton)
											 .OnClicked(BIND_UOBJECT_DELEGATE(FOnClicked, SlateHandleClicked))
											 .OnPressed(BIND_UOBJECT_DELEGATE(FSimpleDelegate, SlateHandlePressed))
											 .OnReleased(BIND_UOBJECT_DELEGATE(FSimpleDelegate, SlateHandleReleased))
											 .OnHovered_UObject(this, &ThisClass::SlateHandleHovered)
											 .OnUnhovered_UObject(this, &ThisClass::SlateHandleUnhovered)
											 .ButtonStyle(&GetStyle())
											 .ClickMethod(GetClickMethod())
											 .TouchMethod(GetTouchMethod())
											 .IsFocusable(GetIsFocusable());
	MyButton = NewButtonRef;

	if (GetChildrenCount())
	{
		if (UButtonSlot* ButtonSlot = Cast<UButtonSlot>(GetContentSlot()))
		{
			ButtonSlot->BuildSlot(NewButtonRef);
		}
	}

	return NewButtonRef;
}
