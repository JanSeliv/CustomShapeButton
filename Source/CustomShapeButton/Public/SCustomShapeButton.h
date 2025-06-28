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
	SCustomShapeButton();

	/** Virtual destructor, unregister data. */
	virtual ~SCustomShapeButton() override;

	/** Attempts to process the event and returns a reply.
	 * @param OutReply The reply to be filled with the result of the event handling or remains the same if event was already handled.
	 * @param Event The pointer event to handle.
	 * @param Callback The callback to invoke if the event is handled. */
	virtual void HandleEvent(FReply& OutReply, const FPointerEvent& Event, const TFunctionRef<FReply(const TSharedRef<SCustomShapeButton>&)>& Callback);

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

	/** Contains cached information about the mouse event. */
	FPointerEvent CachedPointerEvent;

	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonDoubleClick(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual void OnMouseLeave(const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseLeave_Unhovered(const FPointerEvent& MouseEvent);
	virtual void OnMouseEnter(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseEnter_Hovered(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent);

	/** Returns true if cursor is hovered on a texture or material.
	 * - Might be expensive to call frequently as it performs a full pixel check.
	 * - It does not respect the overlap order returning true regardless of other widget layer.
	 * Instead, prefer IsHovered(), which checks cached state and respects proper layering. */
	virtual bool IsAlphaPixelHovered() const;

	/** Set once on render thread the buffer data about all pixels of current image if was not set before. */
	virtual void TryUpdateRawColorsOnce();

	/** Copies the buffer data from the texture.
	 * For public access call TryUpdateRawColorsOnce instead. */
	virtual void UpdateRawColors_Texture(const class UTexture2D& Texture);

	/** Copies the buffer data from the material.
	 * For public access call TryUpdateRawColorsOnce instead. */
	virtual void UpdateRawColors_Material(class UMaterialInterface& Material);
};
