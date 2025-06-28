// Copyright (c) Yevhenii Selivanov

#include "SCustomShapeButton.h"

// Custom Shape Button
#include "CustomShapeButtonManager.h"

// UE
#include "RenderingThread.h"
#include "RHICommandList.h"
#include "RHIResources.h"
#include "TextureResource.h"
#include "Engine/Engine.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Materials/MaterialInterface.h"

SCustomShapeButton::SCustomShapeButton()
{
	// Reset standard hover behavior, it will be set by the custom logic
	SetHover(false);
}

// Virtual destructor, unregister data
SCustomShapeButton::~SCustomShapeButton()
{
	if (IsValid(RenderTarget.Get()))
	{
		RenderTarget->ConditionalBeginDestroy();
		RenderTarget.Reset();
	}
}

// Updates the internal texture size
void SCustomShapeButton::SetTextureSize(const FIntPoint& InSize)
{
	if (!ensureMsgf(InSize.X > 0 && InSize.Y > 0, TEXT("ASSERT: [%i] %hs:\nTexture Size is not valid!"), __LINE__, __FUNCTION__)
		|| TextureRes == InSize)
	{
		// Is already set
		return;
	}

	TextureRes = InSize;
}

// Forces to update the Raw Colors (pixels data) about current image
void SCustomShapeButton::ForceUpdateImage()
{
	RawColors.Empty();
	TryUpdateRawColorsOnce();
}

// Calculates the index of the pixel under the cursor
uint32 SCustomShapeButton::GetCurrentPointIndex() const
{
	const FGeometry CurrentGeometry = GetCachedGeometry();
	const FVector2D CurrentGeometrySize = CurrentGeometry.GetLocalSize();
	if (CurrentGeometrySize.X <= 0.f || CurrentGeometrySize.Y <= 0.f)
	{
		// No valid bounds are set
		return INDEX_NONE;
	}

	FVector2D LocalPosition = CurrentGeometry.AbsoluteToLocal(CachedPointerEvent.GetScreenSpacePosition());
	if (!FMath::IsWithinInclusive(LocalPosition.X, 0.0, CurrentGeometrySize.X)
		|| !FMath::IsWithinInclusive(LocalPosition.Y, 0.0, CurrentGeometrySize.Y))
	{
		// Cursor is out of button bounds
		return INDEX_NONE;
	}

	LocalPosition /= CurrentGeometrySize;
	LocalPosition.X *= TextureRes.X;
	LocalPosition.Y *= TextureRes.Y;

	const uint32 LocalPositionX = FMath::Min(FMath::FloorToInt(LocalPosition.X), TextureRes.X - 1);
	const uint32 LocalPositionY = FMath::Min(FMath::FloorToInt(LocalPosition.Y), TextureRes.Y - 1);

	const uint32 PixelRow = LocalPositionY * TextureRes.X;
	return PixelRow + LocalPositionX;
}

FReply SCustomShapeButton::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	return HANDLE_EVENT(SButton::OnMouseButtonDown(MyGeometry, MouseEvent));
}

FReply SCustomShapeButton::OnMouseButtonDoubleClick(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	return HANDLE_EVENT(SButton::OnMouseButtonDoubleClick(MyGeometry, MouseEvent));
}

FReply SCustomShapeButton::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	return HANDLE_EVENT(SButton::OnMouseButtonUp(MyGeometry, MouseEvent));
}

FReply SCustomShapeButton::OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	return HANDLE_EVENT(SButton::OnMouseMove(MyGeometry, MouseEvent));
}

void SCustomShapeButton::OnMouseLeave(const FPointerEvent& MouseEvent)
{
	HANDLE_EVENT(OnMouseLeave_Unhovered(MouseEvent));

	// No logic is expected here, but in OnMouseLeave_Unhovered()
}

FReply SCustomShapeButton::OnMouseLeave_Unhovered(const FPointerEvent& MouseEvent)
{
	SButton::OnMouseLeave(MouseEvent);

	SetHover(false);

	// Call OnHovered\OnUnhovered event
	ExecuteHoverStateChanged(true);

	return FReply::Handled();
}

void SCustomShapeButton::OnMouseEnter(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	HANDLE_EVENT(OnMouseEnter_Hovered(MyGeometry, MouseEvent));

	// No logic is expected here, but in OnMouseEnter_Hovered()
}

FReply SCustomShapeButton::OnMouseEnter_Hovered(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	SButton::OnMouseEnter(MyGeometry, MouseEvent);

	SetHover(true);

	// Call OnHovered\OnUnhovered event
	ExecuteHoverStateChanged(true);

	return FReply::Handled();
}

// Returns true if cursor is hovered on a texture
bool SCustomShapeButton::IsAlphaPixelHovered() const
{
	if (RawColors.IsEmpty())
	{
		// Raw Colors are not set
		return false;
	}

	const EVisibility CurrentVisibility = GetVisibility();
	if (CurrentVisibility == EVisibility::Collapsed
		|| CurrentVisibility == EVisibility::Hidden)
	{
		// Button is not visible
		return false;
	}

	if (!GetCachedGeometry().IsUnderLocation(CachedPointerEvent.GetScreenSpacePosition()))
	{
		// Button widget itself if not even under pointer, or UpdatePointerEvent was not refreshed
		return false;
	}

	const uint32 BufferPosition = GetCurrentPointIndex();
	if (!RawColors.IsValidIndex(BufferPosition))
	{
		return false;
	}

	const uint8 HoveredPixel = RawColors[BufferPosition].A;
	const uint8 HoveredPixelNormalized = HoveredPixel > 0 ? 1 : 0;

	constexpr uint8 TextureAlpha = 1;
	constexpr uint8 MaterialAlpha = 0;

	return HoveredPixelNormalized == (RenderTarget ? MaterialAlpha : TextureAlpha);
}

// Set once on render thread the buffer data about all pixels of current image if was not set before
void SCustomShapeButton::TryUpdateRawColorsOnce()
{
	if (!RawColors.IsEmpty())
	{
		// Buffer data is already cached, use ForceUpdateImage to refresh it
		return;
	}

	const FSlateBrush* ImageBrush = GetBorderImage();
	UObject* InImage = ImageBrush ? ImageBrush->GetResourceObject() : nullptr;
	if (!ensureMsgf(InImage, TEXT("%hs: 'InImage' is null, most likely no texture is set in the Button Style"), __FUNCTION__))
	{
		return;
	}

	if (const UTexture2D* Texture = Cast<UTexture2D>(InImage))
	{
		UpdateRawColors_Texture(*Texture);
	}
	else if (UMaterialInterface* Material = Cast<UMaterialInterface>(InImage))
	{
		UpdateRawColors_Material(*Material);
	}
	else
	{
		ensureMsgf(false, TEXT("ASSERT: [%i] %hs:\nNo image is set!"), __LINE__, __FUNCTION__);
	}
}

// Copies the buffer data from the texture
void SCustomShapeButton::UpdateRawColors_Texture(const UTexture2D& Texture)
{
	SetTextureSize(FIntPoint(Texture.GetSizeX(), Texture.GetSizeY()));

	// Get Raw Colors data on Render thread
	TWeakPtr<SCustomShapeButton> WeakThisPtr = StaticCastWeakPtr<SCustomShapeButton>(AsWeak());
	checkf(WeakThisPtr.IsValid(), TEXT("ERROR: [%i] %hs:\n'WeakThis' is not valid!"), __LINE__, __FUNCTION__);
	const TWeakObjectPtr<const UTexture2D> WeakTexture = &Texture;
	const FIntRect TextureSize(0, 0, TextureRes.X, TextureRes.Y);
	ENQUEUE_RENDER_COMMAND(TryUpdateRawColorsOnce)([WeakThisPtr, WeakTexture, TextureSize](FRHICommandListImmediate& RHICmdList)
	{
		SCustomShapeButton* This = WeakThisPtr.Pin().Get();
		if (!ensureMsgf(This, TEXT("ASSERT: [%i] %hs:\n'This' is not valid!"), __LINE__, __FUNCTION__))
		{
			return;
		}

		const UTexture2D* Texture2D = WeakTexture.Get();
		const FTextureResource* TextureResource = Texture2D ? Texture2D->GetResource() : nullptr;
		FRHITexture* RHITexture = TextureResource ? TextureResource->GetTexture2DRHI() : nullptr;
		if (ensureMsgf(RHITexture, TEXT("%hs: 'RHITexture' is not valid"), __FUNCTION__))
		{
			// Copy data to cache
			RHICmdList.ReadSurfaceData(RHITexture, TextureSize, /*out*/This->RawColors, FReadSurfaceDataFlags());
		}
	});
}

// Copies the buffer data from the material
void SCustomShapeButton::UpdateRawColors_Material(UMaterialInterface& Material)
{
	checkf(GWorld, TEXT("ERROR: [%i] %hs:\n'GWorld' is null!"), __LINE__, __FUNCTION__);

	const FSlateBrush* Image = GetBorderImage();
	checkf(Image, TEXT("ERROR: [%i] %hs:\n'Image' is null!"), __LINE__, __FUNCTION__);
	const FVector2f ImageSize = Image->GetImageSize();
	SetTextureSize(FIntPoint(ImageSize.X, ImageSize.Y));

	// Create new Render Target
	if (!RenderTarget)
	{
		RenderTarget = TStrongObjectPtr(UKismetRenderingLibrary::CreateRenderTarget2D(GWorld, TextureRes.X, TextureRes.Y));
	}

	// Clear created Render Target now before rendering material
	UKismetRenderingLibrary::ClearRenderTarget2D(GWorld, RenderTarget.Get());

	// Render our material first before copying pixels data
	UKismetRenderingLibrary::DrawMaterialToRenderTarget(GWorld, RenderTarget.Get(), &Material);

	// Copy pixels data from Render Target to our cache
	UKismetRenderingLibrary::ReadRenderTarget(GWorld, RenderTarget.Get(), /*out*/RawColors);
}

// Attempts to process the event and returns a reply
void SCustomShapeButton::HandleEvent(FReply& OutReply, const FPointerEvent& Event, const TFunctionRef<FReply(const TSharedRef<SCustomShapeButton>&)>& Callback)
{
	// Skip if button was already handled during previous iteration, unhover all underlay buttons
	if (OutReply.IsEventHandled())
	{
		if (IsHovered())
		{
			OnMouseLeave_Unhovered(Event);
		}

		return;
	}

	CachedPointerEvent = Event;
	TryUpdateRawColorsOnce();

	const bool bIsHoveredNow = IsAlphaPixelHovered();
	if (IsHovered() == bIsHoveredNow)
	{
		if (IsHovered())
		{
			Callback(SharedThis(this));
			OutReply = FReply::Handled();
		}

		return;
	}

	if (bIsHoveredNow)
	{
		OutReply = OnMouseEnter_Hovered(GetCachedGeometry(), Event);
	}
	else
	{
		OutReply = OnMouseLeave_Unhovered(Event);
	}
}
