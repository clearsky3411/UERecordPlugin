// Fill out your copyright notice in the Description page of Project Settings.


#include "VdjmMobileUiCore.h"

#include "EngineUtils.h"
//#include "SWarningOrErrorBox.h"
#include "ActorEditorUtils.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Camera/CameraComponent.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/RichTextBlock.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/TileView.h"
#include "DSP/AudioDebuggingUtilities.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Kismet/GameplayStatics.h"



DEFINE_LOG_CATEGORY(LogMobileMain);
DEFINE_LOG_CATEGORY(LogOptContentMain);
DEFINE_LOG_CATEGORY(LogMainUiBridge);
DEFINE_LOG_CATEGORY(LogModeContainer);
DEFINE_LOG_CATEGORY(LogSwitcherMgr);
DEFINE_LOG_CATEGORY(LogTileViewMain);
DEFINE_LOG_CATEGORY(LogTileViewItemWidget);
DEFINE_LOG_CATEGORY(LogTileViewItemData);
DEFINE_LOG_CATEGORY(LogTileViewItemLoader);
DEFINE_LOG_CATEGORY(LogCmdSelector);
DEFINE_LOG_CATEGORY(LogCmdChangeSwitcher);
DEFINE_LOG_CATEGORY(LogBackgroundActor);
DEFINE_LOG_CATEGORY(LogMotionActor);


void UTVdjmVcardTileViewWidget::ChangeTileViewItemWidgetClass(TSubclassOf<UUserWidget> newItemWidgetClass)
{
	if (!newItemWidgetClass)
	{
		//	Clear로 돌아가야함.
		return;
	}
	auto preEntryWidgetClass = EntryWidgetClass;
	EntryWidgetClass = newItemWidgetClass;

	RegenerateAllEntries();

	OnTileViewItemWidgetClassChanged.Broadcast(this,preEntryWidgetClass,newItemWidgetClass);
}

UWidget* ATVdjmVcardMainUiBridge::TravelParentClass(UWidget* start,UClass* targetCls)
{
	UWidget* result = nullptr;
	int32 loopCnt = 0;
	const int32 maxLoopCnt = 32;
	UWidget* parentWidget = start;
	while (parentWidget && (loopCnt++ < maxLoopCnt))
	{
		if (parentWidget->IsA(targetCls))
		{
			result = parentWidget;
			break;
		}
		
		if (UWidget* parent = parentWidget->GetParent())
		{
			parentWidget = parent;
		}
		else
		{
			parentWidget = parentWidget->GetTypedOuter<UUserWidget>();
		}
	}
	return result;
}

TArray<UWidget*> ATVdjmVcardMainUiBridge::TravelChildrenClass(UWidget* start, UClass* targetCls)
{
	TArray<UWidget*> result;
	if (IsValid(start) && targetCls != nullptr)
	{
		if (UPanelWidget* panelBase = Cast<UPanelWidget>(start))
		{
			TArray<UWidget*> children(panelBase->GetAllChildren());
			for (UWidget* child : children)
			{
				if (child->IsA(targetCls))
				{
					result.Add(child);
				}
				TArray<UWidget*> subChildren = TravelChildrenClass(child,targetCls);
				result.Append(subChildren);
			}
		}
		else if (UUserWidget* userBase = Cast<UUserWidget>(start))
		{
			if (UWidgetTree* childWidget = userBase->WidgetTree)
			{
				childWidget->ForEachWidget(
					[&result,targetCls](UWidget* widget)
					{
						if (widget->IsA(targetCls))
						{
							result.Add(widget);
						}
					});
			}
		}
		else
		{
			UE_LOG(LogMainUiBridge,Warning,TEXT("TravelChildrenClass: start widget %s is neither PanelWidget nor UserWidget."),*start->GetName());
		}
	}

	return result;
}

ATVdjmVcardMainUiBridge* ATVdjmVcardMainUiBridge::TryGetBridge(UWorld* world)
{
	ATVdjmVcardMainUiBridge* result = nullptr;
	if (world)
	{
		bool found = false;
		for (TActorIterator<ATVdjmVcardMainUiBridge> it(world); it; ++it)
		{
			result = *it;
			found = true;
			break;
		}

		if (not found)
		{
			result = Cast<ATVdjmVcardMainUiBridge>(world->SpawnActor(ATVdjmVcardMainUiBridge::StaticClass()));
		}
	}
	return result;
}

ATVdjmVcardBackgroundActor* ATVdjmVcardMainUiBridge::TryGetBackgroundActorStaticWorld()
{
	return ATVdjmVcardBackgroundActor::TryGetBackgroundActor(ATVdjmVcardMainUiBridge::TryGetStaticWorld());
}

ATVdjmVcardMotionActor* ATVdjmVcardMainUiBridge::TryGetMotionActorStaticWorld()
{
	return ATVdjmVcardMotionActor::TryGetMotionActor(ATVdjmVcardMainUiBridge::TryGetStaticWorld());
}

ATVdjmVcardBackgroundActor* ATVdjmVcardMainUiBridge::GetBackgroundActor() const
{
	if (BackgroundActor.IsValid())
	{
		return BackgroundActor.Get();
	}
	else
	{
		BackgroundActor = ATVdjmVcardBackgroundActor::TryGetBackgroundActor(GetWorld());
		return BackgroundActor.Get();
	}
}

void ATVdjmVcardMainUiBridge::RegisterWidgetCache(const FString& widgetName,UWidget* widgetPtr)
{
	if (widgetPtr)
	{
		if (UiWidgetCache.Contains(widgetName))
		{
			UE_LOG(LogMainUiBridge,Warning,TEXT("WidgetCache already contains widget named %s. Overwriting."),*widgetName);
		}
		UiWidgetCache.Add(widgetName,widgetPtr);
		
	}
}

void ATVdjmVcardMainUiBridge::RegisterWidget_Switcher(const FString& switcherName, UWidget* switcherWidget,const FString& pageName /*= FString(TEXT(""))*/,int32 pageIndex /*= 0*/)
{
	if (ATVdjmVcardMainUiBridge* bridge = TryGetBridgetFromStaticWorld())
	{
		UE_LOG(LogMainUiBridge,Log,TEXT("{{ RegisterWidget_Switcher called for %s"),*switcherName);
		bridge->RegisterWidgetCache(switcherName,switcherWidget);
		
		if (switcherWidget->IsA(UWidgetSwitcher::StaticClass()))
		{
			if (UTVdjmVcardBridgeSwitcherMgrComp::RegisterSwitcherPage(
				bridge,switcherName,pageName,pageIndex))
			{
				UE_LOG(LogMainUiBridge,Log,TEXT("RegisterWidget_Switcher: switcher page %s registered for switcher %s at index %d."),*pageName,*switcherName,pageIndex);
			}
			else
			{
				UE_LOG(LogMainUiBridge,Warning,TEXT("RegisterWidget_Switcher: failed to register switcher page %s for switcher %s at index %d."),*pageName,*switcherName,pageIndex);
			}
		}
		else
		{
			UE_LOG(LogMainUiBridge,Warning,TEXT("RegisterWidget_Switcher: switcherWidget is not UWidgetSwitcher."));
		}
	}
}


int32 ATVdjmVcardMainUiBridge::CommandExecuteInBlueprint(const FName& commandName,TMap<FString, FString>& commandParams, TMap<FString, UObject*> payloadObjects, UObject* fromObj)
{
	return CommandExecutePayload(
		commandName,commandParams,
	    [&payloadObjects]() -> TMap<FString,TWeakObjectPtr<UObject>>
	    {
		    TMap<FString,TWeakObjectPtr<UObject>> result;
		    for (const TPair<FString,UObject*>& pair : payloadObjects)
		    {
		     result.Add(pair.Key,TWeakObjectPtr<UObject>(pair.Value));
		    }
		    return result;
	    }(),
	    TWeakObjectPtr<UObject>(fromObj));
}

void UTVdjmVcardUiModeContainer::UpdateFullName()
{
	if (ContainerDescriptorName.IsEmpty())
	{
		ContainerFullName = GetName().ToLower();
	}
	else
	{
		ContainerFullName = FString::Printf(TEXT("%s-%s"), *(GetName().ToLower()), *(ContainerDescriptorName.ToLower()));
	}
}

void UTVdjmVcardUiModeContainer::RegisterUiModeContainer()
{
	if (UiBridgeActor.IsValid())
	{
		//	RegisterWidgetCache
		UiBridgeActor->RegisterWidgetCache(ContainerFullName, this);
		UE_LOG(LogModeContainer,Log,TEXT("UiModeContainer %s registered in Bridge."),*ContainerFullName);
	}
}

void UTVdjmVcardUiModeContainer::NativeConstruct()
{
	Super::NativeConstruct();
	
	if (UiBridgeActor == nullptr)
	{
		if (UWorld* world = GetWorld())
		{
			UiBridgeActor = ATVdjmVcardMainUiBridge::TryGetBridge(world);
		}
	}
	if (bAutoRegisterToBridge)
	{
		UpdateFullName();
		ATVdjmVcardMainUiBridge::RegisterWidget_Container(ContainerFullName,this);
		//RegisterUiModeContainer();
		// if (UiBridgeActor.IsValid())
		// {
		// 	//	RegisterWidgetCache
		// 	UiBridgeActor->RegisterWidgetCache(ContainerFullName, this);
		// }
	}
}

int32 ATVdjmVcardMainUiBridge::CommandExecute_before(const FTVdjmVcardCmdPacket& cmdPacket)
{
	int32 result = -1;
	// bool isOnFailedBroadcasted = true;
	// if (CommandValidInTable(cmdPacket.CommandName))
	// {
	// 	if (UTVdjmVcardCommand* cmdObject = NewObject<UTVdjmVcardCommand>(this, CommandTable[cmdPacket.CommandName]))
	// 	{
	// 		cmdObject->UiBridgeActor = this;
	// 		cmdObject->CmdPacketSnapShot = cmdPacket;
	// 		
	// 		OnPrevExecutedCommand.Broadcast(this,cmdObject);
	//
	// 		ITVdjmVcardCommandAction::Execute_ExecuteCommand()
	// 		result = cmdObject->Execute();
	// 		if (-1 < result)
	// 		{
	// 			OnExecutedCommandSuccess.Broadcast(this,cmdObject);
	// 			PushUndoCommand(cmdObject);	// calls OnUndoStackChanged
	// 			ClearRedoStack();
	// 		}
	// 		else
	// 		{
	// 			OnExecutedCommandFailed.Broadcast(this,cmdObject);
	// 			LastErrorString += FString::Printf(TEXT("Command %s execution failed: %s"), *cmdPacket.CommandName.ToString(), *cmdObject->ErrorString());
	// 		}
	// 		OnPostExecutedCommand.Broadcast(this,cmdObject);
	// 	}
	// }
	// else
	// {
	// 	LastErrorString += FString::Printf(TEXT("Command %s not found in command table."), *cmdPacket.CommandName.ToString());
	// 	UE_LOG(LogTemp,Warning,TEXT("Command %s not found in command table."),*cmdPacket.CommandName.ToString());
	// }
	
	return result;
}

int32 ATVdjmVcardMainUiBridge::CommandExecute(const FTVdjmVcardCmdPacket& cmdPacket)
{
	int32 result = -1;
	if (CommandValidInTable(cmdPacket.CommandName))
	{
		TScriptInterface<ITVdjmVcardCommandAction> cmdActionInterface = CreateCommandInstance
		(
			this,
			CommandTable[cmdPacket.CommandName],
			this,
			cmdPacket
		);
		if (UObject* cmdObject = cmdActionInterface.GetObject() )
		{
			ITVdjmVcardCommandAction* cmdAction = cmdActionInterface.GetInterface();
			OnPrevExecutedCommand.Broadcast(this,cmdActionInterface);

			result = ITVdjmVcardCommandAction::Execute_ExecuteCommand(cmdObject);
			
			//result = cmdObject->Execute();
			if (-1 < result)
			{
				OnExecutedCommandSuccess.Broadcast(this,cmdActionInterface);
				PushUndoCommand(cmdObject);	// calls OnUndoStackChanged
				ClearRedoStack();
			}
			else
			{
				OnExecutedCommandFailed.Broadcast(this,cmdObject);
				//LastErrorString += FString::Printf(TEXT("Command %s execution failed: %s"), *cmdPacket.CommandName.ToString(), *cmdObject->ErrorString());
			}
			OnPostExecutedCommand.Broadcast(this,cmdObject);
		}
	}
	else
	{
		LastErrorString += FString::Printf(TEXT("Command %s not found in command table."), *cmdPacket.CommandName.ToString());
		UE_LOG(LogMainUiBridge,Warning,TEXT("Command %s not found in command table."),*cmdPacket.CommandName.ToString());
	}

	return result;
}

int32 ATVdjmVcardMainUiBridge::CommandExecuteEmplace(const FName& commandName, TMap<FString, FString> commandParams, UObject* fromObj )
{
	FTVdjmVcardCmdPacket cmdPacket = {};
	cmdPacket.CommandName = commandName;
	cmdPacket.CommandParams = commandParams;
	cmdPacket.FromObject = fromObj;
	return CommandExecute(cmdPacket);

}

int32 ATVdjmVcardMainUiBridge::CommandExecuteEmplaceDebugString(const FName& commandName,
	TMap<FString, FString> commandParams, UObject* fromObj, const FString& debugString)
{
	UE_LOG(LogMainUiBridge,Log,TEXT("CommandExecuteEmplaceDebugString called. DebugString: %s"),*debugString);
	return CommandExecuteEmplace(commandName,commandParams,fromObj);
}

int32 ATVdjmVcardMainUiBridge::CommandExecutePayload(const FName& commandName,const TMap<FString, FString>& commandParams, TMap<FString, TWeakObjectPtr<UObject>> payloadObjects,TWeakObjectPtr<UObject> fromObj)
{
	FTVdjmVcardCmdPacket cmdPacket = {};
	cmdPacket.CommandName = commandName;
	cmdPacket.CommandParams = commandParams;
	cmdPacket.FromObject = fromObj;
	cmdPacket.PayloadObjects = payloadObjects;
	return CommandExecute(cmdPacket);
}

void ATVdjmVcardMainUiBridge::UndoCommand()
{
	int32 result = -1;
	if (IsUndoPossible())
	{
		TScriptInterface<ITVdjmVcardCommandAction> cmd = PopUndoCommand();	//	calls 
		ITVdjmVcardCommandAction::Execute_UndoCommand(cmd.GetObject());
		OnUndoCommandExecuted.Broadcast(this,cmd);

		if (-1 < result)
		{
			OnUndoCommandSuccess.Broadcast(this,cmd);
			PushRedoCommand(cmd);	//	calls OnRedoStackChanged
		}
		else
		{
			OnUndoCommandFailed.Broadcast(this,cmd);
		}
	}
	
}

void ATVdjmVcardMainUiBridge::RedoCommand()
{
	int32 result = -1;
	if (IsRedoPossible())
	{
		TScriptInterface<ITVdjmVcardCommandAction> cmd = PopRedoCommand();	//	calls OnRedoStackChanged
		const FTVdjmVcardCmdPacket& snapshotPacket = ITVdjmVcardCommandAction::Execute_GetCommandPacketSnapShot(cmd.GetObject());
		result = CommandExecute(snapshotPacket);//	이거 말고 여기에서 execute 로 새롭게 만들어줘야함. TODO: bug 인데, 지금은 일단 이렇게.. 왜냐하면 이거가 핵심 기능까지는 아니라서 그럼. 어차피 고칠 내용은 다음과 같음. 이게 아니라 execute 를 직접 호출해주고, 그 결과를 undostack 에 넣어주는거임. 리두를 비우지 말고. 이러면 지금 리두가 딱 하나만 남아서 완전한 리두가 아니게됨.
		OnRedoCommandExecuted.Broadcast(this,cmd);
		
		if (-1 < result)
		{
			OnRedoCommandSuccess.Broadcast(this,cmd);
		}
		else
		{
			OnRedoCommandFailed.Broadcast(this,cmd);
		}
	}
}

void ATVdjmVcardMainUiBridge::BeginPlay()
{
	Super::BeginPlay();

	RegisterCommand<UTVdjmVcardCommandItemSelector>(this);
	RegisterCommand<UTVdjmVcardCommandChangeSwitcher>(this);
	RegisterCommand<UTVdjmVcardCommandControl>(this);

}

bool UTVdjmVcardBridgeSwitcherMgrComp::RegisterSwitcherPage(ATVdjmVcardMainUiBridge* bridge,const FString& switcherName, const FString& pageName, int32 pageIndex)
{
	if (bridge && not pageName.IsEmpty())
	{
		UTVdjmVcardBridgeSwitcherMgrComp* curComp = nullptr;
		
		if (UTVdjmVcardBridgeSwitcherMgrComp* switcherMgr = bridge->GetComponentByClass<UTVdjmVcardBridgeSwitcherMgrComp>())
		{
			curComp = switcherMgr;
		}
		else
		{
			UActorComponent* newComp = bridge->AddComponentByClass(UTVdjmVcardBridgeSwitcherMgrComp::StaticClass(),false,FTransform::Identity,true);
			
			curComp = Cast<UTVdjmVcardBridgeSwitcherMgrComp>(newComp);
		}
			
		if (curComp)
		{
			return curComp->RegisterSwitcherPageIndex(switcherName,pageName,pageIndex);
		}
	}
	else
	{
		if (bridge == nullptr)
		{
			UE_LOG(LogSwitcherMgr,Warning,TEXT("RegisterSwitcherPage: bridge is null."));
		}
		if (pageName.IsEmpty())
		{
			UE_LOG(LogSwitcherMgr,Warning,TEXT("RegisterSwitcherPage: pageName is empty."));
		}
		
	}
	UE_LOG(LogSwitcherMgr,Warning,TEXT("RegisterSwitcherPage: failed to register switcher page %s for switcher %s."),*pageName,*switcherName);
	return false;
}

int32 UTVdjmVcardBridgeSwitcherMgrComp::TryGetSwitcherPageIndex(const FString& switcherPageName) 
{
	int32 outPageIndex = -1;
	
	if (not UiBridge.IsValid())
	{
		if (ATVdjmVcardMainUiBridge* bridge = ATVdjmVcardMainUiBridge::TryGetBridge(GetWorld()))
		{
			UiBridge = bridge;
		}
		else
		{
			UE_LOG(LogSwitcherMgr,Warning,TEXT("TryGetSwitcherPageIndex: UiBridgeActor is invalid."));
			return outPageIndex;
		}
	}
	
	if (const VcardSwitcherPageValue* found = SwitcherPageIndexTable.Find(switcherPageName))
	{
		const FString& switcherName = found->Key;
		const int32& pageIndex = found->Value;

		if (UWidgetSwitcher* switcher = Cast<UWidgetSwitcher>( UiBridge->GetWidgetFromCache(switcherName)))
		{
			int32 currWidgetNum = switcher->GetNumWidgets();
			if (0 <= pageIndex && pageIndex < currWidgetNum)
			{
				outPageIndex = pageIndex;
				UE_LOG(LogSwitcherMgr,Log,TEXT("TryGetSwitcherPageIndex: found pageIndex %d for switcher %s."),pageIndex,*switcherName);
			}
			else
			{
				UE_LOG(LogSwitcherMgr,Warning,TEXT("TryGetSwitcherPageIndex: pageIndex %d out of range for switcher %s with %d widgets."),pageIndex,*switcherName,currWidgetNum);
				SwitcherPageIndexTable.Remove(switcherPageName);
			}
		}
		else
		{
			UE_LOG(LogSwitcherMgr,Warning,TEXT("TryGetSwitcherPageIndex: switcher %s not found in WidgetCache."),*switcherName);
			
			UiBridge->RemoveWidgetFromCache(switcherName);
		}
	}
	else
	{
		UE_LOG(LogSwitcherMgr,Warning,TEXT("TryGetSwitcherPageIndex: switcherPageName %s not found in SwitcherPageIndexTable."),*switcherPageName);
	}
	return outPageIndex;
}

bool UTVdjmVcardBridgeSwitcherMgrComp::RegisterSwitcherPageIndex(const FString& switcherName,const FString& switcherPageName, int32 pageIndex)
{
	if (SwitcherPageIndexTable.Contains(switcherPageName))
	{
		UE_LOG(LogSwitcherMgr,Warning,TEXT("SwitcherPageIndexTable already contains switcherPageName %s. Overwriting."),*switcherPageName);
	}

	if (CheckRegisterSwitcherPageIndex(switcherName,pageIndex))
	{
		SwitcherPageIndexTable.Add(switcherPageName,TPair<FString,int32>(switcherName,pageIndex));
		return true;
	}
	
	UE_LOG(LogSwitcherMgr,Warning,TEXT("RegisterSwitcherPageIndex: failed to register switcher page index %d for switcher %s."),pageIndex,*switcherName);
	return false;
}

bool UTVdjmVcardBridgeSwitcherMgrComp::CheckRegisterSwitcherPageIndex(const FString& switcherName, int32 pageIndex) const
{
	if (not UiBridge.IsValid())
	{
		UiBridge = ATVdjmVcardMainUiBridge::TryGetBridge(GetWorld());
	}
	
	if (UiBridge.IsValid())
	{
		if (UWidget* widget = UiBridge->GetWidgetFromCache(switcherName))
		{
			if (not widget->IsA(UWidgetSwitcher::StaticClass()))
			{
				UE_LOG(LogSwitcherMgr,Warning,TEXT("RegisterSwitcherPageIndex: widget %s is not UWidgetSwitcher."),*switcherName);
				return false;
			}
			
			UWidgetSwitcher* switcher = Cast<UWidgetSwitcher>(widget);
			int32 numWidgets = switcher->GetNumWidgets();
			
			if ( pageIndex < 0 || numWidgets <= pageIndex)
			{
				UE_LOG(LogSwitcherMgr,Warning,TEXT("RegisterSwitcherPageIndex: pageIndex %d out of range for switcher %s with %d widgets."),pageIndex,*switcherName,numWidgets);
				return false;
			}
			return true;
		}	
	}
	
	return false;
}

void UTVdjmVcardBridgeSwitcherMgrComp::BeginPlay()
{
	Super::BeginPlay();

	if(not UiBridge.IsValid())
	{
		UiBridge = ATVdjmVcardMainUiBridge::TryGetBridge(GetWorld());
	}
	
}

int32 UTVdjmVcardCommandItemSelector::ExecuteCommand_Implementation()
{
	int32 result  = Result_Failed_Error();
	
	if (DbcCommandExecuteValid())
	{
		const TArray<FString>& paramKeys = ITVdjmVcardCommandAction::Execute_GetCommandParamKeys(this);
		if (paramKeys.IsEmpty())
		{
			return Result_Failed_InvalidParam();
		}
		const FString& containerNameValue = CmdPacketSnapShot.GetParamString(paramKeys[0]);
		
		if (UUserWidget* foundContainerWidget = UiBridgeActor->GetUserWidgetFromCache(containerNameValue))
		{
			if (foundContainerWidget->GetClass()->ImplementsInterface(UTVdjmVcardInteractable::StaticClass()))
			{
				if (CmdPacketSnapShot.FromObject.IsValid())
				{
					SelectedContainerWidget = foundContainerWidget;
					OldSelectedItem = GetCurrentSelectedItemViaReflection(foundContainerWidget);
					ITVdjmVcardInteractable::Execute_OnVcardSelect(foundContainerWidget,CmdPacketSnapShot.FromObject.Get());
					NewSelectedItem = GetCurrentSelectedItemViaReflection(foundContainerWidget);
				}
				else
				{
					return Result_Failed_PayloadMissing();
				}
				return Result_Succeeded();
			}
			else
			{
				return Result_Failed_CastedContainerInvalid();
			}
		}
		else
		{
			return Result_Failed_ContainerNotFound();
		}
	}
	return result;
}

int32 UTVdjmVcardCommandItemSelector::UndoCommand_Implementation()
{
	int32 result  = Result_Failed_Error();
	if (DbcCommandExecuteValid())
	{
		if (SelectedContainerWidget.IsValid() && SelectedContainerWidget->GetClass()->ImplementsInterface(UTVdjmVcardInteractable::StaticClass()))
		{
			if (OldSelectedItem.IsValid())
			{
				ITVdjmVcardInteractable::Execute_OnVcardSelect(SelectedContainerWidget.Get(),OldSelectedItem.Get());
			}
			else
			{
				return Result_Failed_PayloadMissing();
			}
			return Result_Succeeded();
		}
		else
		{
			return Result_Failed_ContainerNotFound();
		}
	}
	return result;
}

UObject* UTVdjmVcardCommandItemSelector::GetCurrentSelectedItemViaReflection(UUserWidget* foundContainerWidget)
{
	if (not IsValid(foundContainerWidget))
	{
		return nullptr;
	}

	// 1. 변수 이름으로 프로퍼티 정보(메타데이터) 찾기
	const FName PropName(TEXT("CurrentSelectedItem"));
	
	FProperty* Prop = foundContainerWidget->GetClass()->FindPropertyByName(PropName);

	// 2. 해당 프로퍼티가 '인터페이스 프로퍼티'인지 확인 (안전장치)
	if (FInterfaceProperty* InterfaceProp = CastField<FInterfaceProperty>(Prop))
	{
		// 3. 인스턴스(foundContainerWidget)에서 해당 변수의 실제 메모리 주소를 얻음
		void* ValuePtr = InterfaceProp->ContainerPtrToValuePtr<void>(foundContainerWidget);

		// 4. 메모리 주소를 FScriptInterface 포인터로 변환
		// (TScriptInterface는 내부적으로 FScriptInterface와 메모리 구조가 같습니다)
		FScriptInterface* InterfaceData = static_cast<FScriptInterface*>(ValuePtr);

		// 5. 인터페이스 내부에 저장된 UObject(실제 객체) 반환
		return InterfaceData->GetObject();
	}
	else
	{
		UE_LOG(LogCmdSelector, Warning, TEXT("Could not find property 'CurrentSelectedItem' or type mismatch on widget: %s"), *foundContainerWidget->GetName());
	}

	return nullptr;
}

void UTVdjmVcardUiModeButtonItem::NativePreConstruct()
{
	Super::NativePreConstruct();

	if (SelectButton != nullptr)
	{
		if (0.001f < SelectButton->GetRenderOpacity())
		{
			SelectButton->SetRenderOpacity(0.0f);
		}
	}
	
}

void UTVdjmVcardUiModeButtonItem::NativeConstruct()
{
	Super::NativeConstruct();

	if (UiBridgeActor == nullptr)
	{
		if (UWorld* world = GetWorld())
		{
			UiBridgeActor = ATVdjmVcardMainUiBridge::TryGetBridge(world);
		}
	}

	if (UiBridgeActor.IsValid())
	{
		//UE_LOG(LogTemp,Warning,TEXT("{{{{{{{{{{{ UiModeButtonItem %s registered in Bridge. Parent %s }}}}}}}} "),*GetName(),*GetParent()->GetParent()->GetName());
		//UiBridgeActor->GetUiWidgetFromCache()

		if (UWidget* parent = ATVdjmVcardMainUiBridge::TravelParentClass(this, UTVdjmVcardUiModeContainer::StaticClass()))
		{
			mOwnerContainer = Cast<UTVdjmVcardUiModeContainer>(parent);
		}
	}
	
	//if (SelectButton)
	//{
	//	SelectButton->OnClicked.AddDynamic(this, &UTVdjmVcardUiModeButtonItem::OnClickButton);
	//}
	
}

void UTVdjmVcardUiModeContainer::OnVcardSelect_Implementation(UObject* target)
{
	if (CurrentSelectedItem.GetObject() != nullptr)
	{
		ITVdjmVcardInteractable::Execute_OnVcardDeSelect(CurrentSelectedItem.GetObject());
	}

	CurrentSelectedItem = nullptr;
	if (IsValid(target) && target->GetClass()->ImplementsInterface(UTVdjmVcardInteractable::StaticClass()))
	{
		CurrentSelectedItem = TScriptInterface<ITVdjmVcardInteractable>(target);
		if (CurrentSelectedItem.GetObject() != nullptr)
		{
			ITVdjmVcardInteractable::Execute_OnVcardSelect(
				CurrentSelectedItem.GetObject(),
				this
				);
			//CurrentSelectedItem->OnSelect(this);
		}
		else
		{
			UE_LOG(LogModeContainer,Warning,TEXT("mCurrentSelectedItem.GetInterface() == nullptr !! UTVdjmVcardUiModeContainer::OnVcardSelect_Implementation called with target that does not implement ITVdjmVcardInteractable interface."));
		}
	}
	else
	{
		UE_LOG(LogModeContainer,Warning,TEXT("UTVdjmVcardUiModeContainer::OnVcardSelect_Implementation called with invalid target."));
	}
}

UPanelSlot* UTVdjmVcardUiModeContainer::AddPanelItem_Implementation(UUserWidget* itemWidget)
{
	if (ItemHolderPanel&& itemWidget)
	{
		return ItemHolderPanel->AddChild(itemWidget);
	}
	return nullptr;
}


void UTVdjmVcardUiModeDynamicContainer::SetUpCreationList(const TArray<FTVdjmVcardUiModeDynamicItem>& itemCreationList)
{
	if (not mItemCreationList.IsEmpty())
	{
		mItemCreationList.Empty();
		DynamicItemInstanceList.Empty();
	}
	UE_LOG(LogModeContainer,Log,TEXT("UTVdjmVcardUiModeDynamicContainer::SetUpCreationList called. ItemCount %d"),itemCreationList.Num());
	mItemCreationList = itemCreationList;
}

void UTVdjmVcardUiModeDynamicContainer::CreateDynamicItems()
{
	if (not mItemCreationList.IsEmpty())
	{
		ItemHolderPanel->ClearChildren();
		DynamicItemInstanceList.Empty();
		DynamicItemInstanceList.Reserve(mItemCreationList.Num());
		for (const FTVdjmVcardUiModeDynamicItem& itemInfo : mItemCreationList)
		{
			if (itemInfo.ItemWidgetClass != nullptr )
			{
				if(itemInfo.ItemWidgetClass->ImplementsInterface(UTVdjmVcardInteractable::StaticClass())) 
				{
					UUserWidget* newWidget = CreateWidget<UUserWidget>(this, itemInfo.ItemWidgetClass,
						itemInfo.ItemWidgetName);
					DynamicItemInstanceList.Add(newWidget);
					ITVdjmVcardInteractable::Execute_OnVcardSetUpContext(newWidget,itemInfo.ItemData.Get());
					if (IsValid(ItemHolderPanel))
					{
						AddPanelItem(newWidget);
					}
				}
				else
				{
					UE_LOG(LogModeContainer,Warning,TEXT("ItemWidgetClass %s does not implement ITVdjmVcardInteractable interface."),*itemInfo.ItemWidgetClass->GetName());
				}
			}
		}
	}
}

// void UTVdjmVcardUiModeDynamicContainer::OnVcardSelect_Implementation(UObject* target)
// {
// 	UE_LOG(LogTemp,Warning,TEXT("!!!!!!!!!!!!213123123123 UTVdjmVcardUiModeDynamicContainer::OnVcardSelect_Implementation called."));
// 	Super::OnVcardSelect_Implementation(target);
// }

void UTVdjmVcardUiModeDynamicContainer::CreateSpacingWidget()
{
	if (bItemSpacingEnabled && ItemHolderPanel)
	{
		//	ABA
		if (ItemHolderPanel->GetChildrenCount() != 0)
		{
			if (not IsValid(ItemSpacingWidgetClass))
			{
				ItemSpacingWidgetClass = USpacer::StaticClass();
			}
			UWidget* spacingWidget = NewObject<UWidget>(this, ItemSpacingWidgetClass);
			if (USpacer* spacer = Cast<USpacer>(spacingWidget))
			{
				spacer->SetSize(ItemSpacing);
			}
			ItemHolderPanel->AddChild(spacingWidget);
		}
	}
}

UPanelSlot* UTVdjmVcardUiModeDynamicContainer::AddPanelItem_Implementation(UUserWidget* itemWidget)
{
	UE_LOG(LogModeContainer,Warning,TEXT("Current Children Count: %d"),ItemHolderPanel->GetChildrenCount());
	
	if (ItemHolderPanel&& itemWidget)
	{
		CreateSpacingWidget();
		return ItemHolderPanel->AddChild(itemWidget);
	}
	return nullptr;
}

void UTVdjmVcardUiModeDynamicContainer::NativeConstruct()
{
	Super::NativeConstruct();
	UE_LOG(LogModeContainer,Log,TEXT("{ UTVdjmVcardUiModeDynamicContainer::NativeConstruct %s }"),*ContainerFullName);
}

/*
 *	2번호출되는 이유
 *	여기에서 한번 블루프린트에서 한번해줌.
 * 
 */
void UTVdjmVcardUiModeButtonItem::OnClickButton()
{
	if (UiBridgeActor.IsValid()&&mOwnerContainer.IsValid())
	{
		if (UPanelWidget* parent = GetParent())
		{
			UiBridgeActor->CommandExecuteEmplace(
			FName("select-item"),
			{
				{"container-name",
					mOwnerContainer->GetName()},
			},
			this);
		}
	}
}

void UTVdjmVcardUiModeButtonItem::OnVcardSelect_Implementation(UObject* target)
{
	UE_LOG(LogTemp,Warning,TEXT("OnSelect!!!!!!!!!!OnSelect_Implementation."));
}
void UTVdjmVcardUiModeButtonItem::OnVcardDeSelect_Implementation()
{
	UE_LOG(LogTemp,Warning,TEXT("OffSelect!!!!!!!!!!OffSelect_Implementation"));
}

void UTVdjmVcardMobileOptionContentsMain::GenerateOptionWidgetsByDescList()
{
	if (not OptionWidgetDescriptorList.IsEmpty() && WSOptionContents != nullptr)
	{
		WSOptionContents->ClearChildren();
		for (const FTVdjmVcardWidgetDescriptor& descriptor : OptionWidgetDescriptorList)
		{
			if (descriptor.WidgetClass != nullptr)
			{
				if (UWidget* newWidget = NewObject<UWidget>(this, descriptor.WidgetClass.Get(), FName(descriptor.WidgetName)))
				{
					UE_LOG(LogOptContentMain,Log,TEXT("Generating Option Widget %s"),*descriptor.WidgetName);
					WSOptionContents->AddChild(newWidget);
				}
			}
		}
	}
}

void UTVdjmVcardMobileOptionContentsMain::NativePreConstruct()
{
	Super::NativePreConstruct();

	GenerateOptionWidgetsByDescList();
}

UTVdjmVcardConfigDataAsset* UTVdjmVcardMobileMainWidget::TryGetLoadedConfigDataAsset()
{
	UTVdjmVcardConfigDataAsset* result = nullptr;
	
	FSoftObjectPath configObjPath = FSoftObjectPath(TEXT("/Script/VdjmMobileUi.TVdjmVcardConfigDataAsset'/VdjmMobileUi/Bp_VdjmVcardConfigDataAsset.Bp_VdjmVcardConfigDataAsset'"));
	
	if (configObjPath.IsAsset() && configObjPath.IsValid())
	{
		UObject* configResolved = configObjPath.ResolveObject();
		if (configResolved == nullptr)
		{
			configResolved = configObjPath.TryLoad();
			if (configResolved == nullptr)
			{
				UE_LOG(LogMobileMain,Warning,TEXT("Failed to load default VcardConfigDataAsset %s synchronously."),*configObjPath.ToString());
			}
		}
		result = Cast<UTVdjmVcardConfigDataAsset>(configResolved);
	}
	
	return result;
}

void UTVdjmVcardMobileOptionContentsMain::NativeConstruct()
{
	Super::NativeConstruct();

	if (UiBridgeActor == nullptr)
	{
		UiBridgeActor = ATVdjmVcardMainUiBridge::TryGetBridge(GetWorld());
	}

	if (UiBridgeActor.IsValid())
	{
		if (OptionContentsName.IsEmpty())
		{
			OptionContentsName = GetName();
		}
		UE_LOG(LogOptContentMain,Warning,TEXT("-------Registering OptionContents %s to Bridge."),*OptionContentsName.ToLower());
		WSOptionContentsName = OptionContentsName.ToLower();
		ATVdjmVcardMainUiBridge::RegisterWidget_Switcher(*WSOptionContentsName,WSOptionContents);
	}
	
	GenerateOptionWidgetsByDescList();
	
	if (not OptionWidgetDescriptorList.IsEmpty() && WSOptionContents != nullptr && UiBridgeActor.IsValid())
	{
		const FString switcherName = WSOptionContentsName;
		UE_LOG(LogOptContentMain,Log,TEXT("Registering Option Widgets to Bridge SwitcherMgr for switcher %s."),*switcherName);
		for (int32 i = 0; i < OptionWidgetDescriptorList.Num(); ++i )
		{
			const auto& descriptor = OptionWidgetDescriptorList[i];
			if (UTVdjmVcardBridgeSwitcherMgrComp::RegisterSwitcherPage(
			UiBridgeActor.Get(),
				switcherName,
				descriptor.WidgetName,
				i
			))
			{
				UE_LOG(LogOptContentMain,Log,TEXT("Registered Option Widget %s to Bridge SwitcherMgr."),*descriptor.WidgetName);
			}
			else
			{
				UE_LOG(LogOptContentMain,Warning,TEXT("Failed to register Option Widget %s to Bridge SwitcherMgr."),*descriptor.WidgetName);
			}
		}
	}
	else
	{
		if (OptionWidgetDescriptorList.IsEmpty())
		{
			UE_LOG(LogOptContentMain,Warning,TEXT("OptionWidgetDescriptorList is empty."));
		}
		if (WSOptionContents == nullptr)
		{
			UE_LOG(LogOptContentMain,Warning,TEXT("WSOptionContents is null."));
		}
		if (not UiBridgeActor.IsValid())
		{
			UE_LOG(LogOptContentMain,Warning,TEXT("UiBridgeActor is invalid."));
		}
	}
	
}



void UTVdjmVcardMobileMainWidget::DefaultSetting()
{
	if (not IsValid(LoadedConfigDataAsset) )
	{
		LoadedConfigDataAsset = TryGetLoadedConfigDataAsset();
	}
	
	
	if (BottomAndOptionSolver == nullptr)
	{
		BottomAndOptionSolver = NewObject<UTVdjmVcardMobileMoveUiSolver>(this);
	}

	if (UiBridgeActor == nullptr)
	{
		UiBridgeActor = ATVdjmVcardMainUiBridge::TryGetBridge(GetWorld());
	}
	
}


void UTVdjmVcardMobileMainWidget::NativePreConstruct()
{
	Super::NativePreConstruct();
}

void UTVdjmVcardMobileMainWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (WSMainContent)
	{
		ATVdjmVcardMainUiBridge::RegisterWidget_Switcher(WSMainContent->GetName(),WSMainContent);
	}
	
	// if (UiBridgeActor == nullptr)
	// {
	// 	if (UWorld* world = GetWorld())
	// 	{
	// 		UiBridgeActor = ATVdjmVcardMainUiBridge::TryGetBridge(world);
	// 	}
	// }
	
	if (UiBridgeActor)
	{
		/*
		 * TODO: 타일뷰로 이관
		 */
		UClass* findTileViewCls = UTVdjmVcardTileViewMainWidget::StaticClass()
		;
		const TArray<UWidget*>& tileView = ATVdjmVcardMainUiBridge::TravelChildrenClass(this, findTileViewCls);
		if (not tileView.IsEmpty())
		{
			UiBridgeActor->RegisterManagedCommandInstance(findTileViewCls,tileView[0]);	
		}
	}
}

int32 UTVdjmVcardCommandChangeSwitcher::ExecuteCommand_Implementation()
{
	int32 result  = Result_Failed_Error();
	UE_LOG(LogCmdChangeSwitcher,Log,TEXT("UTVdjmVcardCommandChangeSwitcher::Execute called."));
	if (DbcCommandExecuteValid())
	{
		const TArray<FString>& paramKeys = ITVdjmVcardCommandAction::Execute_GetCommandParamKeys(this);
		const FString& switcherNameValue = GetCmdSnapShotParamValue(paramKeys[0]);
		
		UWidgetSwitcher* targetSwitcher = nullptr;
		int32 targetIndex = -1;
		
		if (UWidget* justWidget = UiBridgeActor->GetWidgetFromCache(switcherNameValue))
		{
			if (justWidget->IsA<UWidgetSwitcher>())
			{
				targetSwitcher = Cast<UWidgetSwitcher>(justWidget);
			}
			else
			{
				UE_LOG(LogCmdChangeSwitcher,Warning,TEXT("Cached widget { %s } is not a WidgetSwitcher."),*switcherNameValue);
				return Result_Failed_ContainerNotFound();
			}
		}
		else
		{
			UE_LOG(LogCmdChangeSwitcher,Warning,TEXT("WidgetSwitcher %s not found in cache."),*switcherNameValue);
			return Result_Failed_PayloadMissing();
		}
		const FString& switcherValueType = GetCmdSnapShotParamValue(paramKeys[1]);
		const FString& switcherValue = GetCmdSnapShotParamValue(paramKeys[2]);
		
		if (TEXT("index") == switcherValueType)
		{
			if (not switcherValue.IsEmpty() && switcherValue.IsNumeric())
			{
				UE_LOG(LogCmdChangeSwitcher,Log,TEXT("Parsed target index parameter: %s"),*switcherValue);
				targetIndex = FCString::Atoi(*switcherValue);
			}
			else
			{
				UE_LOG(LogCmdChangeSwitcher,Warning,TEXT("Invalid target index parameter: %s"),*switcherValue);
				return Result_Failed_PayloadValueMissing();
			}
		}
		else if (TEXT("key") == switcherValueType )
		{
			if (not switcherValue.IsEmpty() &&UiBridgeActor.IsValid())
			{
				if (UTVdjmVcardBridgeSwitcherMgrComp* switcherMgr = UiBridgeActor->GetComponentByClass<UTVdjmVcardBridgeSwitcherMgrComp>())
				{
					targetIndex = switcherMgr->TryGetSwitcherPageIndex(switcherValue);
					UE_LOG(LogCmdChangeSwitcher,Log,TEXT("Got target index %d for switcher page key %s."),targetIndex,*switcherValue);
				}
				else
				{
					UE_LOG(LogCmdChangeSwitcher,Warning,TEXT("UiBridgeActor does not have UTVdjmVcardBridgeSwitcherMgrComp component."));
					return Result_Failed_InvalidParam();
				}
			}
		}

		if (targetIndex < 0 || targetIndex >= targetSwitcher->GetNumWidgets())
		{
			UE_LOG(LogCmdChangeSwitcher,Warning,TEXT("Target index %d is out of bounds for WidgetSwitcher %s."),targetIndex,*switcherNameValue);
			return Result_Failed_InvalidParam();
		}
		
		if (targetSwitcher)
		{
			OldIndex = targetSwitcher->GetActiveWidgetIndex();
			targetSwitcher->SetActiveWidgetIndex(targetIndex);
			NewIndex = targetIndex;
			UE_LOG(LogCmdChangeSwitcher,Log,TEXT("WidgetSwitcher %s changed active index from %d to %d."),*switcherNameValue,OldIndex,NewIndex);
			return Result_Succeeded();
		}
		else
		{
			UE_LOG(LogCmdChangeSwitcher,Warning,TEXT("Target WidgetSwitcher %s is invalid."),*switcherNameValue);
			return Result_Failed_ContainerNotFound();
		}
		
		
	}
	UE_LOG(LogCmdChangeSwitcher,Warning,TEXT("DbcCommandExecuteValid() returned false in UTVdjmVcardCommandChangeSwitcher::Execute."));
	return result;
}

int32 UTVdjmVcardCommandChangeSwitcher::UndoCommand_Implementation()
{
	if (TargetSwitcher.IsValid())
	{
		if (OldIndex < 0 || OldIndex >= TargetSwitcher->GetNumWidgets())
		{
			return Result_Failed_InvalidParam();
		}
		TargetSwitcher->SetActiveWidgetIndex(OldIndex);
	}
	else
	{
		return Result_Failed_ContainerNotFound();
	}
	
	return Result_Succeeded();
}

int32 UTVdjmVcardCommandControl::ExecuteCommand_Implementation()
{
	int32 result  = Result_Failed_Error();
	
	if (DbcCommandExecuteValid())
	{
		const TArray<FString>& paramKeys = ITVdjmVcardCommandAction::Execute_GetCommandParamKeys(this);
		const FString& command = GetCmdSnapShotParamValue(paramKeys[0]);
		//	생각해보니깐 이거 필요한가?
	}
	
	return result;
}

int32 UTVdjmVcardCommandControl::UndoCommand_Implementation()
{
	return -1;
}
void UTVdjmVcardTileViewItemWidget::OnOffSelectVisuals_Implementation(bool isSelected)
{
	UE_LOG(LogTileViewItemWidget,Log,TEXT("UTVdjmVcardTileViewItemWidget::OnOffSelectVisuals_Implementation called. isSelected %s"),isSelected?TEXT("true"):TEXT("false"));
}
void UTVdjmVcardTileViewItemWidget::NativeOnListItemObjectSet(UObject* ListItemObject)
{
	if (ListItemObject)
	{
		UE_LOG(LogTileViewItemWidget,Log,TEXT("UTVdjmVcardTileViewItemWidget::NativeOnListItemObjectSet called. Item %s"),*ListItemObject->GetName());
		LinkedTileViewItemData = Cast<UTVdjmVcardTileViewItemData>(ListItemObject);
		if (LinkedTileViewItemData.IsValid())
		{
			LinkedTileViewItemData->PostBindToTileViewItemWidget(this);
			ITVdjmVcardInteractable::Execute_OnVcardActivate(LinkedTileViewItemData.Get());
		}
	}
}

void UTVdjmVcardTileViewItemWidget::NativeOnItemSelectionChanged(bool bIsSelected)
{
	IsSelected = bIsSelected;
	if (IsSelected)
	{
		CallOnVcardSelect(this);
	}
	else
	{
		CallOnVcardDeSelect();
	}
}

void UTVdjmVcardTileViewItemData::OnVcardDeSelect_Implementation()
{
}

void UTVdjmVcardTileViewItemData::OnVcardSelect_Implementation(UObject* target)
{
	UE_LOG(LogTileViewItemData,Log,TEXT("UTVdjmVcardTileViewItemData::OnVcardSelect called. Target %s"),target?*target->GetName():TEXT("nullptr"));
}

void UTVdjmVcardTileViewItemData::PostBindToTileViewItemWidget_Implementation(
	const TScriptInterface<IUserObjectListEntry>& itemWidget)
{
	if (itemWidget && itemWidget.GetObject()->GetClass()->ImplementsInterface(UTVdjmVcardInteractable::StaticClass()))
	{
		UTVdjmVcardTileViewItemWidget* castedWidget = Cast<UTVdjmVcardTileViewItemWidget>(itemWidget.GetObject());
		if (castedWidget)
		{
			castedWidget->LinkedTileViewItemData = TWeakObjectPtr<UTVdjmVcardTileViewItemData>(this);
		}
	}
}

bool UTVdjmVcardTileViewDataAsset::DbcValidDataAssetSetup() const
{
	return ItemLoaderClass != nullptr && ItemDataClass != nullptr && ItemWidgetClass != nullptr && ItemDataList.Num() > 0;
}

void UTVdjmVcardTileViewItemWidget::CallOnVcardSelect(UObject* target)
{
	if (LinkedTileViewItemData.IsValid())
	{	
		/*	TODO:
		 *		command 를 여기에 넣어야함. 커맨드를 여기에서 호출하게 만들어줘야함.
		 *		
		 * 
		 */
		
		UE_LOG(LogTileViewItemWidget,Log,TEXT("UTVdjmVcardTileViewItemWidget::CallOnVcardSelect called. %s "),target?*target->GetName():TEXT("nullptr"));
		ITVdjmVcardInteractable::Execute_OnVcardSelect(LinkedTileViewItemData.Get(),target);
	}
}

void UTVdjmVcardTileViewItemWidget::CallOnVcardDeSelect()
{
	if (LinkedTileViewItemData.IsValid())
	{
		ITVdjmVcardInteractable::Execute_OnVcardDeSelect(LinkedTileViewItemData.Get());
	}
}

void UTVdjmVcardTileViewItemWidget::OnVcardDeSelect_Implementation()
{
	UE_LOG(LogTileViewItemWidget,Log,TEXT("UTVdjmVcardTileViewItemWidget::OnVcardDeSelect_Implementation called."));
	OnOffSelectVisuals(false);
}

void UTVdjmVcardTileViewItemWidget::OnVcardSelect_Implementation(UObject* target)
{
	UE_LOG(LogTileViewItemWidget,Log,TEXT("UTVdjmVcardTileViewItemWidget::OnVcardSelect_Implementation called."));
	OnOffSelectVisuals(true);
}


bool UTVdjmVcardTileViewItemLoader::LoadDataAssetForTileView_Implementation(UTileView* tileViewWidget,
	UTVdjmVcardTileViewDataAsset* dataAsset, UTVdjmVcardTileViewMainWidget* ownerTileViewMainWidget)
{
	if (tileViewWidget && dataAsset && ownerTileViewMainWidget)
	{
		ItemDataClass = dataAsset->ItemDataClass;
		ItemWidgetClass = dataAsset->ItemWidgetClass;
	
		mLinkedTileViewWidget= tileViewWidget;
		mLinkedTileViewMainWidget = ownerTileViewMainWidget;
		mLinkedTileViewDataAsset = dataAsset;
			
		if (UTVdjmVcardTileViewWidget* castedTileViewWidget = Cast<UTVdjmVcardTileViewWidget>(mLinkedTileViewWidget.Get()))
		{
			castedTileViewWidget->ChangeTileViewItemWidgetClass(ItemWidgetClass);
		}
		return true;
	}
	UE_LOG(LogTileViewItemLoader,Warning,TEXT("UTVdjmVcardTileViewItemLoader::LoadDataAssetForTileView_Implementation failed due to null parameter."));
	return false;
}

bool UTVdjmVcardTileViewItemLoader::CheckSkipTileViewItemKeys(const TArray<FName> stringKeys,const TArray<FName> imageKeys, const TObjectPtr<UTVdjmVcardTileViewDataItemDesc>& tileViewItemDesc)
{
	bool onSkip = false;
	for (const FName& reqKey :stringKeys)
	{
		if (not tileViewItemDesc->StringTable.Contains(reqKey))
		{
			onSkip = true;
			break;
		}
	}
	for (const FName& reqKey :imageKeys)
	{
		if (not tileViewItemDesc->ImageTable.Contains(reqKey))
		{
			onSkip = true;
			break;
		}
	}
	return onSkip;
}

void UTVdjmVcardTileViewMainWidget::OnBeginLoadItems(UTVdjmVcardTileViewItemLoader* caller)
{
	EntrustBeginFlag = false;
	LoadStartTime = UGameplayStatics::GetRealTimeSeconds(GetWorld());
	CurrentTileViewItemLoader = caller;
	
	GetWorld()->GetTimerManager().SetTimer(
		LoadExceedFallbackTimerHandle,
		FTimerDelegate::CreateUObject(
			this,
			&UTVdjmVcardTileViewMainWidget::LoadExceedFallback
		),
		MaxLoadExceedTime,
		false
	);
}


void UTVdjmVcardTileViewMainWidget::LoadExceedFallback()
{
	//	어쩌다가 잘 되서 이게 호출은 되었는데, 현재 로딩중이 아니면 무시
	if (IsEntrustItems())
	{
		UE_LOG(LogTileViewMain,Warning,TEXT("UTVdjmVcardTileViewMainWidget::LoadExceedFallback called. Load time exceeded maximum of %.3f seconds."),MaxLoadExceedTime);
		//	시작할때 넣었던 놈이 제대로 되어 있으면 끝내는 처리
		if (CurrentTileViewItemLoader.IsValid())
		{
			CurrentTileViewItemLoader->LoadEndedTileViewItems();
		}
		//	내부에서 제대로 끝내주는 함수 호출
		OnEndLoadItems();
	}
	else
	{
		UE_LOG(LogTileViewMain,Warning,TEXT("UTVdjmVcardTileViewMainWidget::LoadExceedFallback called but not in Entrusting state."));
	}
}

void UTVdjmVcardTileViewMainWidget::BeginEntrustItemInstance(UTVdjmVcardTileViewItemLoader* caller)
{
	if (IsEntrustPossible())
	{
		OnBeginLoadItems(caller);
		if (TileViewWidget)
		{
			TileViewWidget->ClearListItems();
		}
		ItemInstanceList.Empty();
	}
	else
	{
		UE_LOG(LogTileViewMain,Warning,TEXT("UTVdjmVcardTileViewMainWidget::EntrustItemInstanceBegin called while not in Idle state."));
	}
}
void UTVdjmVcardTileViewMainWidget::OnEndLoadItems()
{
	if (IsEntrustItems())
	{
		EntrustBeginFlag = true;
		float loadEndTime = UGameplayStatics::GetRealTimeSeconds(GetWorld());
		UE_LOG(LogTileViewMain,Log,TEXT("UTVdjmVcardTileViewMainWidget::OnEndLoadItems called. Loaded %d items in %.3f seconds."),ItemInstanceList.Num(),loadEndTime - LoadStartTime);
	}

}
void UTVdjmVcardTileViewMainWidget::EntrusNewInstance(UTVdjmVcardTileViewItemData* itemData)
{
	if (itemData && IsEntrustItems())
	{
		itemData->LinkedTileViewMainWidget = this;
		ItemInstanceList.Add(itemData);
		if (IsValid(TileViewWidget))
		{
			TileViewWidget->AddItem(itemData);	//	after to NativeOnListItemObjectSet(itemData)
			UE_LOG(LogTileViewMain,Log,TEXT("UTVdjmVcardTileViewMainWidget::EntrusNewInstance added item %s to TileView."),*itemData->GetName());
		}
	}
}

void UTVdjmVcardTileViewMainWidget::EntrustAppendNewInstances(TArray<UTVdjmVcardTileViewItemData*> itemDatas)
{
}

void UTVdjmVcardTileViewMainWidget::EndEntrustItemInstance()
{
	if (IsEntrustItems())
	{
		OnEndLoadItems();
	}
	else
	{
		UE_LOG(LogTileViewMain,Warning,TEXT("UTVdjmVcardTileViewMainWidget::EntrustItemInstanceEnd called while not in Entrusting state."));
	}
}

void UTVdjmVcardTileViewMainWidget::OnVcardDeSelect_Implementation()
{
	
}

void UTVdjmVcardTileViewMainWidget::OnVcardSelect_Implementation(UObject* target)
{
	if (CurrentSelectedItemData.IsValid() && CurrentSelectedItemData.Get() != target)
	{
		UE_LOG(LogTileViewMain,Log,TEXT("_{ UTVdjmVcardTileViewMainWidget::OnVcardSelect_Implementation: Deselecting previous item %s."),*CurrentSelectedItemData->GetName());
		//	To ItemData
		ITVdjmVcardInteractable::Execute_OnVcardDeSelect(CurrentSelectedItemData.Get());
	}
	
	CurrentSelectedItemData = nullptr;
	
	if (UTVdjmVcardTileViewItemData* curSelect = Cast<UTVdjmVcardTileViewItemData>(target))
	{
		UE_LOG(LogTileViewMain,Log,TEXT("_{ UTVdjmVcardTileViewMainWidget::OnVcardSelect_Implementation: Target is valid %s."),*curSelect->GetName());
		CurrentSelectedItemData = curSelect;
	}
}


void UTVdjmVcardTileViewBgItemData::PostBindToTileViewItemWidget_Implementation(
	const TScriptInterface<IUserObjectListEntry>& itemWidget)
{
	Super::PostBindToTileViewItemWidget_Implementation(itemWidget);
	if (UTVdjmVcardTileViewBgItemWidget* castedWidget = Cast<UTVdjmVcardTileViewBgItemWidget>(itemWidget.GetObject()))
	{
		LinkedTileViewItemWidget = castedWidget;
		
		castedWidget->BackgroundName = BackgroundName;
		castedWidget->ThumbnailImage = BackgroundImage;
		
		if (UBorder* target = castedWidget->ThumbnailImageWidget)
		{
			target->SetBrushFromTexture(BackgroundThumbnailImage);
		}
	}
}

void UTVdjmVcardTileViewBgItemData::OnVcardDeSelect_Implementation()
{
	if (LinkedTileViewItemWidget.IsValid())
	{
		ITVdjmVcardInteractable::Execute_OnVcardDeSelect(LinkedTileViewItemWidget.Get());
	}
}

void UTVdjmVcardTileViewBgItemData::OnVcardSelect_Implementation(UObject* target)
{
	if (target && target->IsA<UTVdjmVcardTileViewItemWidget>())
	{
		
		if (LinkedTileViewMainWidget.IsValid())
		{
			ITVdjmVcardInteractable::Execute_OnVcardSelect(LinkedTileViewMainWidget.Get(),this);//	to Data
		}
	
		if (LinkedTileViewItemWidget.IsValid())
		{
			ITVdjmVcardInteractable::Execute_OnVcardSelect(LinkedTileViewItemWidget.Get(),this);
		}
		else
		{
			//UE_LOG(LogTemp,Warning,TEXT("!!!!!!!!LinkedTileViewItemWidget is invalid in UTVdjmVcardTileViewBgItemData::OnVcardSelect_Implementation"));
		}

		//	여기에서 Background 를 변화시키는걸 직접 호출해줌.
		if (ATVdjmVcardBackgroundActor* backgroundActor = ATVdjmVcardMainUiBridge::TryGetBackgroundActorStaticWorld())
		{
		
			ITVdjmVcardInteractable::Execute_OnVcardSelect(backgroundActor,this);
		}
	}
	else //	이게 맞겠지?...
	{
		
		if (LinkedTileViewMainWidget.IsValid())
		{
			ITVdjmVcardInteractable::Execute_OnVcardSelect(LinkedTileViewMainWidget.Get(),nullptr);//	to Data
		}
		// else
		// {
		// 	UE_LOG(LogTemp,Warning,TEXT("OwnerTileViewWidget is invalid in UTVdjmVcardTileViewBgItemData::OnVcardSelect_Implementation"));
		// }
	
		if (LinkedTileViewItemWidget.IsValid())
		{
			ITVdjmVcardInteractable::Execute_OnVcardSelect(LinkedTileViewItemWidget.Get(),this);
		}
		// else
		// {
		// 	UE_LOG(LogTemp,Warning,TEXT("LinkedTileViewItemWidget is invalid in UTVdjmVcardTileViewBgItemData::OnVcardSelect_Implementation"));
		// }

		//	여기에서 Background 를 변화시키는걸 직접 호출해줌.
		if (ATVdjmVcardBackgroundActor* backgroundActor = ATVdjmVcardMainUiBridge::TryGetBackgroundActorStaticWorld())
		{
			ITVdjmVcardInteractable::Execute_OnVcardDeSelect(backgroundActor);
		}
		// else
		// {
		// 	UE_LOG(LogTemp,Warning,TEXT("BackgroundActor is invalid in UTVdjmVcardTileViewBgItemData::OnVcardSelect_Implementation"));
		// }
	}
}

bool UTVdjmVcardTileViewBgItemLoader::LoadDataAssetForTileView_Implementation(UTileView* tileView,	UTVdjmVcardTileViewDataAsset* dataAsset, UTVdjmVcardTileViewMainWidget* ownerMainWidget)
{
	if (Super::LoadDataAssetForTileView_Implementation(tileView, dataAsset, ownerMainWidget))
	{
		if (DbcValidDefaultLoaderSetup() && mLinkedTileViewMainWidget->IsEntrustPossible())
		{
			constexpr int32 paramStringTable = 0;
			constexpr int32 paramImageTable = 1;
			
			const TArray<FName> stringKeys = GetRequiredParamKeys(paramStringTable);
			const TArray<FName>	imageKeys = GetRequiredParamKeys(paramImageTable);
			
			mLinkedTileViewMainWidget->BeginEntrustItemInstance(this);
	
			for (const TObjectPtr<UTVdjmVcardTileViewDataItemDesc>& tileViewItemDesc : mLinkedTileViewDataAsset->ItemDataList)
			{
				if (IsValid(tileViewItemDesc))
				{
					if (CheckSkipTileViewItemKeys(stringKeys, imageKeys, tileViewItemDesc))
					{
						UE_LOG(LogTileViewItemLoader,Warning,TEXT("Skipping tileViewItemDesc due to missing required keys in UTVdjmVcardTileViewBgItemLoader::LoadDataAssetForTileView_Implementation"));
						continue;
					}

					const FString bgName = tileViewItemDesc->StringTable[stringKeys[0]];
					const TSoftObjectPtr<UTexture2D> bgThumbnailPtr	= tileViewItemDesc->ImageTable[imageKeys[0]];
					const TSoftObjectPtr<UTexture2D> bgBackImg = tileViewItemDesc->ImageTable[imageKeys[1]];
					
					if (UTVdjmVcardTileViewBgItemData* castedData =Cast<UTVdjmVcardTileViewBgItemData>(NewObject<UObject>(mLinkedTileViewMainWidget.Get(), ItemDataClass)))
					{
						castedData->BackgroundName = bgName;
						castedData->BackgroundThumbnailImage = bgThumbnailPtr.LoadSynchronous();
						castedData->BackgroundImage = bgBackImg.LoadSynchronous();
						
						if (castedData->IsStoredDataValid_Implementation())
						{
							mLinkedTileViewMainWidget->EntrusNewInstance(castedData);
						}
						else
						{
							castedData = nullptr;
							UE_LOG(LogTileViewItemLoader,Warning,TEXT("castedData is not valid in UTVdjmVcardTileViewBgItemLoader::LoadDataAssetForTileView_Implementation"));
						}
					}
					else{UE_LOG(LogTileViewItemLoader,Warning,TEXT("Failed to create ItemData instance in UTVdjmVcardTileViewBgItemLoader::LoadDataAssetForTileView_Implementation"));}
				}
				else{ UE_LOG(LogTileViewItemLoader,Warning,TEXT("tileViewItemDesc is invalid in UTVdjmVcardTileViewBgItemLoader::LoadDataAssetForTileView_Implementation"));}
			}
			mLinkedTileViewMainWidget->EndEntrustItemInstance();
		}
	}
	else
	{
		UE_LOG(LogTileViewItemLoader,Warning,TEXT("Super::LoadDataAssetForTileView_Implementation returned false in UTVdjmVcardTileViewBgItemLoader::LoadDataAssetForTileView_Implementation"));
		return false;
	}
	return true;
}

void UTVdjmVcardTileViewBgItemLoader::InitTileViewLoader_Implementation(UTileView* tileView, UTVdjmVcardTileViewDataAsset* dataAsset,
	UTVdjmVcardTileViewMainWidget* ownerWidget)
{
	Super::InitTileViewLoader_Implementation(tileView, dataAsset, ownerWidget);

	if (UTVdjmVcardTileViewDataAsset* tileviewDataAsset = Cast<UTVdjmVcardTileViewDataAsset>(dataAsset))
	{
		ItemDataClass = tileviewDataAsset->ItemDataClass;
		ItemWidgetClass = tileviewDataAsset->ItemWidgetClass;
	}
	mLinkedTileViewWidget= tileView;
	mLinkedTileViewMainWidget = ownerWidget;
	
	if (mLinkedTileViewWidget.IsValid())
	{
		if (UTVdjmVcardTileViewWidget* tileViewWidget = Cast<UTVdjmVcardTileViewWidget>(mLinkedTileViewWidget.Get()))
		{
			tileViewWidget->ChangeTileViewItemWidgetClass(ItemWidgetClass);
		}
	}
}

bool UTVdjmVcardTileViewMainWidget::StoreTileViewItems(const FSoftObjectPath& itemDataAssetPath)
{
	if (itemDataAssetPath.IsValid())
	{
		UObject* branchResolved = nullptr;
		const FString& itemDataPathStr = itemDataAssetPath.ToString();
		
		//	캐싱을 해둔게 있는지 확인
		if (TObjectPtr<UTVdjmVcardTileViewDataAsset>* found = LoadedDataAssetCache.Find(itemDataPathStr))
		{
			branchResolved = *found;
		}
		else
		{
			//	없으면 무조건 검색을 시작
			branchResolved = itemDataAssetPath.ResolveObject();
			
			if (branchResolved == nullptr)
			{
				branchResolved = itemDataAssetPath.TryLoad();
				if (branchResolved == nullptr)
				{
					UE_LOG(LogTileViewMain,Warning,TEXT("Failed to load ItemDataAssetPath %s synchronously."),*itemDataPathStr);
					return false;
				}
			}
			else
			{
				UE_LOG(LogTileViewMain,Log,TEXT("ItemDataAssetPath %s already loaded in memory."),*itemDataPathStr);
			}
		}
		
		UTVdjmVcardTileViewDataAsset* loadedDataAsset =  Cast<UTVdjmVcardTileViewDataAsset>(branchResolved);
	
		if (not IsValid(loadedDataAsset))
		{
			UE_LOG(LogTileViewMain,Warning,TEXT("Loaded object from ItemDataAssetPath %s is not a UDataAsset."),*itemDataPathStr);
			return false;
		}
		
		if (not loadedDataAsset->DbcValidDataAssetSetup())
		{
			if (loadedDataAsset->ItemLoaderClass == nullptr)
			{
				UE_LOG(LogTileViewMain,Warning,TEXT("LoadedDataAsset's ItemLoaderClass is null for asset path %s."),*itemDataPathStr);
			}
			if (loadedDataAsset->ItemDataClass == nullptr)
			{
				UE_LOG(LogTileViewMain,Warning,TEXT("LoadedDataAsset's ItemDataClass is null for asset path %s."),*itemDataPathStr);
			}
			if (loadedDataAsset->ItemWidgetClass == nullptr)
			{
				UE_LOG(LogTileViewMain,Warning,TEXT("LoadedDataAsset's ItemWidgetClass is null for asset path %s."),*itemDataPathStr);
			}
			if (loadedDataAsset->ItemDataList.IsEmpty())
			{
				UE_LOG(LogTileViewMain,Warning,TEXT("LoadedDataAsset's ItemDataList is empty for asset path %s."),*itemDataPathStr);
			}
			return false;
		}
		
		//	위에서 로드까지 마쳐진거라 삽입한다. 이미 찾아놓았어도 Add 는 대체하는건데 같은객체라도 동일한 과정을 위해 삽입.
		LoadedDataAssetCache.Add(itemDataPathStr, loadedDataAsset);
		
		UTVdjmVcardTileViewItemLoader* tileViewItemLoader = nullptr;
		//	아이템로더 캐시 확인
		if (LoadedItemLoaderCache.Find(itemDataPathStr))
		{
			tileViewItemLoader = *LoadedItemLoaderCache.Find(itemDataPathStr);
		}
		else
		{
			tileViewItemLoader = NewObject<UTVdjmVcardTileViewItemLoader>(this, loadedDataAsset->ItemLoaderClass);
			LoadedItemLoaderCache.Add(itemDataPathStr, tileViewItemLoader);
		}
		
		if (IsValid(tileViewItemLoader))
		{
			//tileViewItemLoader->InitTileViewLoader(TileViewWidget, loadedDataAsset, this);
			return tileViewItemLoader->LoadDataAssetForTileView(TileViewWidget, loadedDataAsset, this);
			// if (tileViewItemLoader->DbcValidClassesInLoader() )
			// {
			// 	
			// 	return true;
			// }
			// else
			// {
			// 	UE_LOG(LogTemp,Warning,TEXT("TileViewItemLoader is not valid after initialization."));
			// 	return false;
			// }
		}
		else
		{
			
			UE_LOG(LogTileViewMain,Warning,TEXT("TileViewItemLoader is not valid after initialization."));
			return false;
		}
	}
	else
	{
		UE_LOG(LogTileViewMain,Warning,TEXT("ItemDataAssetPath is not valid."));
	}
	return false;
}

void UTVdjmVcardTileViewMainWidget::RegisterTileViewWidget(const FSoftObjectPath& itemDataAssetPath)
{
	if (itemDataAssetPath.IsValid())
	{
		if (StoreTileViewItems(itemDataAssetPath))
		{
			FString tileviewFullName = FString::Printf(TEXT("%s-%s"),*TileViewMainName,*TileViewWidget->GetName());
			
			if (TileViewMainName.IsEmpty())
			{
				tileviewFullName = FString::Printf(TEXT("%s-%s"),*GetName(),*TileViewWidget->GetName());
			}

			UE_LOG(LogTileViewMain,Log,TEXT("UTVdjmVcardTileViewMainWidget::NativeConstruct registered tile view %s"),*tileviewFullName);
		
			ATVdjmVcardMainUiBridge::RegisterWidget_TileView(tileviewFullName.ToLower(),TileViewWidget);
		}
		else
		{
			UE_LOG(LogTileViewMain,Warning,TEXT("Failed to store tile view items for asset path %s"),*RecentItemDataAssetPath.ToString());
		}
	}
	else
	{
		UE_LOG(LogTileViewMain,Warning,TEXT("TileViewDataAssetPath is not valid in UTVdjmVcardTileViewMainWidget::NativeConstruct"));
	}
}

void UTVdjmVcardTileViewMainWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (UiBridgeActor == nullptr)
	{
		UiBridgeActor = ATVdjmVcardMainUiBridge::TryGetBridge(GetWorld());
	}

	if (UWidget* parent = ATVdjmVcardMainUiBridge::TravelParentClass(this,UTVdjmVcardMobileOptionContentsMain::StaticClass()))
	{
		if (UTVdjmVcardMobileOptionContentsMain* optionContentMain = Cast<UTVdjmVcardMobileOptionContentsMain>(parent))
		{
			TileViewDataAssetPath = optionContentMain->LoadDataAssetPath;
			RegisterTileViewWidget(TileViewDataAssetPath);
			UE_LOG(LogTileViewMain,Log,TEXT("UTVdjmVcardTileViewMainWidget::NativeConstruct found parent UTVdjmVcardMobileOptionContentsMain and registered TileView with data asset path %s"),*TileViewDataAssetPath.ToString());
		}
	}
	else
	{
		UE_LOG(LogTileViewMain,Log,TEXT("UTVdjmVcardTileViewMainWidget::NativeConstruct did not find parent UTVdjmVcardMobileOptionContentsMain, using own TileViewDataAssetPath %s"),*TileViewDataAssetPath.ToString());
		RegisterTileViewWidget(TileViewDataAssetPath);
	}
	
}


void UTVdjmVcardTileViewMainWidget::NativePreConstruct()
{
	Super::NativePreConstruct();
}

FVector2D UTVdjmVcardMobileMoveUiSolver::GetCurrentInputPoint(UPanelWidget* Panel)
{
	if (!Panel) return FVector2D::ZeroVector;

	// 1. 기본적으로 Slate의 커서(마우스/터치 통합) 위치를 가져옴
	FVector2D ResultPos = FSlateApplication::Get().GetCursorPos();

	// 2. 만약 커서 위치가 (0,0)이거나, 모바일 환경이라 확실하지 않다면 PlayerController 확인
	// (대부분의 경우 1번에서 해결되지만, 특수 상황 대비)
	if (ResultPos.IsZero()) 
	{
		if (UWorld* World = Panel->GetWorld())
		{
			if (APlayerController* PC = World->GetFirstPlayerController())
			{
				float MouseX, MouseY;
				// GetMousePosition은 Viewport 기준 좌표를 주지만, DPI Scale은 적용 안 된 픽셀 단위일 수 있음.
				// 하지만 AbsoluteToLocal을 쓰려면 '전체 화면 기준'이 필요하므로
				// 사실상 1번(GetCursorPos)이 가장 정확합니다.
                
				// 모바일 터치 좌표 확인 (인덱스 0번 손가락)
				bool bIsTouched = false;
				PC->GetInputTouchState(ETouchIndex::Touch1, MouseX, MouseY, bIsTouched);
				if (bIsTouched)
				{
					// 터치 좌표를 찾았다면 갱신 (보통 Viewport 좌표 = Screen 좌표)
					ResultPos = FVector2D(MouseX, MouseY);
				}
			}
		}
	}

	return ResultPos;
}

bool UTVdjmVcardMobileMoveUiSolver::GetInputPoint(FVector2D& outPoint)
{
	if (mParentPanelWidget.IsValid())
	{
		if (UWorld* World = mParentPanelWidget->GetWorld())
		{
			if (APlayerController* PC = World->GetFirstPlayerController())
			{
				float TouchX, TouchY;
				bool bIsPressed;
				PC->GetInputTouchState(ETouchIndex::Touch1, TouchX, TouchY, bIsPressed);

				if (bIsPressed)
				{
					FVector2D ViewportPos(TouchX, TouchY);
					// Viewport -> Absolute 변환
					FGeometry ViewportGeo = UWidgetLayoutLibrary::GetViewportWidgetGeometry(World);
					outPoint = ViewportGeo.LocalToAbsolute(ViewportPos);
					return true;
				}
			}
		}

		// 2순위: 마우스 (터치가 없을 때)
		if (FSlateApplication::Get().IsMouseAttached())
		{
			outPoint = FSlateApplication::Get().GetCursorPos();
			return true;
		}
	}
	return false;
}


void UTVdjmVcardMobileMoveUiSolver::InitializeMoveSolver(UPanelWidget* parentPanel, UPanelWidget* childWidget,float leftBound, float rightBound, float topBound, float bottomBound,bool useInitChildLocalTranslation)
{
	parentPanel->ForceLayoutPrepass();
	childWidget->ForceLayoutPrepass();

	{
		mParentPanelWidget = parentPanel;
		mChildWidget = childWidget;
		
		mBoundLeftRight.X = leftBound;
		mBoundLeftRight.Y = rightBound;
		mBoundTopBottom.X = topBound;
		mBoundTopBottom.Y = bottomBound;
		
		mInitChildAbsLocation = mChildWidget->GetCachedGeometry().GetAbsolutePosition();
		mInitChildLocalTranslation = mChildWidget->GetRenderTransform().Translation;

		mUseInitChildLocalTranslation = useInitChildLocalTranslation;
	}
	
	{
		CalculateMovableArea();
	}
}
void UTVdjmVcardMobileMoveUiSolver::CalculateMovableArea()
{
	const FVector2D childSize = mChildWidget->GetCachedGeometry().GetLocalSize();
	const FVector2D childScale = mChildWidget->GetRenderTransform().Scale;
	const FVector2D childPivot = mChildWidget->GetRenderTransformPivot();
	
	FVector2D initTranslation = (mUseInitChildLocalTranslation)? mInitChildLocalTranslation : FVector2D(0.f,0.f);
	
	FVector2D initLU = initTranslation - (childSize * childScale * childPivot);

	mMovableArea.Left = initLU.X - (childSize.X * childScale.X * mBoundLeftRight.X);
	mMovableArea.Top = initLU.Y - (childSize.Y * childScale.Y * mBoundTopBottom.X);
	mMovableArea.Right = initLU.X + (childSize.X * childScale.X * (1.f + mBoundLeftRight.Y));
	mMovableArea.Bottom = initLU.Y + (childSize.Y * childScale.Y * (1.f + mBoundTopBottom.Y));
	
}

void UTVdjmVcardMobileMoveUiSolver::StartEdit(float holdTime, int32 moveDirection,float updateInterval)
{
	if (mParentPanelWidget.IsValid()  && mChildWidget.IsValid())
	{
		mEvaluationHoldTime = holdTime;
		mMoveDirection = moveDirection;
		
		mParentPanelWidget->ForceLayoutPrepass();
		mChildWidget->ForceLayoutPrepass();

		{
		    mSnapshotAbsPoint = GetCurrentInputPoint(mParentPanelWidget.Get());
		    mSnapshotPointInParent = mParentPanelWidget->GetCachedGeometry().AbsoluteToLocal(mSnapshotAbsPoint);
    
		    mSnapshotTranslation = mChildWidget->GetRenderTransform().Translation;

			//	이거는 안쓸수도 있음
		    mSnapshotChildPivot = mChildWidget->GetRenderTransformPivot();
		    mSnapshotchildSize = mChildWidget->GetCachedGeometry().GetLocalSize() * mChildWidget->GetRenderTransform().Scale;
		    mSnapshotChildRect = GetChildWidgetRect();
		}
		//	원래 Movable 을 계산. mInitChildLocalTranslation 를 이용. InitializeMoveSolver 에서 획득
		CalculateMovableArea();
		
		OnBottomAndOptStart.Broadcast(mParentPanelWidget.Get(),  mChildWidget.Get(), moveDirection);
		
		//	Update Timer Start
		if (UWorld* world = mParentPanelWidget->GetWorld())
		{
			FTimerManager& timeMgr = world->GetTimerManager();
			if (timeMgr.IsTimerActive(mUpdateSliderHandle))
			{
				timeMgr.ClearTimer(mUpdateSliderHandle);
			}

			timeMgr.SetTimer(mUpdateSliderHandle, [this]()
			{
				if (mParentPanelWidget.IsValid() && mChildWidget.IsValid())
				{
					/*
					 * 제한을 거는 delta 값 계산
					 * 1. 부모판넬의 클릭 지점을 로컬로 변환
					 * 2. mSnapshotPointInParent 시점과의 거리를 계산
					 * 3. moveDirection 에 따라 축 제한
					 */
					FVector2D restrictedDelta = CalculateRestrictedDelta(
					mParentPanelWidget->GetCachedGeometry().
					AbsoluteToLocal(
						GetCurrentInputPoint(
							mParentPanelWidget.Get())
							) - mSnapshotPointInParent
					, mMoveDirection);

					/*
					 * restrictedDelta 를 통해서 local 에서 이동해야하는 만큼 Offset 이동
					 * 1. deltaLU 과 deltaRD 는 parentPanel 이고 실시간으로 계산되는 좌표임.
					 * 2. 이 좌표가 mMovableArea 를 벗어나는지 검사
                     * 3. 벗어난다면, 벗어난 만큼 restrictedDelta 에 보정
					 */
					FSlateRect tryMoveRect = mSnapshotChildRect.OffsetBy(restrictedDelta);

					//	LURD == left , top , right, bottom
					const FVector2D deltaLU = tryMoveRect.GetTopLeft();
					const FVector2D deltaRD = tryMoveRect.GetBottomRight();
					
					FWidgetTransform childTrans = mChildWidget->GetRenderTransform();
					
					if (not CheckCollisionRectArea(deltaLU, deltaRD))
					{
						if (deltaLU.X < mMovableArea.Left)
						{
							restrictedDelta.X += (mMovableArea.Left - deltaLU.X);
						}
						else if (deltaRD.X > mMovableArea.Right)
						{
							restrictedDelta.X += (mMovableArea.Right - deltaRD.X);
						}

						if (deltaLU.Y < mMovableArea.Top)
						{
							restrictedDelta.Y += (mMovableArea.Top - deltaLU.Y);
						}
						else if (deltaRD.Y > mMovableArea.Bottom)
						{
							restrictedDelta.Y += (mMovableArea.Bottom - deltaRD.Y);
						}
					}
					
					childTrans.Translation = mSnapshotTranslation + restrictedDelta;
					mLastTranslation = childTrans.Translation;
					mCorrectedTransform = childTrans;
					OnBottomAndOptUpdate.Broadcast(mParentPanelWidget.Get(),  mChildWidget.Get(), mMoveDirection);
					
					mChildWidget->SetRenderTransform(childTrans);
				}
			}, updateInterval, true,mEvaluationHoldTime);
		}
	}
}


void UTVdjmVcardMobileMoveUiSolver::BeginDestroy()
{
	StopUpdate();
	Super::BeginDestroy();
}

void UTVdjmVcardMobileMoveUiSolver::EndEdit()
{
	StopUpdate();
	OnBottomAndOptEnd.Broadcast();
}

void UTVdjmVcardMobileMoveUiSolver::StopUpdate()
{
	if (UWorld* world = GetWorld())
	{
		FTimerManager& timeMgr = world->GetTimerManager();
		timeMgr.ClearTimer(mUpdateSliderHandle);
	}	
}

FSlateRect UTVdjmVcardMobileMoveUiSolver::GetWidgetRect(UPanelWidget* widget) const
{
	FSlateRect result = FSlateRect();
	if (IsValid(widget))
	{
		widget->ForceLayoutPrepass();
		const FGeometry& childGeo = widget->GetCachedGeometry();
		const FWidgetTransform& WidgetTransform = widget->GetRenderTransform();

		FVector2D scaleSize = childGeo.GetLocalSize() * WidgetTransform.Scale * widget->GetRenderTransformPivot();
		
		const FVector2D lu = WidgetTransform.Translation - scaleSize;
		result = FSlateRect::FromPointAndExtent(lu, scaleSize);
	}

	return result;
}

FSlateRect UTVdjmVcardMobileMoveUiSolver::GetChildWidgetRect() const
{
	FSlateRect result;
	if (mChildWidget.IsValid())
	{
		result = GetWidgetRect(mChildWidget.Get());
	}
	return result;
}

FVector2D UTVdjmVcardMobileMoveUiSolver::CalculateRestrictedDelta(FVector2D deltaPoint, int32 moveDirection)
{
	FVector2D result = FVector2D::ZeroVector;
	if ( moveDirection & static_cast<int32>(ETVdjmVcardMoveDirection::EVerticality))
	{
		result.Y = 1.f;
	}

	if (moveDirection & static_cast<int32>(ETVdjmVcardMoveDirection::EHorizontality))
	{
		result.X = 1.f;
	}

	return result * deltaPoint;
}

bool UTVdjmVcardMobileMoveUiSolver::CheckCollisionRectArea(const FVector2D& deltaLU, const FVector2D& deltaRD) const
{
	bool result = true;
	if ( mMoveDirection & static_cast<int32>(ETVdjmVcardMoveDirection::EHorizontality))
	{
		if (deltaLU.X <  mMovableArea.Left ||  mMovableArea.Right < deltaRD.X)
		{
			result = false;
		}
	}

	if (mMoveDirection & static_cast<int32>(ETVdjmVcardMoveDirection:: EVerticality))
	{
		if (deltaLU.Y < mMovableArea.Top  || mMovableArea.Bottom < deltaRD.Y )
		{
			result = false;
		}
	}
	return result;
}

ATVdjmVcardBackgroundActor* ATVdjmVcardBackgroundActor::TryGetBackgroundActor(UWorld* world)
{
	ATVdjmVcardBackgroundActor* result = nullptr;
	bool spawnNew = true;
	if (world)
	{
		if (UTVdjmVcardConfigDataAsset* dataAsset =  UTVdjmVcardMobileMainWidget::TryGetLoadedConfigDataAsset())
		{
			for (TActorIterator<AActor> actorItr(world); actorItr; ++actorItr)
			{
				if (dataAsset->BackgroundActorClass && actorItr->IsA(ATVdjmVcardBackgroundActor::StaticClass()))
				{
					result = Cast<ATVdjmVcardBackgroundActor>( *actorItr );
					spawnNew = false;
					break;
				}
			}

			if (spawnNew)
			{
				if (IsValid(dataAsset->BackgroundActorClass))
				{
					FActorSpawnParameters spawnParams;
					spawnParams.Name = FName("VcardBackgroundActor");
					spawnParams.NameMode = FActorSpawnParameters::ESpawnActorNameMode::Requested;
					spawnParams.ObjectFlags |= RF_Transient;
					spawnParams.Owner = ATVdjmVcardMainUiBridge::TryGetBridge(world);

					result = Cast<ATVdjmVcardBackgroundActor>(world->SpawnActor(dataAsset->BackgroundActorClass, &dataAsset->BackgroundActorSpawnTransform, spawnParams));
				}
				else
				{
					UE_LOG(LogBackgroundActor,Warning,TEXT("BackgroundActorClass is invalid in VcardConfigDataAsset."));
				}
			}
		}
	}
	return result;
}
ATVdjmVcardBackgroundActor::ATVdjmVcardBackgroundActor()
{
	PrimaryActorTick.bCanEverTick = false;
	
	RootComp = CreateDefaultSubobject<USceneComponent>(TEXT("RootComp"));
	SetRootComponent(RootComp);
	BackgroundComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BackgroundComp"));
	BackgroundComp->SetupAttachment(RootComp);
}


void ATVdjmVcardBackgroundActor::BeginPlay()
{
	Super::BeginPlay();

	if (BackgroundComp)
	{
		if (UMaterialInterface* defaultMat = BackgroundComp->GetMaterial(0))
		{
			if (BackgroundMaterialInstance == nullptr)
			{
				BackgroundMaterialInstance = UMaterialInstanceDynamic::Create(defaultMat, this);
			}
			BackgroundComp->SetMaterial(0, BackgroundMaterialInstance);
		}
	}

	OnBackgroundSelected.AddDynamic(this,&ATVdjmVcardBackgroundActor::ATVdjmVcardBackgroundActor::SizeModify );
}

void UTVdjmVcardTileViewMotionItemLoader::InitTileViewLoader_Implementation(UTileView* tileViewWidget, UTVdjmVcardTileViewDataAsset* dataAsset,	UTVdjmVcardTileViewMainWidget* ownerTileViewMainWidget)
{
	Super::InitTileViewLoader_Implementation(tileViewWidget, dataAsset, ownerTileViewMainWidget);

	if (dataAsset)
	{
		ItemDataClass = dataAsset->ItemDataClass;
		ItemWidgetClass = dataAsset->ItemWidgetClass;
	}
	
	mLinkedTileViewWidget= tileViewWidget;
	mLinkedTileViewMainWidget = ownerTileViewMainWidget;
	
	if (mLinkedTileViewWidget.IsValid())
	{
		if (UTVdjmVcardTileViewWidget* castedTileViewWidget = Cast<UTVdjmVcardTileViewWidget>(mLinkedTileViewWidget.Get()))
		{
			castedTileViewWidget->ChangeTileViewItemWidgetClass(ItemWidgetClass);
		}
	}
}

bool UTVdjmVcardTileViewMotionItemLoader::LoadDataAssetForTileView_Implementation(UTileView* tileView, UTVdjmVcardTileViewDataAsset* dataAsset, UTVdjmVcardTileViewMainWidget* ownerMainWidget)
{
	if (Super::LoadDataAssetForTileView_Implementation(tileView, dataAsset, ownerMainWidget))
	{
		if (DbcValidDefaultLoaderSetup() && mLinkedTileViewMainWidget->IsEntrustPossible())
		{
			constexpr int32 paramImageTable = 1;
			constexpr int32 paramStringTable = 0;
			
			const TArray<FName> stringKeys = GetRequiredParamKeys(paramStringTable);
			const TArray<FName>	imageKeys = GetRequiredParamKeys(paramImageTable);
			
			mLinkedTileViewMainWidget->BeginEntrustItemInstance(this);
			
			for (const TObjectPtr<UTVdjmVcardTileViewDataItemDesc>& tileViewItemDesc : mLinkedTileViewDataAsset->ItemDataList)
			{
				if (IsValid(tileViewItemDesc))
				{
					if (CheckSkipTileViewItemKeys(stringKeys, imageKeys,tileViewItemDesc))
					{
						continue;
					}

					const FString motionName = tileViewItemDesc->StringTable[stringKeys[0]];
					const FString motionPath = tileViewItemDesc->StringTable[stringKeys[1]];
					
					const TSoftObjectPtr<UTexture2D> bgThumbnailPtr	= tileViewItemDesc->ImageTable[imageKeys[0]];
					
					if (UTVdjmVcardTileViewMotionItemData* castedData =  Cast<UTVdjmVcardTileViewMotionItemData>(NewObject<UObject>(mLinkedTileViewMainWidget.Get(), ItemDataClass)))
					{
						FSoftObjectPath motionAssetPath(motionPath);
						if (mPendingMotionItemDataMap.Contains(motionAssetPath.ToString()))
						{
							castedData = nullptr;
							continue;
						}
						
						castedData->MotionName = motionName;
						castedData->MotionThumbnailImage = bgThumbnailPtr.LoadSynchronous();
						//	Async Load
						castedData->LoadRequestUniqueId =
							motionAssetPath.LoadAsync(
							FLoadSoftObjectPathAsyncDelegate::CreateLambda
							([
                                	weakTarget = TWeakObjectPtr<UTVdjmVcardTileViewMotionItemData>( castedData),
                                	weakLoader = TWeakObjectPtr<UTVdjmVcardTileViewMotionItemLoader>(this)
                                	]
                                	(const FSoftObjectPath& path, UObject* loadedObj)
                                {
                                	UAnimSequence* motionAnim = nullptr;
								
                                	if (weakTarget.IsValid())
                                	{
                                		if (loadedObj && loadedObj->IsA<UAnimSequence>())
                                		{
                                			motionAnim = Cast<UAnimSequence>(loadedObj);
											weakTarget->MotionAnimation = motionAnim;
										}
                                	}
                                	if (motionAnim && weakLoader.IsValid())
                                	{
                                		weakLoader->TryAddAnimation(path.ToString(),motionAnim);
                                	}
                                }
							));//	Async Load Callback
						UE_LOG(LogTileViewItemLoader,Log,TEXT("Started async load for motion animation %s"),*motionAssetPath.ToString());
						mPendingMotionItemDataMap .Add(motionAssetPath.ToString(),castedData);
					}
				}
			}
		}
	}
	else
	{
		return false;
	}
	return true;
}

void UTVdjmVcardTileViewMotionItemLoader::RemoveAnimationLoadRequest(const FString& path)
{
	mPendingMotionItemDataMap.Remove(path);
	if (mPendingMotionItemDataMap.IsEmpty())
	{
		LoadEndedTileViewItems();
		mLinkedTileViewMainWidget->EndEntrustItemInstance();
	}
}
void UTVdjmVcardTileViewMotionItemLoader::LoadEndedTileViewItems()
{
	mPendingMotionItemDataMap.Empty();
}

void UTVdjmVcardTileViewMotionItemLoader::TryAddAnimation(const FString& path, UAnimSequence* anim)
{
	if (IsInGameThread())
	{
		if (anim && DbcValidDefaultLoaderSetup())
		{
			if (TObjectPtr<UTVdjmVcardTileViewMotionItemData>* found = mPendingMotionItemDataMap.Find(path))
			{
				UTVdjmVcardTileViewMotionItemData* target = (*found).Get();
				
				target->LoadedMotionAnimation = true;
				mLinkedTileViewMainWidget->EntrusNewInstance(target);
				
				UE_LOG(LogTileViewItemLoader,Log,TEXT("Successfully loaded motion animation for item %s"),*path);
			}
		}
	}
	RemoveAnimationLoadRequest(path);
}

void UTVdjmVcardTileViewMotionItemData::PostBindToTileViewItemWidget_Implementation(
	const TScriptInterface<IUserObjectListEntry>& itemWidget)
{
	Super::PostBindToTileViewItemWidget_Implementation(itemWidget);
	if (UTVdjmVcardTileViewMotionItemWidget* castedWidget = Cast<UTVdjmVcardTileViewMotionItemWidget>(itemWidget.GetObject()))
	{
		LinkedTileViewItemWidget = castedWidget;

		castedWidget->MotionName = MotionName;
		if (UTextBlock* texBlock = castedWidget->MotionTextWidget)
		{
			texBlock->SetText(FText::FromString(MotionName));
		}
		
		if (UBorder* border = castedWidget->ThumbnailImageWidget)
		{
			border->SetBrushFromTexture(MotionThumbnailImage);
		}
	}
}

void UTVdjmVcardTileViewMotionItemData::OnVcardDeSelect_Implementation()
{
	if (LinkedTileViewItemWidget.IsValid())
	{
		ITVdjmVcardInteractable::Execute_OnVcardDeSelect(LinkedTileViewItemWidget.Get());
	}
}
void UTVdjmVcardTileViewMotionItemData::ASyncLoadCallback_Implementation(const FSoftObjectPath&, UObject*)
{
	
}
void UTVdjmVcardTileViewMotionItemData::OnVcardSelect_Implementation(UObject* target)
{
	if (target && target->IsA<UTVdjmVcardTileViewItemWidget>())
	{
		if (LoadedMotionAnimation)
		{
			if (LinkedTileViewMainWidget.IsValid())
			{
				//	To UTVdjmVcardTileViewMainWidget
				ITVdjmVcardInteractable::Execute_OnVcardSelect(LinkedTileViewMainWidget.Get(),this);
			}
	
			if (LinkedTileViewItemWidget.IsValid())
			{
				//	To UTVdjmVcardTileViewItemWidget
				ITVdjmVcardInteractable::Execute_OnVcardSelect(LinkedTileViewItemWidget.Get(),this);
			}
			if (not LinkedMotionActor.IsValid())
			{
				LinkedMotionActor = ATVdjmVcardMainUiBridge::TryGetMotionActorStaticWorld();
			}
		
			if (LinkedMotionActor.IsValid())
			{
				ITVdjmVcardInteractable::Execute_OnVcardSelect(LinkedMotionActor.Get(),this);
			}
		}
		else
		{
			UE_LOG(LogTileViewItemData,Warning,TEXT("Motion Animation is not loaded yet for item %s"),*MotionName);
		}
	}
	else //	이게 맞겠지?...
	{
		if (LinkedTileViewMainWidget.IsValid())
		{
			ITVdjmVcardInteractable::Execute_OnVcardSelect(LinkedTileViewMainWidget.Get(),nullptr);//	to Data
		}
		
		if (LinkedTileViewItemWidget.IsValid())
		{
			ITVdjmVcardInteractable::Execute_OnVcardSelect(LinkedTileViewItemWidget.Get(),this);
		}
		
		if (ATVdjmVcardBackgroundActor* backgroundActor = ATVdjmVcardMainUiBridge::TryGetBackgroundActorStaticWorld())
		{
			ITVdjmVcardInteractable::Execute_OnVcardDeSelect(backgroundActor);
		}
		
	}
}


void ATVdjmVcardBackgroundActor::SizeModify_Implementation(	ATVdjmVcardBackgroundActor* backgroundActor, bool isSelected)
{
	if (IsValid(backgroundActor))
	{
		if (isSelected)
		{
			UObject* worldContext = backgroundActor;
			int32 playerIndex = 0;

			APawn* playerPawn = UGameplayStatics::GetPlayerPawn(worldContext, playerIndex);
			APlayerCameraManager* pcm = UGameplayStatics::GetPlayerCameraManager(worldContext, playerIndex);
			UStaticMeshComponent* bgPlaneComp = backgroundActor->BackgroundComp.Get();
			
			UStaticMesh* bgPlanMesh = (bgPlaneComp) ? bgPlaneComp->GetStaticMesh() : nullptr;
			UTexture2D* bgTexture = (backgroundActor->CurrentSelectedBgItemData.IsValid()) ? 
				backgroundActor->CurrentSelectedBgItemData->BackgroundImage : nullptr;

			if (playerPawn && pcm && bgPlaneComp && bgPlanMesh && bgTexture)
			{
				backgroundActor->SetActorTransform(playerPawn->GetActorTransform());

				// ----------------------------------------------------------------
				// [Step 1] 카메라 및 화면 정보 계산
				// ----------------------------------------------------------------
				FVector CamLoc = pcm->GetCameraLocation();
				FRotator CamRot = pcm->GetCameraRotation();
				
				// 카메라의 기준 벡터들 (World Space)
				FVector CamFwd = CamRot.Vector();
				FVector CamUp = CamRot.RotateVector(FVector::UpVector);
				FVector CamRight = CamRot.RotateVector(FVector::RightVector);

				// 화면 크기 및 타겟 사이즈 계산 (Envelope Logic)
				const float fixedDist = backgroundActor->BillboardDistance;
				float texWid = FMath::Max((float)bgTexture->GetSizeX(), 1.0f);
				float texHei = FMath::Max((float)bgTexture->GetSizeY(), 1.0f);
				float texRatio = texWid / texHei;

				FVector2D ViewportSize;
				if (backgroundActor->GetWorld()->GetGameViewport())
					backgroundActor->GetWorld()->GetGameViewport()->GetViewportSize(ViewportSize);
				else
					ViewportSize = FVector2D(1920.f, 1080.f);

				float screenRatio = (ViewportSize.Y > 0) ? (ViewportSize.X / ViewportSize.Y) : 1.77f;
				float FOV_Rad = FMath::DegreesToRadians(pcm->GetFOVAngle());
				float visibleWid = 2.0f * fixedDist * FMath::Tan(FOV_Rad * 0.5f);
				float visibleHei = visibleWid / screenRatio;

				float TargetWidth = visibleWid;
				float TargetHeight = visibleHei;

				if (texRatio > screenRatio)
				{
					TargetHeight = visibleHei;
					TargetWidth = TargetHeight * texRatio;
				}
				else
				{
					TargetWidth = visibleWid;
					TargetHeight = TargetWidth / texRatio;
				}

				// ----------------------------------------------------------------
				// [Step 2] 메시의 형태 분석 (두께, 가로, 세로 축 찾기)
				// ----------------------------------------------------------------
				FVector BoxSize = bgPlanMesh->GetBoundingBox().GetSize();
				
				// 1. 가장 얇은 축(Thickness) 찾기 -> 이것이 Normal이 됨
				float minSize = BoxSize.Z;
				EAxis::Type ThicknessAxis = EAxis::Z;

				if (BoxSize.X < minSize) { ThicknessAxis = EAxis::X; minSize = BoxSize.X; }
				if (BoxSize.Y < minSize) { ThicknessAxis = EAxis::Y; minSize = BoxSize.Y; }

				// ----------------------------------------------------------------
				// [Step 3] 회전 매트릭스 구성 (Basis Vector Mapping)
				// ----------------------------------------------------------------
				// 목표: 메시의 Thickness축 -> -CamFwd (카메라 보기)
				//       메시의 Up축        -> CamUp (화면 위쪽)
				//       메시의 Right축     -> CamRight (화면 오른쪽)
				
				FVector TargetAxisX, TargetAxisY, TargetAxisZ; // 회전 후의 X, Y, Z가 될 벡터
				FVector FinalScaleVector; // 최종 스케일 값

				float ThicknessVal = 8.0f; // 혹은 minSize, 혹은 1.0f

				static bool flip = true;
				
				switch (ThicknessAxis)
				{
				case EAxis::X: // [벽 형태] X가 두께
					// X가 Normal(앞뒤), Z가 Up(위아래), Y가 Right(좌우)라고 가정 (언리얼 표준)
					// 목표: X -> -CamFwd, Z -> CamUp, Y -> CamRight
					TargetAxisX = -CamFwd;
					TargetAxisZ = CamUp;
					TargetAxisY = CamRight;
					
					// 스케일 매핑: X=두께, Y=가로, Z=세로
					FinalScaleVector = FVector(ThicknessVal,TargetHeight * 0.01f, TargetWidth * 0.01f);
					
					
					break;

				case EAxis::Y: // [옆면 형태] Y가 두께
					// Y가 Normal, Z가 Up, X가 Right라고 가정
					// 목표: Y -> -CamFwd, Z -> CamUp, X -> CamRight
					TargetAxisY = -CamFwd;
					TargetAxisZ = CamUp;
					TargetAxisX = CamRight;
					FinalScaleVector = FVector( TargetHeight * 0.01f , ThicknessVal,TargetWidth * 0.01f );
					// 스케일 매핑: X=가로, Y=두께, Z=세로
					

				case EAxis::Z:
					// [바닥 형태 - 일반 Plane] Z가 두께
					// Z가 Normal(바닥), X가 Up(UV상 위쪽), Y가 Right(UV상 오른쪽)라고 가정 (표준 Plane UV)
					// 목표: Z -> -CamFwd, X -> CamUp, Y -> CamRight
					
					// 주의: Z를 -Fwd로 보내고, X를 Up으로 보내면, Y는 자동으로 Right가 됨 (Cross Product 원리)
					TargetAxisZ = -CamFwd;
					TargetAxisX = CamUp;    // X축을 화면의 세로(Up)로 매핑
					TargetAxisY = CamRight; // Y축을 화면의 가로(Right)로 매핑

					// 스케일 매핑: 
					// X축이 CamUp(세로)에 매핑되었으므로 -> X Scale = TargetHeight
					// Y축이 CamRight(가로)에 매핑되었으므로 -> Y Scale = TargetWidth
					// Z축이 Normal(두께)에 매핑되었으므로 -> Z Scale = Thickness
					FinalScaleVector = FVector(TargetWidth * 0.01f, TargetHeight * 0.01f,  ThicknessVal);
					
					break;
				}

				// ----------------------------------------------------------------
				// [Step 4] 행렬 -> 회전 변환 및 적용
				// ----------------------------------------------------------------
				// 구한 타겟 축들로 회전 행렬을 만듭니다.
				// FRotationMatrix::MakeFromXY 등은 축을 강제로 직교화(Orthogonalize)해주므로 안전합니다.
				
				// X, Y, Z 축이 각각 어디를 향해야 하는지 지정하여 Rotator를 생성합니다.
				FMatrix TargetMatrix = FMatrix::Identity;
				TargetMatrix.SetAxes(&TargetAxisX, &TargetAxisY, &TargetAxisZ, &CamLoc);

				bgPlaneComp->SetWorldScale3D(FinalScaleVector);

				bgPlaneComp->SetWorldRotation(TargetMatrix.Rotator());
				bgPlaneComp->AddLocalRotation(FRotator(0,90,0));
				
				bgPlaneComp->SetWorldLocation(CamLoc + (CamFwd * fixedDist * 2));
				
				// 위치 적용
				// 회전 적용 (Matrix to Rotator)
				// 스케일 적용
			}
		}
	}
	else
	{
		UE_LOG(LogBackgroundActor,Warning,TEXT("SizeModify_Implementation: backgroundActor is invalid."));	
	}
}

void ATVdjmVcardBackgroundActor::OnVcardDeSelect_Implementation()
{
	VisibleBackground(false);
}

void ATVdjmVcardBackgroundActor::OnVcardSelect_Implementation(UObject* target)
{
	bool bValidTarget = false;
	if (UTVdjmVcardTileViewBgItemData* bgItem = Cast<UTVdjmVcardTileViewBgItemData>(target))
	{
		if (IsValid( BackgroundMaterialInstance))
		{
			CurrentSelectedBgItemData = bgItem;
			BackgroundMaterialInstance->SetTextureParameterValue(FName("BackgroundTexture"), bgItem->BackgroundImage);
			bValidTarget = true;
		}
	}
	VisibleBackground(bValidTarget);
}

void ATVdjmVcardBackgroundActor::VisibleBackground(bool bVisible)
{
	if (bVisible)
	{
		BackgroundComp->SetVisibility(true);
		BackgroundComp->SetMaterial(0, BackgroundMaterialInstance);
		//	onvisible callback
	}
	else
	{
		BackgroundComp->SetVisibility(false);
		//	onvisible callback
	}
	OnBackgroundSelected.Broadcast(this,bVisible);
}

void FTVdjmVcardMotionInfo::RestorePreviousState()
{
	if (TargetSkeletalMeshComp.IsValid())
	{
		TargetSkeletalMeshComp->SetPosition(PrevPositionInSec);
		TargetSkeletalMeshComp->SetPlayRate(PrevPlayRate);
		if (PrevAnimMode == EAnimationMode::Type::AnimationSingleNode)
		{
			TargetSkeletalMeshComp->SetAnimationMode(EAnimationMode::Type::AnimationSingleNode);
			TargetSkeletalMeshComp->AnimationData = PrevAnimPlayData;
			TargetSkeletalMeshComp->Play(PrevLooping);
		}
		else if (PrevAnimMode == EAnimationMode::Type::AnimationBlueprint)
		{
			TargetSkeletalMeshComp->SetAnimationMode(EAnimationMode::Type::AnimationBlueprint);
			if (PrevAnimInstanceClass)
			{
				TargetSkeletalMeshComp->SetAnimInstanceClass(PrevAnimInstanceClass);
			}
		}
	}
}

void FTVdjmVcardMotionInfo::CaptureCurrentState(USkeletalMeshComponent* skelComp)
{
	if (skelComp)
	{
		Clear();
		UE_LOG(LogMotionActor,Log,TEXT("Capturing current state of SkeletalMeshComponent %s"),*skelComp->GetName());
		TargetSkeletalMeshComp = skelComp;
		PrevAnimMode = skelComp->GetAnimationMode();
		if (PrevAnimMode == EAnimationMode::Type::AnimationSingleNode)
		{
			PrevAnimPlayData = skelComp->AnimationData;
			PrevLooping = PrevAnimPlayData.bSavedLooping;
			bHasPrevAnimPlayData = true;
			//PrevPlayRate = PrevAnimPlayData.SavedPlayRate;
		}
		else if (PrevAnimMode == EAnimationMode::Type::AnimationBlueprint)
		{
			if (skelComp->GetAnimInstance())
			{
				PrevAnimInstanceClass = skelComp->GetAnimInstance()->GetClass();
				bHasPrevAnimInstance = true;
			}
			else
			{
				bHasPrevAnimInstance = false;
			}
		}
		PrevPositionInSec = skelComp->GetPosition();
		PrevPlayRate = skelComp->GetPlayRate();
	}
	else
	{
		UE_LOG(LogMotionActor,Warning,TEXT("Failed to capture current state. SkeletalMeshComponent is null."));
	}
}

void FTVdjmVcardMotionInfo::PlayMotionAnimation(UAnimSequence* motionAnim, float playRate, float startPositionInSec)
{
	if (motionAnim && TargetSkeletalMeshComp.IsValid())
	{
		if (bPaused)
		{
			if (CurrentMotionAnimation.Get() != motionAnim)
			{
				//	다른 애니메이션이면 일단 멈추고 새로 재생
				TargetSkeletalMeshComp->SetPosition(0.f);
				TargetSkeletalMeshComp->SetPlayRate(0.f);
				PauseOff();
				bPaused = false;
			}
			else
			{
				playRate = PausePlayRate;
				startPositionInSec = PausedPositionInSec;
				PauseOff();
			}
		}
		else
		{
			TargetSkeletalMeshComp->SetAnimationMode(EAnimationMode::Type::AnimationSingleNode);
		}
		UE_LOG(LogMotionActor,Log,TEXT("Playing motion animation %s at position %f with playrate %f"),*motionAnim->GetName(), startPositionInSec, playRate);
		CurrentMotionAnimation = motionAnim;
		TargetSkeletalMeshComp->SetPosition(startPositionInSec);
		TargetSkeletalMeshComp->SetPlayRate(playRate);
		TargetSkeletalMeshComp->PlayAnimation(motionAnim, true);
	}
}

void FTVdjmVcardMotionInfo::StopMotionAnimation()
{
	if (TargetSkeletalMeshComp.IsValid())
	{
		CurrentMotionAnimation = nullptr;
		TargetSkeletalMeshComp->SetPosition(0.f);
		TargetSkeletalMeshComp->SetPlayRate(0.f);
		PauseOff();
		RestorePreviousState();
	}
}

void FTVdjmVcardMotionInfo::PauseMotionAnimation()
{
	if (TargetSkeletalMeshComp.IsValid())
	{
		bPaused = true;
		PausePlayRate = TargetSkeletalMeshComp->GetPlayRate();
		PausedPositionInSec = TargetSkeletalMeshComp->GetPosition();
		TargetSkeletalMeshComp->SetPlayRate(0.f);
	}
}

void FTVdjmVcardMotionSnapshot::CaptureSnapshot(const ATVdjmVcardMotionActor* motionActor, const AActor* targetActor, UTVdjmVcardTileViewMotionItemData* motionItemData)
{
	if (motionActor&& targetActor && motionItemData && motionItemData->IsPlayableMotionAnimation())
	{
		UAnimSequence* motionAnim = motionItemData->MotionAnimation;
		TriggeredMotionData = motionItemData;
		
		for (UActorComponent* comp :targetActor->GetComponents())
		{
			if (USkeletalMeshComponent* skelComp = Cast<USkeletalMeshComponent>(comp))
			{
				if (motionActor->IsAnimAndSkelCompatible(motionAnim,skelComp))
				{
					FTVdjmVcardMotionInfo newMotionInfo = {};
					newMotionInfo.CaptureCurrentState(skelComp);
					TargetSkeletalMeshComps.Add(newMotionInfo);
				}
				else
				{
					UE_LOG(LogMotionActor,Warning,TEXT("Motion animation %s is not compatible with SkeletalMeshComponent %s, skipping."),*motionAnim->GetName(),*skelComp->GetName());
				}
			}
			else
			{
				UE_LOG(LogMotionActor,Warning,TEXT("Component %s is not SkeletalMeshComponent, skipping."),*comp->GetName());
			}
		}
	}
	else
	{
		UE_LOG(LogMotionActor,Warning,TEXT("Failed to capture motion snapshot. Invalid parameters."));
	}
}

void FTVdjmVcardMotionSnapshot::RestoreSnapshot()
{
	for (FTVdjmVcardMotionInfo& motionInfo : TargetSkeletalMeshComps)
	{
		motionInfo.RestorePreviousState();
	}
	
}

void FTVdjmVcardMotionSnapshot::PlayMotionAnimation(float playRate, float startPositionInSec)
{
	if (TriggeredMotionData.IsValid())
	{
		for (FTVdjmVcardMotionInfo& motionInfo : TargetSkeletalMeshComps)
		{
			motionInfo.PlayMotionAnimation(TriggeredMotionData->MotionAnimation, playRate, startPositionInSec);
		}
	}
}

void FTVdjmVcardMotionSnapshot::PauseMotionAnimation()
{
	if (not TargetSkeletalMeshComps.IsEmpty())
	{
		for (FTVdjmVcardMotionInfo& motionInfo : TargetSkeletalMeshComps)
		{
			motionInfo.PauseMotionAnimation();
		}
	}
}

void FTVdjmVcardMotionSnapshot::StopMotionAnimation()
{
	for (FTVdjmVcardMotionInfo& motionInfo : TargetSkeletalMeshComps)
	{
		motionInfo.StopMotionAnimation();
	}
}

ATVdjmVcardMotionActor* ATVdjmVcardMotionActor::TryGetMotionActor(UWorld* world)
{
	ATVdjmVcardMotionActor* result = nullptr;
	if (world)
	{
		if (UTVdjmVcardConfigDataAsset* dataAsset =  UTVdjmVcardMobileMainWidget::TryGetLoadedConfigDataAsset())
		{
			bool spawnNew = true;
			for (TActorIterator<AActor> actorItr(world); actorItr; ++actorItr)
			{
				if (dataAsset->MotionActorClass && actorItr->IsA(ATVdjmVcardMotionActor::StaticClass()))
				{
					result = Cast<ATVdjmVcardMotionActor>( *actorItr );
					spawnNew = false;
					break;
				}
			}
			
			if (spawnNew)
			{
				if (IsValid(dataAsset->BackgroundActorClass))
				{
					FActorSpawnParameters spawnParams;
					spawnParams.Name = FName("VcardMotionActor");
					spawnParams.NameMode = FActorSpawnParameters::ESpawnActorNameMode::Requested;
					spawnParams.ObjectFlags |= RF_Transient;
					spawnParams.Owner = ATVdjmVcardMainUiBridge::TryGetBridge(world);
					result = Cast<ATVdjmVcardMotionActor>(world->SpawnActor(dataAsset->MotionActorClass, &dataAsset->MotionActorSpawnTransform, spawnParams));
				}
				else
				{
					UE_LOG(LogMotionActor,Warning,TEXT("MotionActorClass is invalid in VcardConfigDataAsset."));
				}
			}
		}
	}
	return result;
}

ATVdjmVcardMotionActor::ATVdjmVcardMotionActor()
{
	PrimaryActorTick.bCanEverTick = false;
	RootComp = CreateDefaultSubobject<USceneComponent>(TEXT("RootComp"));
	SetRootComponent(RootComp);
	
}

AActor* ATVdjmVcardMotionActor::TryFindMotionTargetActor() const
{
	AActor* result = nullptr;
	if (TargetActorForMotion.IsValid())
	{
		result = TargetActorForMotion.Get();
	}
	else
	{
		if (UTVdjmVcardConfigDataAsset* dataAsset =  UTVdjmVcardMobileMainWidget::TryGetLoadedConfigDataAsset())
		{
			UClass* charactorCls = dataAsset->CharacterTargetActorClass;
			if (charactorCls)
			{
				TArray<AActor*> foundActors;
				UGameplayStatics::GetAllActorsOfClass(GetWorld(), charactorCls, foundActors);
				for (AActor* findActor : foundActors)
				{
					if (findActor != nullptr)
					{
						result = findActor;
						break;
					}
				}
			}
			else
			{
				UE_LOG(LogMotionActor,Warning,TEXT("TryFindMotionTargetActor: CharacterTargetActorClass is invalid in VcardConfigDataAsset."));
			}
		}
		else
		{
			UE_LOG(LogMotionActor,Warning,TEXT("TryFindMotionTargetActor: Failed to get VcardConfigDataAsset."));
		}
	}
	if (result == nullptr)
	{
		UE_LOG(LogMotionActor,Warning,TEXT("TryFindMotionTargetActor: Could not find valid target actor for motion."));
	}
	return result;
}

void ATVdjmVcardMotionActor::SetMotionPlayInfo(float playRate, float startPositionInSec)
{
	CurrentPlayRate = FMath::Max(playRate, 0.f);
	CurrentStartPositionInSec = FMath::Max(startPositionInSec, 0.f);
}

void ATVdjmVcardMotionActor::PlayMotionAnimation()
{
	if (CurrentMotionSnapshotPtr && CurrentMotionSnapshotPtr->DbcValidSnapshot())
	{
		UE_LOG(LogMotionActor,Log,TEXT("Playing motion animation for item %s"),*CurrentSelectedMotionItemData->MotionName);
		CurrentMotionSnapshotPtr->PlayMotionAnimation(CurrentPlayRate, CurrentStartPositionInSec);
	}
	else
	{
		UE_LOG(LogMotionActor,Warning,TEXT("No valid motion snapshot to play animation."));
		if (CurrentMotionSnapshotPtr == nullptr)
		{
			UE_LOG(LogMotionActor,Warning,TEXT("CurrentMotionSnapshotPtr is null."));
		}
		else if (not CurrentMotionSnapshotPtr->DbcValidSnapshot())
		{
			UE_LOG(LogMotionActor,Warning,TEXT("CurrentMotionSnapshotPtr does not have a valid snapshot."));
			
			
		}
	}
}

void ATVdjmVcardMotionActor::PauseMotionAnimation()
{
	if (not MotionSnapshotStack.IsEmpty())
	{
		FTVdjmVcardMotionSnapshot& motionSnap = MotionSnapshotStack.Last();
		motionSnap.PauseMotionAnimation();
	}
}

void ATVdjmVcardMotionActor::StopMotionAnimation()
{
	if (not MotionSnapshotStack.IsEmpty())
	{
		FTVdjmVcardMotionSnapshot& motionSnap = MotionSnapshotStack.Last();
		motionSnap.StopMotionAnimation();
	}
}

void ATVdjmVcardMotionActor::OnVcardDeSelect_Implementation()
{
	
}

void ATVdjmVcardMotionActor::CaptureVcardMotionActorSnapshot(UTVdjmVcardTileViewMotionItemData* bgItem, AActor* targetActor)
{
	if (bgItem && targetActor && bgItem->IsPlayableMotionAnimation())
	{
		CurrentSelectedMotionItemData = bgItem;
		TargetActorForMotion = targetActor;
		FTVdjmVcardMotionSnapshot newSnapshot = {};
		newSnapshot.CaptureSnapshot(this, targetActor, bgItem);
		
		if (newSnapshot.DbcValidSnapshot())
		{
			UE_LOG(LogMotionActor,Log,TEXT("Captured motion snapshot for item %s"),*bgItem->MotionName);
			MotionSnapshotStack.Add(newSnapshot);
		}
		else
		{
			UE_LOG(LogMotionActor,Warning,TEXT("Failed to capture motion snapshot for item %s"),*bgItem->MotionName);
		}
	}
}

void ATVdjmVcardMotionActor::OnVcardSelect_Implementation(UObject* target)
{
	if (CurrentMotionSnapshotPtr && CurrentMotionSnapshotPtr->DbcValidSnapshot())
	{
		CurrentMotionSnapshotPtr->StopMotionAnimation();
	}
	
	CurrentMotionSnapshotPtr = nullptr;
	
	if (UTVdjmVcardTileViewMotionItemData* bgItem = Cast<UTVdjmVcardTileViewMotionItemData>(target))
	{
		if (TargetActorForMotion == nullptr)
		{
			TargetActorForMotion = TryFindMotionTargetActor();
		}
		
		if (TargetActorForMotion.IsValid())
		{
			TargetActorForMotion->SetActorTransform(GetActorTransform());
			CaptureVcardMotionActorSnapshot(bgItem, TargetActorForMotion.Get());
			CurrentMotionSnapshotPtr = &MotionSnapshotStack.Last();
			UE_LOG(LogMotionActor,Log,TEXT("Motion Actor captured motion snapshot for item %s"),*bgItem->MotionName);
		}
		else
		{
			CurrentMotionSnapshotPtr = &IdleMotionCapture;
			UE_LOG(LogMotionActor,Warning,TEXT("Motion Actor could not find valid target actor for motion."));
		}
		
		UE_LOG(LogMotionActor,Log,TEXT("Motion Actor selected motion item %s"),*bgItem->MotionName);
		PlayMotionAnimation();
	}
}

bool ATVdjmVcardMotionActor::IsAnimAndSkelCompatible(UAnimSequence* animSeq, USkeletalMeshComponent* skelComp) const
{
	if (!animSeq || !skelComp)
	{
		if (animSeq == nullptr)
		{
			UE_LOG(LogMotionActor,Warning,TEXT("IsAnimAndSkelCompatible: animSeq is null."));
		}
		if (skelComp == nullptr)
		{
			UE_LOG(LogMotionActor,Warning,TEXT("IsAnimAndSkelCompatible: skelComp is null."));
		}
		return false;
	}

	if (USkeletalMesh* meshAsset = skelComp->GetSkeletalMeshAsset())
	{
		USkeleton* compSkeleton = meshAsset->GetSkeleton();
		USkeleton* animSkeleton = animSeq->GetSkeleton();
		if (not compSkeleton || not animSkeleton)
		{
			if (compSkeleton == nullptr)
			{
				UE_LOG(LogMotionActor,Warning,TEXT("IsAnimAndSkelCompatible: compSkeleton is null."));
			}
			if (animSkeleton == nullptr)
			{
				UE_LOG(LogMotionActor,Warning,TEXT("IsAnimAndSkelCompatible: animSkeleton is null."));
			}
			return false;
		}

		//	캐시 확인
		uintptr_t compSkelAddr = reinterpret_cast<uintptr_t>(compSkeleton);
		uintptr_t animSkelAddr = reinterpret_cast<uintptr_t>(animSkeleton);
		FTVdjmVcardSkeletonCachedKey compKey(compSkelAddr,animSkelAddr);
		
		if (bool* compatible = mSkeletonAnimCompatibilityCache.Find(compKey))
		{
			return *compatible;
		}
		
		//	Ptr 비교
		if (compSkelAddr == animSkelAddr)
		{
			mSkeletonAnimCompatibilityCache.Add(compKey,true);
			return true;
		}

		//	path 비교
		const FString& compSkelPath = compSkeleton->GetPathName();
		const FString& animSkelPath = animSkeleton->GetPathName();
		if (compSkelPath.Equals(animSkelPath, ESearchCase::IgnoreCase))
		{
			mSkeletonAnimCompatibilityCache.Add(compKey,true);
			return true;
		}

		UE_LOG(LogMotionActor,Warning,TEXT("IsAnimAndSkelCompatible: Skeletons do not match. compSkeleton: %s , animSkeleton: %s"),*compSkelPath,*animSkelPath);
		mSkeletonAnimCompatibilityCache.Add(compKey,false);
		return false;
	}
	else
	{
		UE_LOG(LogMotionActor,Warning,TEXT("IsAnimAndSkelCompatible: skelComp's SkeletalMeshAsset is null."));
		return false;
	}
}

ETVdjmVcardCommandManagedTypes ATVdjmVcardMotionActor::GetCommandManagedType_Implementation() const
{
	return ETVdjmVcardCommandManagedTypes::EManagedSingleton;
}

FName ATVdjmVcardMotionActor::GetCommandName_Implementation() const
{
	return TEXT("motion-actor");
}

TArray<FString> ATVdjmVcardMotionActor::GetCommandParamKeys_Implementation() const
{
	return ITVdjmVcardCommandAction::GetCommandParamKeys_Implementation();
}
int32 ATVdjmVcardMotionActor::ExecuteCommand_Implementation()
{
	return ITVdjmVcardCommandAction::ExecuteCommand_Implementation();
}

int32 ATVdjmVcardMotionActor::UndoCommand_Implementation()
{
	return ITVdjmVcardCommandAction::UndoCommand_Implementation();
}

void ATVdjmVcardMotionActor::BeginPlay()
{
	Super::BeginPlay();

	if (RootComp)
	{
		if (auto configDataAsset = UTVdjmVcardMobileMainWidget::TryGetLoadedConfigDataAsset())
		{
			SetActorTransform(configDataAsset->MotionActorSpawnTransform);
		}
	}
	if (not UiBridgeActor.IsValid())
	{
		UiBridgeActor = ATVdjmVcardMainUiBridge::TryGetBridge(GetWorld());
	}
	
}




























