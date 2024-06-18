// Copyright (c) Yevhenii Selivanov

#include "SCustomShapeButton.h"
//---
#include "RenderingThread.h"
#include "RHICommandList.h"
#include "RHIResources.h"
#include "TextureResource.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Materials/MaterialInterface.h"

// Virtual destructor, unregister data
SCustomShapeButton::~SCustomShapeButton()
{
	if (IsValid(RenderTarget.Get()))
	{
		RenderTarget->ConditionalBeginDestroy();
		RenderTarget.Reset();
	}
}

/** Allows button to be hovered. */
void SCustomShapeButton::SetCanHover(bool bAllow)
{
	if (bCanHover == bAllow)
	{
		return;
	}

	bCanHover = bAllow;

	TAttribute<bool> bHovered = false;
	if (bAllow)
	{
		bHovered = MakeAttributeLambda([WeakThisPtr = StaticCastWeakPtr<SCustomShapeButton>(AsWeak())]() -> bool
		{
			const TSharedPtr<SCustomShapeButton> This = WeakThisPtr.Pin();
			return This.IsValid() && This->IsAlphaPixelHovered();
		});
	}

	SetHover(bHovered);

	TryDetectOnHovered();
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
	const FVector2D CurrentGeometrySize = CurrentGeometry.GetLocalSize();
	if (CurrentGeometrySize.X <= 0.f || CurrentGeometrySize.Y <= 0.f)
	{
		return INDEX_NONE;
	}

	FVector2D LocalPosition = CurrentGeometry.AbsoluteToLocal(CurrentMouseEvent.GetScreenSpacePosition());
	LocalPosition /= CurrentGeometrySize;
	LocalPosition.X *= TextureRes.X;
	LocalPosition.Y *= TextureRes.Y;
	const uint32 LocalPositionX = FMath::FloorToInt(LocalPosition.X);
	const uint32 LocalPositionY = FMath::FloorToInt(LocalPosition.Y);

	const uint32 PixelRow = LocalPositionY * TextureRes.X;
	return PixelRow + LocalPositionX;
}

FReply SCustomShapeButton::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	UpdateMouseData(MyGeometry, MouseEvent);

	if (!IsHovered())
	{
		// Avoid press event
		return FReply::Unhandled();
	}

	return SButton::OnMouseButtonDown(MyGeometry, MouseEvent);
}

FReply SCustomShapeButton::OnMouseButtonDoubleClick(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	UpdateMouseData(MyGeometry, MouseEvent);

	if (!IsHovered())
	{
		// Avoid press event
		return FReply::Unhandled();
	}

	return SButton::OnMouseButtonDoubleClick(MyGeometry, MouseEvent);
}

FReply SCustomShapeButton::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	UpdateMouseData(MyGeometry, MouseEvent);

	FReply Reply = FReply::Unhandled();
	if (IsHovered())
	{
		Reply = SButton::OnMouseButtonUp(MyGeometry, MouseEvent);
	}

	SetCanHover(false);

	return Reply;
}

FReply SCustomShapeButton::OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	UpdateMouseData(MyGeometry, MouseEvent);

	if (!bCanHover
		&& IsAlphaPixelHovered())
	{
		// Allow to be hovered since mouse is hovered on alpha pixel
		SetCanHover(true);
	}

	TryDetectOnHovered();

	return SButton::OnMouseMove(MyGeometry, MouseEvent);
}

void SCustomShapeButton::OnMouseLeave(const FPointerEvent& MouseEvent)
{
	SButton::OnMouseLeave(MouseEvent);

	SetCanHover(false);
}

void SCustomShapeButton::OnMouseEnter(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	SButton::OnMouseEnter(MyGeometry, MouseEvent);

	TryUpdateRawColorsOnce();

	SetCanHover(true);
}

// Returns true if cursor is hovered on a texture
bool SCustomShapeButton::IsAlphaPixelHovered() const
{
	if (RawColors.IsEmpty())
	{
		// Raw Colors are not set
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
	if (!ensureMsgf(InImage, TEXT("%s: 'InImage' is null, most likely no texture is set in the Button Style"), *FString(__FUNCTION__)))
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
		ensureMsgf(false, TEXT("ASSERT: [%i] %hs:\n'No image' is set!"), __LINE__, __FUNCTION__);
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
		FRHITexture2D* RHITexture2D = TextureResource ? TextureResource->GetTexture2DRHI() : nullptr;
		if (ensureMsgf(RHITexture2D, TEXT("%hs: 'RHITexture2D' is not valid"), __FUNCTION__))
		{
			// Copy data to cache
			RHICmdList.ReadSurfaceData(RHITexture2D, TextureSize, /*out*/This->RawColors, FReadSurfaceDataFlags());
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

// Try register On Hovered and On Unhovered events
void SCustomShapeButton::TryDetectOnHovered()
{
	const bool bIsHoveredNow = bCanHover && IsHovered();
	if (bIsHovered != bIsHoveredNow)
	{
		// Call OnHovered\OnUnhovered event
		ExecuteHoverStateChanged(true);
	}

	bIsHovered = bIsHoveredNow;
}

// Caching current geometry and last mouse event
void SCustomShapeButton::UpdateMouseData(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	CurrentGeometry = MyGeometry;
	CurrentMouseEvent = MouseEvent;
}
