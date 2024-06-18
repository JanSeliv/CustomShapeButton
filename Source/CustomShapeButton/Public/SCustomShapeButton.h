// Copyright (c) Yevhenii Selivanov

#pragma once

#include "Widgets/Input/SButton.h"
//---
#include "UObject/StrongObjectPtr.h"
#include "Engine/TextureRenderTarget2D.h"

/**
 * Implements slate button with one difference:
 * it proceed events (hover, press) if only the mouse is on the button's non-alpha pixel.
 */
class CUSTOMSHAPEBUTTON_API SCustomShapeButton : public SButton
{
public:
	/** Virtual destructor, unregister data. */
	virtual ~SCustomShapeButton() override;

	/** Allows button to be hovered. */
	virtual void SetCanHover(bool bAllow);

	/** Updates the internal texture size. */
	virtual void SetTextureSize(const FIntPoint& InSize);

	/** Forces to update the Raw Colors (pixels data) about current image.
	 * @warning Is not recommended to use at all since often updates are expensive.
	 * However, can be useful if button changes in runtime (new texture set or material is changing dynamically).
	 * By default, image is cached only once at the beginning. */
	void ForceUpdateImage();

	/** Calculates the index of the pixel under the cursor.
	 * Returns -1 if the cursor is not on the button or can't access the data. */
	uint32 GetCurrentPointIndex() const;

protected:
	/** Cached buffer data about all pixels of current texture or material, is set once on render thread. */
	TArray<FColor> RawColors;

	/** Is created once if no render target was set before, cleanups on destruction. */
	TStrongObjectPtr<UTextureRenderTarget2D> RenderTarget = nullptr;

	/** Contains the size of current texture. */
	FIntPoint TextureRes = FIntPoint::ZeroValue;

	/** Is true when is allowed button to be hovered. */
	bool bCanHover = false;

	/** Contains cached true if on last frame the button was hovered. */
	bool bIsHovered = false;

	/** Contains cached geometry of the button. */
	FGeometry CurrentGeometry;

	/** Contains cached information about the mouse event. */
	FPointerEvent CurrentMouseEvent;

	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonDoubleClick(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual void OnMouseLeave(const FPointerEvent& MouseEvent) override;
	virtual void OnMouseEnter(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;

	/** Returns true if cursor is hovered on a texture. */
	virtual bool IsAlphaPixelHovered() const;

	/** Set once on render thread the buffer data about all pixels of current image if was not set before. */
	virtual void TryUpdateRawColorsOnce();

	/** Copies the buffer data from the texture. */
	virtual void UpdateRawColors_Texture(const class UTexture2D& Texture);

	/** Copies the buffer data from the material. */
	virtual void UpdateRawColors_Material(class UMaterialInterface& Material);

	/** Try register On Hovered and On Unhovered events. */
	virtual void TryDetectOnHovered();

	/** Caching current geometry and last mouse event. */
	virtual void UpdateMouseData(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent);
};
