// Copyright (c) Yevhenii Selivanov.

#pragma once

#include "Components/Button.h"
//---
#include "CustomShapeButton.generated.h"

class SCustomShapeButton;

/**
 * Custom shape button.
 * To make it work:
 * - ave an image with alpha pixels
 * - set an image as for regular button under the 'Appearance' -> 'Style' category
 * Is wrapper around the SCustomShapeButton widget.
 * @see SCustomShapeButton
 */
UCLASS()
class CUSTOMSHAPEBUTTON_API UCustomShapeButton : public UButton
{
	GENERATED_BODY()

public:
	/** Default constructor. */
	UCustomShapeButton();

	/** Returns the slate shape button. */
	TSharedPtr<SCustomShapeButton> GetSlateCustomShapeButton() const;

	/** Forces to update the Raw Colors (pixels data) about current image.
	 * @warning Is not recommended to use at all since often updates are expensive.
	 * However, can be useful if button changes in runtime (new texture set or material is changing dynamically).
	 * By default, image is cached only once at the beginning. */
	UFUNCTION(BlueprintCallable, Category = "Custom Shape Button")
	void ForceUpdateImage();

	/** Defines the button's Z-order priority for event handling.
	 * Higher values mean the button is visually and interactively above others.
	 * Used during registration to ensure correct overlap behavior. */
	UPROPERTY(EditAnywhere, Category = "CustomShape")
	int32 OverlapOrder = 0;

protected:
	/** Is called when the underlying SWidget needs to be constructed. */
	virtual TSharedRef<SWidget> RebuildWidget() override;

	/** Is called when the underlying SWidget needs to be destroyed. */
	virtual auto ReleaseSlateResources(bool bReleaseChildren) -> void override;
};
