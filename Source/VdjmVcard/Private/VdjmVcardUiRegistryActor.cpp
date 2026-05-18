#include "VdjmVcardUiRegistryActor.h"

#include "Blueprint/UserWidget.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "VdjmVcard.h"
#include "VdjmVcardDescriptorApplier.h"
#include "VdjmVcardDescriptorBase.h"
#include "VdjmVcardDescriptorRegistryDataAsset.h"
#include "VdjmVcardWidgets.h"

AVcardUiRegistryActor::AVcardUiRegistryActor()
{
	PrimaryActorTick.bCanEverTick = false;
}

bool AVcardUiRegistryActor::CreateRootWidget(FString& outErrorReason)
{
	outErrorReason.Reset();

	if (IsValid(mRootWidget))
	{
		return true;
	}

	if (!*RootWidgetClass)
	{
		outErrorReason = TEXT("RootWidgetClass is not assigned.");
		return false;
	}

	UWorld* world = GetWorld();
	if (world == nullptr)
	{
		outErrorReason = TEXT("World is invalid.");
		return false;
	}

	if (APlayerController* playerController = UGameplayStatics::GetPlayerController(world, PlayerIndex))
	{
		mRootWidget = CreateWidget<UUserWidget>(playerController, RootWidgetClass);
	}

	if (!IsValid(mRootWidget) && !bRequireOwningPlayer)
	{
		mRootWidget = CreateWidget<UUserWidget>(world, RootWidgetClass);
	}

	if (!IsValid(mRootWidget))
	{
		outErrorReason = FString::Printf(TEXT("Failed to create root widget '%s'."), *GetNameSafe(*RootWidgetClass));
		return false;
	}

	if (UVcardWidgetBase* rootVcardWidget = Cast<UVcardWidgetBase>(mRootWidget))
	{
		rootVcardWidget->ApplyVcardDescriptorContext(DescriptorRegistry, this);
	}

	mRootWidget->AddToViewport(RootZOrder);

	if (bApplyRootDescriptorOnCreate)
	{
		FVcardDescriptorApplyResult applyResult;
		if (!ApplyRootDescriptor(applyResult))
		{
			outErrorReason = applyResult.ErrorReason;
			return false;
		}
	}

	return true;
}

bool AVcardUiRegistryActor::ApplyRootDescriptor(FVcardDescriptorApplyResult& outResult)
{
	outResult = FVcardDescriptorApplyResult();
	outResult.DescriptorKey = RootDescriptorKey;

	if (RootDescriptorKey.IsNone())
	{
		outResult.bSuccess = true;
		return true;
	}

	if (!IsValid(DescriptorRegistry))
	{
		outResult.ErrorReason = TEXT("DescriptorRegistry is not assigned.");
		return false;
	}

	if (!IsValid(mRootWidget))
	{
		outResult.ErrorReason = TEXT("Root widget is not created.");
		return false;
	}

	TArray<UUserWidget*> createdWidgets;
	FString errorReason;
	const bool bGenerated = UVcardDescriptorApplier::GenerateWidgetsIntoNamedSlotsFromVcardDescriptorDataAsset(
		mRootWidget,
		DescriptorRegistry,
		RootDescriptorKey,
		this,
		createdWidgets,
		errorReason);
	outResult.bSuccess = bGenerated;
	for (UUserWidget* createdWidget : createdWidgets)
	{
		if (IsValid(createdWidget))
		{
			outResult.CreatedWidgets.Add(createdWidget);
		}
	}

	outResult.ErrorReason = errorReason;
	return bGenerated;
}

bool AVcardUiRegistryActor::ApplyRegistryToWidget(UVcardWidgetBase* widget, UObject* contextObject)
{
	if (!IsValid(widget))
	{
		return false;
	}

	widget->ApplyVcardDescriptorContext(DescriptorRegistry, IsValid(contextObject) ? contextObject : this);
	return true;
}

bool AVcardUiRegistryActor::FindDescriptorByKey(FName descriptorKey, UVcardDescriptorBase*& outDescriptor) const
{
	outDescriptor = nullptr;
	return IsValid(DescriptorRegistry) && DescriptorRegistry->FindDescriptorByKey(descriptorKey, outDescriptor);
}

void AVcardUiRegistryActor::BeginPlay()
{
	Super::BeginPlay();

	if (!bCreateRootOnBeginPlay)
	{
		return;
	}

	FString errorReason;
	if (!CreateRootWidget(errorReason))
	{
		UE_LOG(LogVdjmVcard, Warning, TEXT("Vcard root widget creation failed Actor=%s Reason=%s"), *GetNameSafe(this), *errorReason);
	}
}
