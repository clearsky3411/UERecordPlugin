#include "VdjmVcardDescriptorEventNodes.h"

#include "Blueprint/UserWidget.h"
#include "Components/NamedSlot.h"
#include "Components/PanelWidget.h"
#include "VdjmEvents/VdjmRecordEventManager.h"
#include "VdjmRecorderWorldContextSubsystem.h"
#include "VdjmVcard.h"
#include "VdjmVcardDescriptorApplier.h"
#include "VdjmVcardDescriptorBase.h"
#include "VdjmVcardDescriptorRegistryDataAsset.h"

namespace
{
	struct FVcardEventHostWidgetLookupResult
	{
		UUserWidget* Widget = nullptr;
		bool bFoundFromRuntimeSlot = false;
		bool bFoundFromContext = false;
	};

	FName ResolveContextLookupKey(FName runtimeSlotKey, FName contextKey)
	{
		return not contextKey.IsNone() ? contextKey : runtimeSlotKey;
	}

	UUserWidget* FindRuntimeWidgetBySlot(UVdjmRecordEventManager* eventManager, FName runtimeSlotKey)
	{
		if (eventManager == nullptr || runtimeSlotKey.IsNone())
		{
			return nullptr;
		}

		return Cast<UUserWidget>(eventManager->FindRuntimeObjectSlot(runtimeSlotKey));
	}

	UUserWidget* FindWidgetByContext(UVdjmRecordEventManager* eventManager, FName contextKey)
	{
		if (eventManager == nullptr || contextKey.IsNone())
		{
			return nullptr;
		}

		UVdjmRecorderWorldContextSubsystem* worldContextSubsystem = UVdjmRecorderWorldContextSubsystem::Get(eventManager);
		if (worldContextSubsystem == nullptr)
		{
			return nullptr;
		}

		return Cast<UUserWidget>(worldContextSubsystem->FindContextObject(contextKey));
	}

	bool ResolveHostWidget(
		UVdjmRecordEventManager* eventManager,
		FName runtimeSlotKey,
		FName contextKey,
		EVdjmRecordEventWidgetLookupPolicy lookupPolicy,
		FVcardEventHostWidgetLookupResult& outLookupResult)
	{
		outLookupResult = FVcardEventHostWidgetLookupResult();

		const FName contextLookupKey = ResolveContextLookupKey(runtimeSlotKey, contextKey);
		const auto tryRuntimeSlot = [&]()
		{
			if (UUserWidget* widget = FindRuntimeWidgetBySlot(eventManager, runtimeSlotKey))
			{
				outLookupResult.Widget = widget;
				outLookupResult.bFoundFromRuntimeSlot = true;
				return true;
			}

			return false;
		};

		const auto tryContext = [&]()
		{
			if (UUserWidget* widget = FindWidgetByContext(eventManager, contextLookupKey))
			{
				outLookupResult.Widget = widget;
				outLookupResult.bFoundFromContext = true;
				return true;
			}

			return false;
		};

		switch (lookupPolicy)
		{
		case EVdjmRecordEventWidgetLookupPolicy::ERuntimeSlotOnly:
			return tryRuntimeSlot();
		case EVdjmRecordEventWidgetLookupPolicy::EContextOnly:
			return tryContext();
		case EVdjmRecordEventWidgetLookupPolicy::EContextThenRuntime:
			return tryContext() || tryRuntimeSlot();
		case EVdjmRecordEventWidgetLookupPolicy::ERuntimeSlotThenContext:
		default:
			return tryRuntimeSlot() || tryContext();
		}
	}

	FVdjmRecordEventResult MakeVcardDescriptorNodeResult(bool bSuccess, bool bSucceedIfMissing)
	{
		return UVdjmRecordEventBase::MakeResult(
			(bSuccess || bSucceedIfMissing) ? EVdjmRecordEventResultType::ESuccess : EVdjmRecordEventResultType::EFailure,
			INDEX_NONE,
			NAME_None,
			0.0f);
	}
}

FVdjmRecordEventResult UVcardEventApplyDescriptorNode::ExecuteEvent_Implementation(
	UVdjmRecordEventManager* EventManager,
	AVdjmRecordBridgeActor* BridgeActor)
{
	FVcardEventHostWidgetLookupResult lookupResult;
	if (!ResolveHostWidget(EventManager, RuntimeSlotKey, ContextKey, LookupPolicy, lookupResult) || lookupResult.Widget == nullptr)
	{
		if (bLogResult)
		{
			UE_LOG(
				LogVdjmVcard,
				Warning,
				TEXT("VcardDescriptorEvent Apply failed: host widget missing. RuntimeSlotKey=%s ContextKey=%s DescriptorKey=%s"),
				*RuntimeSlotKey.ToString(),
				*ContextKey.ToString(),
				*DescriptorKey.ToString());
		}

		return MakeVcardDescriptorNodeResult(false, bSucceedIfMissing);
	}

	if (bStoreContextResultInRuntimeSlot && lookupResult.bFoundFromContext && EventManager != nullptr && !RuntimeSlotKey.IsNone())
	{
		EventManager->SetRuntimeObjectSlot(RuntimeSlotKey, lookupResult.Widget);
	}

	if (!IsValid(DescriptorRegistryDataAsset) || DescriptorKey.IsNone())
	{
		if (bLogResult)
		{
			UE_LOG(
				LogVdjmVcard,
				Warning,
				TEXT("VcardDescriptorEvent Apply failed: registry or descriptor key missing. Host=%s Registry=%s DescriptorKey=%s"),
				*GetNameSafe(lookupResult.Widget),
				*GetNameSafe(DescriptorRegistryDataAsset),
				*DescriptorKey.ToString());
		}

		return MakeVcardDescriptorNodeResult(false, bSucceedIfMissing);
	}

	UVcardDescriptorBase* descriptor = nullptr;
	if (!DescriptorRegistryDataAsset->FindDescriptorByKey(DescriptorKey, descriptor) || !IsValid(descriptor))
	{
		if (bLogResult)
		{
			UE_LOG(
				LogVdjmVcard,
				Warning,
				TEXT("VcardDescriptorEvent Apply failed: descriptor not found. Registry=%s DescriptorKey=%s"),
				*GetNameSafe(DescriptorRegistryDataAsset),
				*DescriptorKey.ToString());
		}

		return MakeVcardDescriptorNodeResult(false, bSucceedIfMissing);
	}

	FVcardDescriptorApplyRequest request;
	request.NamedSlotHostWidget = lookupResult.Widget;
	request.CacheOwnerWidget = lookupResult.Widget;
	request.FallbackTargetSlotName = FallbackTargetSlotName;
	request.DescriptorKey = DescriptorKey;
	request.PayloadData = PayloadData;
	request.bAllowCreate = true;

	FVcardDescriptorApplyResult applyResult;
	const bool bApplied = descriptor->ApplyToWidget(request, applyResult);
	applyResult.DescriptorKey = DescriptorKey;

	if (bLogResult)
	{
		UE_LOG(
			LogVdjmVcard,
			Log,
			TEXT("VcardDescriptorEvent Apply Result=%s Host=%s DescriptorKey=%s Created=%d Error=%s"),
			bApplied ? TEXT("true") : TEXT("false"),
			*GetNameSafe(lookupResult.Widget),
			*DescriptorKey.ToString(),
			applyResult.CreatedWidgets.Num(),
			*applyResult.ErrorReason);
	}

	return MakeVcardDescriptorNodeResult(bApplied, bSucceedIfMissing);
}

FVdjmRecordEventResult UVcardEventClearDescriptorSlotNode::ExecuteEvent_Implementation(
	UVdjmRecordEventManager* EventManager,
	AVdjmRecordBridgeActor* BridgeActor)
{
	FVcardEventHostWidgetLookupResult lookupResult;
	if (!ResolveHostWidget(EventManager, RuntimeSlotKey, ContextKey, LookupPolicy, lookupResult) || lookupResult.Widget == nullptr)
	{
		if (bLogResult)
		{
			UE_LOG(
				LogVdjmVcard,
				Warning,
				TEXT("VcardDescriptorEvent Clear failed: host widget missing. RuntimeSlotKey=%s ContextKey=%s TargetSlotName=%s"),
				*RuntimeSlotKey.ToString(),
				*ContextKey.ToString(),
				*TargetSlotName.ToString());
		}

		return MakeVcardDescriptorNodeResult(false, bSucceedIfMissing);
	}

	if (TargetSlotName.IsNone())
	{
		if (bLogResult)
		{
			UE_LOG(LogVdjmVcard, Warning, TEXT("VcardDescriptorEvent Clear failed: TargetSlotName is None. Host=%s"), *GetNameSafe(lookupResult.Widget));
		}

		return MakeVcardDescriptorNodeResult(false, bSucceedIfMissing);
	}

	UNamedSlot* namedSlot = nullptr;
	if (UVcardDescriptorApplier::FindNamedSlot(lookupResult.Widget, TargetSlotName, namedSlot) && IsValid(namedSlot))
	{
		if (bHideInsteadOfRemove)
		{
			namedSlot->SetVisibility(ESlateVisibility::Collapsed);
		}
		else
		{
			if (UWidget* contentWidget = namedSlot->GetContent())
			{
				contentWidget->RemoveFromParent();
			}
			namedSlot->SetContent(nullptr);
			namedSlot->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		}

		if (bLogResult)
		{
			UE_LOG(
				LogVdjmVcard,
				Log,
				TEXT("VcardDescriptorEvent Clear named slot succeeded. Host=%s TargetSlotName=%s Hide=%s"),
				*GetNameSafe(lookupResult.Widget),
				*TargetSlotName.ToString(),
				bHideInsteadOfRemove ? TEXT("true") : TEXT("false"));
		}

		return MakeVcardDescriptorNodeResult(true, bSucceedIfMissing);
	}

	UPanelWidget* panelWidget = nullptr;
	if (UVcardDescriptorApplier::FindPanelWidget(lookupResult.Widget, TargetSlotName, panelWidget) && IsValid(panelWidget))
	{
		if (bHideInsteadOfRemove)
		{
			panelWidget->SetVisibility(ESlateVisibility::Collapsed);
		}
		else
		{
			panelWidget->ClearChildren();
			panelWidget->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		}

		if (bLogResult)
		{
			UE_LOG(
				LogVdjmVcard,
				Log,
				TEXT("VcardDescriptorEvent Clear panel succeeded. Host=%s TargetSlotName=%s Hide=%s"),
				*GetNameSafe(lookupResult.Widget),
				*TargetSlotName.ToString(),
				bHideInsteadOfRemove ? TEXT("true") : TEXT("false"));
		}

		return MakeVcardDescriptorNodeResult(true, bSucceedIfMissing);
	}

	if (bLogResult)
	{
		UE_LOG(
			LogVdjmVcard,
			Warning,
			TEXT("VcardDescriptorEvent Clear failed: target slot/panel not found. Host=%s TargetSlotName=%s"),
			*GetNameSafe(lookupResult.Widget),
			*TargetSlotName.ToString());
	}

	return MakeVcardDescriptorNodeResult(false, bSucceedIfMissing);
}
