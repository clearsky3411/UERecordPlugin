// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "NaniteDefinitions.h"
#include "Blueprint/IUserObjectListEntry.h"
#include "Blueprint/UserWidget.h"
#include "Components/PanelWidget.h"
#include "Components/StackBox.h"
#include "Components/TileView.h"
#include "Components/WidgetSwitcher.h"
#include "Containers/Deque.h"
#include "DSP/AudioDebuggingUtilities.h"
#include "Extensions/UserWidgetExtension.h"
#include "UObject/Object.h"
#include "VdjmMobileUiCore.generated.h"

/*
 *	Config DataAsset
 *	- Class: UTVdjmVcardConfigDataAsset
 *	- Path: /Script/VdjmMobileUi.TVdjmVcardConfigDataAsset'/VdjmMobileUi/Bp_VdjmVcardConfigDataAsset.Bp_VdjmVcardConfigDataAsset'
 *	- Called at: UTVdjmVcardMobileMainWidget::DefaultSetting()
 *	- Owner Object: UTVdjmVcardMobileMainWidget
 * 
 */


class UTextBlock;
DECLARE_LOG_CATEGORY_EXTERN(LogMobileMain, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogOptContentMain, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogMainUiBridge, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogModeContainer, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogSwitcherMgr, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogTileViewMain, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogTileViewItemWidget, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogTileViewItemData, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogTileViewItemLoader, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogCmdSelector, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogCmdChangeSwitcher, Log, All);

DECLARE_LOG_CATEGORY_EXTERN(LogBackgroundActor, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogMotionActor, Log, All);


class UTVdjmVcardTileViewMotionItemData;
class UTVdjmVcardTileViewItemWidget;
class ATVdjmVcardMotionActor;
class URichTextBlock;
class ATVdjmVcardBackgroundActor;
class UTVdjmVcardTileViewItemData;
class IUserObjectListEntry;
class UTVdjmVcardTileViewMainWidget;
class UTileView;
struct FTVdjmVcardCmdPacket;
class UImage;
class UButton;
class UBorder;
class UOverlay;
class UWidgetSwitcher;
class USizeBox;
class UTVdjmVcardContainerDescriptor;
/*
 *
 */
class ITVdjmVcardInteractable;
class ATVdjmVcardMainUiBridge;
class UTVdjmVcardMobileMainWidget;

class UTVdjmVcardCommand;
class UTVdjmVcardCommandItemSelector;
class UTVdjmVcardCommandChangeSwitcher;
class UTVdjmVcardCommandControl;

class UTVdjmVcardTileViewWidget;

UCLASS(Blueprintable)
class VDJMMOBILEUI_API UTVdjmVcardSetUpContext : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadWrite,EditAnywhere)
	TMap<FName, TScriptInterface< ITVdjmVcardInteractable>> UiFunctionInterfaces;
	UPROPERTY()
	TMap<FName, TWeakObjectPtr<UObject>> AdditionalData;
	UPROPERTY()
	TMap<FName,TWeakObjectPtr<UUserWidget>> UiWidgets;

	UPROPERTY(BlueprintReadWrite,EditAnywhere)
	TWeakObjectPtr<ATVdjmVcardMainUiBridge> SpawnHelperActor;
};


UENUM(BlueprintType)
enum class ETVdjmVcardInteractableType : uint8
{
	EUndefined UMETA(DisplayName="Undefined"),
	EDescriptor UMETA(DisplayName="Descriptor"),
	EWidget UMETA(DisplayName="Widget"),
};
USTRUCT(Blueprintable)
struct VDJMMOBILEUI_API FTVdjmVcardCmdPacket
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite,EditAnywhere)
	FName CommandName;
	UPROPERTY(BlueprintReadWrite,EditAnywhere)
	TMap<FString,FString> CommandParams;
	UPROPERTY(BlueprintReadWrite,EditAnywhere)
	TWeakObjectPtr<UObject> FromObject;
	UPROPERTY(EditAnywhere)
	TMap<FString,TWeakObjectPtr<UObject>> PayloadObjects;

	FString ToString() const
	{
		FString result = FString::Printf(TEXT("CommandName: %s; Params: "), *CommandName.ToString());
		for (const TPair<FString,FString>& paramPair : CommandParams)
		{
			result += FString::Printf(TEXT("[%s : %s] "), *paramPair.Key, *paramPair.Value);
		}
		return result;
	}
	FString GetParamString(const FString& key) const
	{
		if (const FString* foundParam = CommandParams.Find(key))
		{
			return *foundParam;
		}
		return FString(TEXT(""));
	}
	UObject* GetPayloadObject(const FString& key) const
	{
		if (const TWeakObjectPtr<UObject>* foundObj = PayloadObjects.Find(key))
		{
			return foundObj->Get();
		}
		return nullptr;
	}
};

UENUM(BlueprintType)
enum class ETVdjmVcardCommandManagedTypes : uint8
{
	ENewInstance,
	EManagedSingleton,
	ENoUse
};
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FTVdjmVcardTileViewChangeItemWidgetClsEvent, UTVdjmVcardTileViewWidget*, calldTileViewWidget, TSubclassOf<UUserWidget>, prevItemWidgetCls,TSubclassOf<UUserWidget>, newItemWidgetCls);

UCLASS(Blueprintable)
class VDJMMOBILEUI_API UTVdjmVcardTileViewWidget : public UTileView
{
	GENERATED_BODY()
public:
	UFUNCTION(Category="Vcard", BlueprintCallable)
	void ChangeTileViewItemWidgetClass(TSubclassOf<UUserWidget> newItemWidgetClass);

	UPROPERTY(Category="Vcard",BlueprintReadWrite,BlueprintAssignable)
	FTVdjmVcardTileViewChangeItemWidgetClsEvent OnTileViewItemWidgetClassChanged;
};

UINTERFACE(Blueprintable)
class VDJMMOBILEUI_API UTVdjmVcardCommandAction : public UInterface
{
	GENERATED_BODY()
};

class VDJMMOBILEUI_API ITVdjmVcardCommandAction
{
	GENERATED_BODY()

public:
	UFUNCTION(
		Category = "Vcard|Command",
		BlueprintNativeEvent, BlueprintCallable)
	void ActivateCommand(ATVdjmVcardMainUiBridge* bridge,const FTVdjmVcardCmdPacket& cmdPacket);


	UFUNCTION(
		Category = "Vcard|Command",
		BlueprintNativeEvent, BlueprintCallable)
	int32 ExecuteCommand();
	UFUNCTION(
		Category = "Vcard|Command",
		BlueprintNativeEvent, BlueprintCallable)
	int32 UndoCommand();
	UFUNCTION(
			Category = "Vcard|Command",
			BlueprintNativeEvent, BlueprintCallable)
	FTVdjmVcardCmdPacket GetCommandSnapshot() const;

	UFUNCTION(
			Category = "Vcard|Command",
			BlueprintNativeEvent, BlueprintCallable)
	FName GetCommandName() const;

	UFUNCTION(
			Category = "Vcard|Command",
			BlueprintNativeEvent, BlueprintCallable)
	TArray<FString> GetCommandParamKeys() const;

	UFUNCTION(
			Category = "Vcard|Command",
			BlueprintNativeEvent, BlueprintCallable)
	ETVdjmVcardCommandManagedTypes GetCommandManagedType() const;

	UFUNCTION(
			Category = "Vcard|Command",
			BlueprintNativeEvent, BlueprintCallable)
	FTVdjmVcardCmdPacket GetCommandPacketSnapShot() const;
};


UINTERFACE(Blueprintable)
class VDJMMOBILEUI_API UTVdjmVcardInteractable : public UInterface
{
	GENERATED_BODY()
};

class VDJMMOBILEUI_API ITVdjmVcardInteractable
{
	GENERATED_BODY()

public:
	virtual ETVdjmVcardInteractableType GetInteractableType() const PURE_VIRTUAL(ITVdjmInteractable::GetInteractableType, return ETVdjmVcardInteractableType::EUndefined; );

	UFUNCTION(
	Category="Vcard|Interactable",
	Blueprintable,BlueprintNativeEvent )
	void OnVcardSelect(UObject* target);
	UFUNCTION(
	Category="Vcard|Interactable",Blueprintable,BlueprintNativeEvent)
	void OnVcardDeSelect();
	UFUNCTION(
	Category="Vcard|Interactable",Blueprintable,BlueprintNativeEvent)
	bool IsVcardSelected(UObject* target);
	UFUNCTION(
	Category="Vcard|Interactable",Blueprintable,BlueprintNativeEvent)
	void OnVcardHover(UObject* target);
	UFUNCTION(
	Category="Vcard|Interactable",Blueprintable,BlueprintNativeEvent)
	void OnVcardUnHover(UObject* target);
	UFUNCTION(
	Category="Vcard|Interactable",Blueprintable,BlueprintNativeEvent)
	bool IsVcardHovered(UObject* target);

	UFUNCTION(
	Category="Vcard|Interactable",Blueprintable,BlueprintNativeEvent)
	void OnVcardSetUpContext(UObject* setUpContext);
	UFUNCTION(
	Category="Vcard|Interactable",Blueprintable,BlueprintNativeEvent)
	void OnVcardActivate();
	UFUNCTION(
	Category="Vcard|Interactable",Blueprintable,BlueprintNativeEvent)
	void OnVcardDeActivate();
	UFUNCTION(
	Category="Vcard|Interactable",Blueprintable,BlueprintNativeEvent)
	bool IsVcardActivate() const;

	UFUNCTION(
	Category="Vcard|Interactable",Blueprintable,BlueprintNativeEvent)
	void OnVcardValueChangedFloat(float NewValue);

	UFUNCTION(
	Category="Vcard|Interactable",Blueprintable,BlueprintNativeEvent)
	ATVdjmVcardMainUiBridge* GetVcardUiBridge() const;
};

UCLASS()
class VDJMMOBILEUI_API UTVdjmVcardAllConfigure : public UObject
{
	GENERATED_BODY()

public:
	
};



UCLASS(Blueprintable)
class VDJMMOBILEUI_API UTVdjmVcardCommand : public UObject, public ITVdjmVcardCommandAction
{
	GENERATED_BODY()

public:

	int32 Result_Failed_PayloadValueMissing() {		return ErrorCode = -6;	};
	int32 Result_Failed_PayloadMissing() {		return ErrorCode = -5;	};
	int32 Result_Failed_CastedContainerInvalid()	{		return ErrorCode = -4;	};
	int32 Result_Failed_ContainerNotFound()	{		return ErrorCode = -3;	};
	int32 Result_Failed_InvalidParam ()	{		return ErrorCode = -2;	};
	int32 Result_Failed_Error()	{		return ErrorCode = -1;	};
	int32 Result_Succeeded() 	{		return ErrorCode = 1;	};

	const FTVdjmVcardCmdPacket & GetSnapShot() const
	{
		return CmdPacketSnapShot;
	}
	ATVdjmVcardMainUiBridge* GetUiBridge() const
	{
		return UiBridgeActor.Get();
	}

	bool DbcCommandValid() const
	{
		return UiBridgeActor.IsValid();
	}
	virtual bool DbcCommandExecuteValid() const
	{
		return DbcCommandValid();
	}

	// virtual int32 Execute() PURE_VIRTUAL(UTVdjmVcardCommand::Execute, return -1; );
	// virtual int32 Undo() PURE_VIRTUAL(UTVdjmVcardCommand::Undo, return -1; );
	//
	// virtual FString ErrorString() const
	// {
	// 	switch (ErrorCode)
	// 	{
	// 	case -5:
	// 		return FString(TEXT("Payload object missing."));
	// 	case -4:
	// 		return FString(TEXT("Casted container is invalid."));
	// 	case -3:
	// 		return FString(TEXT("Container not found."));
	// 	case -2:
	// 		return FString(TEXT("Invalid parameter."));
	// 	case -1:
	// 		return FString(TEXT("General error."));
	// 	case 1:
	// 		return FString(TEXT("Succeeded."));
	// 	default: return FString(TEXT("Unknown error code."));
	// 	}
	// }
	// virtual FString GetCommandName() const
	// {
	// 	return FString(TEXT("undefined-command"));
	// }
	//
	// virtual TArray<FString> GetParamKeys() const
	// {
	// 	return TArray<FString>();
	// }

	FString GetCmdSnapShotParamValue(const FString& key) const
	{
		return CmdPacketSnapShot.GetParamString(key);
	}

	virtual int32 ExecuteCommand_Implementation() override
	{
		return -1;
	}
	virtual FName GetCommandName_Implementation() const override
	{
		return FName(TEXT("undefined-command"));
	}
	virtual TArray<FString> GetCommandParamKeys_Implementation() const override
	{
		return TArray<FString>();
	}
	virtual FTVdjmVcardCmdPacket GetCommandSnapshot_Implementation() const override
	{
		return CmdPacketSnapShot;
	}
	virtual void ActivateCommand_Implementation(ATVdjmVcardMainUiBridge* bridge,
		const FTVdjmVcardCmdPacket& cmdPacket) override
	{
		UiBridgeActor = bridge;
		CmdPacketSnapShot = cmdPacket;
	}
	virtual int32 UndoCommand_Implementation() override
	{
		return -1;
	}

	UPROPERTY(BlueprintReadWrite,EditAnywhere)
	TWeakObjectPtr<ATVdjmVcardMainUiBridge> UiBridgeActor;

	UPROPERTY(BlueprintReadWrite,EditAnywhere)
	FTVdjmVcardCmdPacket CmdPacketSnapShot;

	int32 ErrorCode = 1;
};

UENUM(BlueprintType)
enum class ETVdjmVcardChangeCommandTypes : uint8
{
	EAdd,
	EPop,
	EClear,
	ERefresh,
	EModify,
	ESelect,
	EUnSelect,
	EActivate,
	EDeActivate,
	EUndefined
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FTVdjmVcardBridgeChangeCommandEvent,ATVdjmVcardMainUiBridge*,bridge,TScriptInterface<ITVdjmVcardCommandAction>,command,ETVdjmVcardChangeCommandTypes, changeType);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FTVdjmVcardBridgeCommandEvent,ATVdjmVcardMainUiBridge*,bridge,TScriptInterface<ITVdjmVcardCommandAction>,command);

UCLASS(Blueprintable)
class VDJMMOBILEUI_API ATVdjmVcardMainUiBridge : public AActor
{
	GENERATED_BODY()
	friend UTVdjmVcardMobileMainWidget;
public:
	template<typename TIHTemplate>
	static void RegisterCommand(TWeakObjectPtr<ATVdjmVcardMainUiBridge> bridge )
	{
		if (bridge.IsValid())
		{
			UClass* templateCls = TIHTemplate::StaticClass();
			if (templateCls == nullptr)
			{
				UE_LOG(LogTemp,Warning,TEXT("Cannot register command to bridge. Template class is null."));
				return;
			}
			if (not templateCls->ImplementsInterface(UTVdjmVcardCommandAction::StaticClass()))
			{
				UE_LOG(LogTemp,Warning,TEXT("Cannot register command to bridge. Template class does not implement UTVdjmVcardCommandAction interface."));
				return;
			}

			auto& commandTable = bridge->CommandTable;

			//	TODO 여기에 추가
			TScriptInterface<ITVdjmVcardCommandAction> cmdActionInterface = CreateCommandInstance(
				bridge.Get(),
				templateCls,
				bridge.Get(),
				FTVdjmVcardCmdPacket()
			);

			if (cmdActionInterface.GetObject() == nullptr || cmdActionInterface.GetInterface() == nullptr)
			{
				UE_LOG(LogTemp,Warning,TEXT("Cannot register command to bridge. Failed to create command action interface instance."));
				return;
			}

			FName commandName = ITVdjmVcardCommandAction::Execute_GetCommandName(cmdActionInterface.GetObject());

			if (bridge->CommandValidInTable(commandName) == false)
			{
				commandTable.Add(commandName,templateCls);
				UE_LOG(LogTemp,Log,TEXT("Command %s registered to bridge."),*commandName.ToString());
			}
			cmdActionInterface = nullptr;
		}
		else
		{
			UE_LOG(LogTemp,Warning,TEXT("Cannot register command to invalid bridge."));
		}
	}
	static TScriptInterface<ITVdjmVcardCommandAction> CreateCommandInstance(UObject* outer,UClass* commandCls,ATVdjmVcardMainUiBridge* bridge, const FTVdjmVcardCmdPacket& cmdPacket)
	{
		TScriptInterface<ITVdjmVcardCommandAction> result = nullptr;
		if (ITVdjmVcardCommandAction* cmdAction = bridge->TryGetManagedCommandInstance(commandCls))
		{
			UObject* objType = bridge->TryGetManagedCommandUObjectInstance(commandCls);
			result.SetInterface( cmdAction );
			result.SetObject( objType );
		}
		else
		{
			UObject* newCmdObj = NewObject<UObject>(outer, commandCls);
			if (newCmdObj && newCmdObj->GetClass()->ImplementsInterface(UTVdjmVcardCommandAction::StaticClass()))
            {
                result.SetObject(newCmdObj);
                result.SetInterface(Cast<ITVdjmVcardCommandAction>(newCmdObj));
                ETVdjmVcardCommandManagedTypes managedType = ITVdjmVcardCommandAction::Execute_GetCommandManagedType(
	                newCmdObj);

				if (ETVdjmVcardCommandManagedTypes::EManagedSingleton == managedType)
				{
					bridge->RegisterManagedCommandInstance(commandCls,newCmdObj);
				}
				//else if (ETVdjmVcardCommandManagedTypes::ENewInstance == managedType)
            }
		}
		if (UObject* curObj = result.GetObject())
		{
			ITVdjmVcardCommandAction::Execute_ActivateCommand(curObj, bridge, cmdPacket);
		}
		return result;
	}

	UFUNCTION(BlueprintCallable)
	static UWidget* TravelParentClass(UWidget* start,UClass* targetCls);

	UFUNCTION(BlueprintCallable)
	static UWorld* TryGetStaticWorld()
	{
		if (GEngine)
		{
			for (const FWorldContext& context : GEngine->GetWorldContexts())
			{
				if (context.WorldType == EWorldType::Game || context.WorldType == EWorldType::PIE)
				{
					return context.World();
				}
			}
		}
		return nullptr;
	}
	
	//	start 에서 하위로 가면서 만나는 모든 애들을 기록함.
	UFUNCTION(BlueprintCallable)
	static TArray<UWidget*> TravelChildrenClass(UWidget* start,UClass* targetCls);

	UFUNCTION(BlueprintCallable)
	static ATVdjmVcardMainUiBridge* TryGetBridge(UWorld* world);

	UFUNCTION(BlueprintCallable)
	static ATVdjmVcardMainUiBridge* TryGetBridgetFromStaticWorld()
	{
		return TryGetBridge(TryGetStaticWorld());
	}

	static ATVdjmVcardBackgroundActor* TryGetBackgroundActorStaticWorld();
	static ATVdjmVcardMotionActor* TryGetMotionActorStaticWorld();

	ATVdjmVcardBackgroundActor* GetBackgroundActor() const;

	bool IsManagedCommand(UClass* commandCls) const
    {
        return commandCls != nullptr && ManagedCommandInstancePoolTable.Contains(commandCls);
    }

	ITVdjmVcardCommandAction* TryGetManagedCommandInstance(UClass* commandCls)
    {
        ITVdjmVcardCommandAction* result = nullptr;
        if (IsManagedCommand(commandCls))
        {
            UObject* foundInstance = ManagedCommandInstancePoolTable[commandCls];
            if (foundInstance && foundInstance->GetClass()->ImplementsInterface(UTVdjmVcardCommandAction::StaticClass()))
            {
                result = Cast<ITVdjmVcardCommandAction>(foundInstance);
            }
        	else
        	{
        		ManagedCommandInstancePoolTable.Remove(commandCls);
        	}
        }
        return result;
    }
	UObject* TryGetManagedCommandUObjectInstance(UClass* commandCls)
    {
        UObject* result = nullptr;
        if (IsManagedCommand(commandCls))
        {
            if (UObject* inst = ManagedCommandInstancePoolTable[commandCls])
            {
            	result = inst;
            }
        	else
        	{
        		ManagedCommandInstancePoolTable.Remove(commandCls);
        	}
        }
        return result;
    }
	UFUNCTION(BlueprintCallable)
	void RegisterManagedCommandInstance(UClass* commandCls,UObject* instance)
	{
		if (commandCls && instance && instance->GetClass()->ImplementsInterface(UTVdjmVcardCommandAction::StaticClass()))
		{
			ManagedCommandInstancePoolTable.Add(commandCls,instance);
		}
	};
	
	/*
	 * >> RegisterWidgetCache variations
	 */
	UFUNCTION(BlueprintCallable)
	static void RegisterWidget_Container(const FString& containerName,UWidget* containerWidget)
	{
		if (ATVdjmVcardMainUiBridge* bridge = TryGetBridgetFromStaticWorld())
		{
			UE_LOG(LogTemp,Log,TEXT("{{ RegisterWidget_Container called for %s"),*containerName);
			bridge->RegisterWidgetCache(containerName,containerWidget);
		}
	}
	UFUNCTION(BlueprintCallable)
	static void RegisterWidget_Switcher(const FString& switcherName,UWidget* switcherWidget,const FString& pageName=FString(TEXT("")),int32 pageIndex = 0);
	
	UFUNCTION(BlueprintCallable)
	static void RegisterWidget_TileView(const FString& tileViewName,UWidget* tileViewWidget)
	{
		if (ATVdjmVcardMainUiBridge* bridge = TryGetBridgetFromStaticWorld())
		{
			UE_LOG(LogTemp,Log,TEXT("{{ RegisterWidget_TileView called for %s"),*tileViewName);
			bridge->RegisterWidgetCache(tileViewName,tileViewWidget);
		}
	}

	UFUNCTION(BlueprintCallable)
	void RegisterWidgetCache(const FString& widgetName, UWidget* widgetPtr);
	
	UFUNCTION(
		Category= "Vcard|Bridge|Command",
		BlueprintCallable)
	int32 CommandExecute_before(const FTVdjmVcardCmdPacket& cmdPacket);
	UFUNCTION(
		Category= "Vcard|Bridge|Command",
		BlueprintCallable)
	int32 CommandExecute(const FTVdjmVcardCmdPacket& cmdPacket);
	UFUNCTION(
		Category= "Vcard|Bridge|Command",
		BlueprintCallable)
	int32 CommandExecuteInBlueprint(const FName& commandName, TMap<FString,FString>& commandParams,TMap<FString,UObject*> payloadObjects ,UObject* fromObj = nullptr);
	UFUNCTION(
		Category= "Vcard|Bridge|Command",
		BlueprintCallable)
	int32 CommandExecuteEmplace(const FName& commandName, TMap<FString,FString> commandParams,UObject* fromObj = nullptr);
	UFUNCTION(
		Category= "Vcard|Bridge|Command",
		BlueprintCallable)
	int32 CommandExecuteEmplaceDebugString(const FName& commandName, TMap<FString,FString> commandParams,UObject* fromObj ,const FString& debugString);

	int32 CommandExecutePayload(const FName& commandName,const TMap<FString,FString>& commandParams,
		TMap<FString,TWeakObjectPtr<UObject>> payloadObjects,TWeakObjectPtr<UObject> fromObj = nullptr);

	bool CommandValidInTable(const FName& commandName) const
	{
		return CommandTable.Contains(commandName) && (CommandTable[commandName] != nullptr);
	}
	UWidget* GetWidgetFromCache(const FString& widgetName) const
	{
		if (const TWeakObjectPtr<UWidget>* foundWidget = UiWidgetCache.Find(widgetName))
		{
			return foundWidget->Get();
		}
		return nullptr;
	}
	UUserWidget* GetUserWidgetFromCache(const FString& widgetName) const
	{
		return Cast<UUserWidget>(GetWidgetFromCache(widgetName));
	}
	void RemoveWidgetFromCache(const FString& widgetName)
	{
		UiWidgetCache.Remove(widgetName);
	}
	
	/*
	 *	command success -> push to undo stack, clear redo stack
	 *	execute undo -> pop from undo stack, push to redo stack
	 *	execute redo -> pop from redo stack, push to undo stack
	 */
	UFUNCTION(
		Category= "Vcard|Bridge|Command",
		BlueprintCallable)
	void UndoCommand();
	UFUNCTION(
		Category= "Vcard|Bridge|Command",
		BlueprintCallable)
	int32 PushUndoCommand(TScriptInterface<ITVdjmVcardCommandAction> command)
	{
		int32 result = -1;
		if (command)
		{
			result = UndoStack.Add(command);
			OnUndoStackChanged.Broadcast(this,command,ETVdjmVcardChangeCommandTypes::EAdd);
		}
		return result;
	}

	UFUNCTION(
		Category= "Vcard|Bridge|Command",
		BlueprintCallable)
	TScriptInterface<ITVdjmVcardCommandAction> PopUndoCommand()
	{
		TScriptInterface<ITVdjmVcardCommandAction> result = nullptr;
		if (IsUndoPossible())
		{
			result = UndoStack.Pop();
			if (IsValid(result.GetObject() ))
			{
				OnUndoStackChanged.Broadcast(this,result,ETVdjmVcardChangeCommandTypes::EPop);
			}
			else
			{
				result.SetObject(nullptr);
				result.SetInterface(nullptr);
			}
		}
		return result;
	}

	UFUNCTION(
		Category= "Vcard|Bridge|Command",
		BlueprintCallable)
	void ClearUndoStack()
	{
		if (UndoStack.Num() > 0)
		{
			UndoStack.Empty();
			//	Undo stack 변화
			OnUndoStackChanged.Broadcast(this,nullptr,ETVdjmVcardChangeCommandTypes::EClear);
		}
	}
	UFUNCTION(
		Category= "Vcard|Bridge|Command",
		BlueprintCallable)
	bool IsUndoPossible() const
	{
		return UndoStack.Num() > 0;
	}
	UFUNCTION(
		Category= "Vcard|Bridge|Command",
		BlueprintCallable)
	int32 GetUndoStackCount() const
	{
		return UndoStack.Num();
	}

	UFUNCTION(
		Category= "Vcard|Bridge|Command",
		BlueprintCallable)
	void RedoCommand();
	UFUNCTION(
		Category= "Vcard|Bridge|Command",
		BlueprintCallable)
	int32 PushRedoCommand(TScriptInterface<ITVdjmVcardCommandAction> command)
	{
		int32 result = -1;
		if (command)
		{
			result = RedoStack.Add(command);
			OnRedoStackChanged.Broadcast(this,command,ETVdjmVcardChangeCommandTypes::EAdd);
		}
		return result;
	}
	UFUNCTION(
		Category= "Vcard|Bridge|Command",
		BlueprintCallable)
	TScriptInterface<ITVdjmVcardCommandAction> PopRedoCommand()
	{
		TScriptInterface<ITVdjmVcardCommandAction> result = nullptr;
		if (IsRedoPossible())
		{
			result = RedoStack.Pop();
			if (IsValid(result.GetObject()))
			{
				OnRedoStackChanged.Broadcast(this,result,ETVdjmVcardChangeCommandTypes::EPop);
			}
			else
			{
				result.SetObject(nullptr);
				result.SetInterface(nullptr);
			}
		}
		return result;
	}
	UFUNCTION(
		Category= "Vcard|Bridge|Command",
		BlueprintCallable)
	void ClearRedoStack()
	{
		if (IsRedoPossible())
		{
			OnRedoStackChanged.Broadcast(this ,nullptr,ETVdjmVcardChangeCommandTypes::EClear);
			RedoStack.Empty();
			//	Redo stack 변화
		}
	}

	UFUNCTION(
		Category= "Vcard|Bridge|Command",
		BlueprintCallable)
	bool IsRedoPossible() const
	{
		return RedoStack.Num() > 0;
	}

	UFUNCTION(
		Category= "Vcard|Bridge|Command",
		BlueprintCallable)
	int32 GetReUndoStackCount() const
	{
		return RedoStack.Num();
	}
	UPROPERTY(
		Category= "Vcard|Bridge|Command|Events",
		BlueprintReadWrite,EditAnywhere,BlueprintAssignable)
	FTVdjmVcardBridgeCommandEvent OnPrevExecutedCommand;
	UPROPERTY(
		Category= "Vcard|Bridge|Command|Events",
		BlueprintReadWrite,EditAnywhere,BlueprintAssignable)
	FTVdjmVcardBridgeCommandEvent OnPostExecutedCommand;
	UPROPERTY(
		Category= "Vcard|Bridge|CommandEvents",
		BlueprintReadWrite,EditAnywhere,BlueprintAssignable)
	FTVdjmVcardBridgeCommandEvent OnExecutedCommandSuccess;
	UPROPERTY(
		Category= "Vcard|Bridge|Command|Events",
		BlueprintReadWrite,EditAnywhere,BlueprintAssignable)
	FTVdjmVcardBridgeCommandEvent OnExecutedCommandFailed;

	UPROPERTY(
		Category= "Vcard|Bridge|Command|Events",
		BlueprintReadWrite,EditAnywhere,BlueprintAssignable)
	FTVdjmVcardBridgeChangeCommandEvent OnUndoStackChanged;
	UPROPERTY(
		Category= "Vcard|Bridge|Command|Events",
		BlueprintReadWrite,EditAnywhere,BlueprintAssignable)
	FTVdjmVcardBridgeChangeCommandEvent OnRedoStackChanged;

	UPROPERTY(
		Category= "Vcard|Bridge|Command|Events",
		BlueprintReadWrite,EditAnywhere,BlueprintAssignable)
	FTVdjmVcardBridgeCommandEvent OnUndoCommandExecuted;
	UPROPERTY(
		Category= "Vcard|Bridge|Command|Events",
		BlueprintReadWrite,EditAnywhere,BlueprintAssignable)
	FTVdjmVcardBridgeCommandEvent OnRedoCommandExecuted;

	UPROPERTY(
		Category= "Vcard|Bridge|Command|Events",
		BlueprintReadWrite,EditAnywhere,BlueprintAssignable)
	FTVdjmVcardBridgeCommandEvent OnUndoCommandSuccess;
	UPROPERTY(
		Category= "Vcard|Bridge|Command|Events",
		BlueprintReadWrite,EditAnywhere,BlueprintAssignable)
	FTVdjmVcardBridgeCommandEvent OnRedoCommandSuccess;

	UPROPERTY(
		Category= "Vcard|Bridge|Command|Events",
		BlueprintReadWrite,EditAnywhere,BlueprintAssignable)
	FTVdjmVcardBridgeCommandEvent OnUndoCommandFailed;
	UPROPERTY(
		Category= "Vcard|Bridge|Command|Events",
		BlueprintReadWrite,EditAnywhere,BlueprintAssignable)
	FTVdjmVcardBridgeCommandEvent OnRedoCommandFailed;

	TMap<UClass*,TObjectPtr<UObject>> ManagedCommandInstancePoolTable;
	TMap<FName,TSubclassOf<UObject>> CommandTable;
	TMap<FString,TWeakObjectPtr<UWidget>> UiWidgetCache;

	UPROPERTY()
	mutable TWeakObjectPtr<ATVdjmVcardBackgroundActor> BackgroundActor;
	
protected:
	virtual void BeginPlay() override;

	UPROPERTY()
	TArray<TScriptInterface<ITVdjmVcardCommandAction>> UndoStack;
	UPROPERTY()
	TArray<TScriptInterface<ITVdjmVcardCommandAction>> RedoStack;

	FString LastErrorString;
private:
	
};

UCLASS(BlueprintType)
class VDJMMOBILEUI_API UTVdjmVcardBridgeSwitcherMgrComp : public UActorComponent
{
	GENERATED_BODY()

	using VcardSwitcherPageValue = TPair<FString,int32>;
public:

	UFUNCTION(Category = "Vcard|Bridge|Switcher", BlueprintCallable)
	static bool RegisterSwitcherPage(ATVdjmVcardMainUiBridge* bridge,const FString& switcherName,const FString& pageName = FString(TEXT("")),int32 pageIndex = 0);
	
	UFUNCTION(BlueprintCallable)
	int32 TryGetSwitcherPageIndex(const FString& switcherPageName) ;
	UFUNCTION(BlueprintCallable)
	bool RegisterSwitcherPageIndex(const FString& switcherName ,const FString& switcherPageName, int32 pageIndex);

	bool CheckRegisterSwitcherPageIndex(const FString& switcherName ,int32 pageIndex) const;
	virtual void BeginPlay() override;

	//	page, <switcherName, pageIndex>
	TMap<FString,VcardSwitcherPageValue>	 SwitcherPageIndexTable;
protected:
	mutable TWeakObjectPtr<ATVdjmVcardMainUiBridge> UiBridge;
};



UCLASS(BlueprintType)
class VDJMMOBILEUI_API UTVdjmVcardCommandItemSelector : public UTVdjmVcardCommand
{
	GENERATED_BODY()

public:
	virtual int32 ExecuteCommand_Implementation() override;
	virtual int32 UndoCommand_Implementation () override;
	virtual FName GetCommandName_Implementation() const override
	{
		return FName(TEXT("select-item"));
	}
	virtual TArray<FString> GetCommandParamKeys_Implementation() const override
	{
		return {TEXT("container-name")	};
	}

	/**
	 * @brief 그냥 리플렉션으로 현재 선택된 아이템 오브젝트를 가져옴. 딱 여기서만 사용할 거임. 어차피 그 기능도 컨테이너의 베이스인 UObject 가 가지고 있던거라 이상한게 아님.
	 * @param foundContainerWidget 찾은 컨테이너 위젯
	 * @return 현재 선택된 아이템 오브젝트
	 */
	UObject* GetCurrentSelectedItemViaReflection(UUserWidget* foundContainerWidget);

	TWeakObjectPtr<UObject> OldSelectedItem;
	TWeakObjectPtr<UObject> NewSelectedItem;
	TWeakObjectPtr<UUserWidget> SelectedContainerWidget;
};


UCLASS(BlueprintType)
class VDJMMOBILEUI_API UTVdjmVcardUiModeContainer : public UUserWidget, public ITVdjmVcardInteractable
{
	GENERATED_BODY()

public:
	virtual void OnVcardSelect_Implementation(UObject* target) override;
	//	Call via bridge with command "select-item" from item widget
	//virtual void OnSelect(UObject* target) override;

	UFUNCTION(BlueprintCallable,BlueprintNativeEvent)
	UPanelSlot* AddPanelItem(UUserWidget* itemWidget);

	UPROPERTY(BlueprintReadWrite,EditAnywhere,
		meta=(BindWidget))
	TObjectPtr<UPanelWidget> ItemHolderPanel;
	UPROPERTY(BlueprintReadWrite,EditAnywhere)
	TWeakObjectPtr<ATVdjmVcardMainUiBridge> UiBridgeActor;

	UPROPERTY()
	TScriptInterface<ITVdjmVcardInteractable> CurrentSelectedItem;

	UFUNCTION(BlueprintCallable)
	void UpdateFullName();
	/**
	 * call RegisterWidgetCache, UTVdjmVcardUiModeContainer and 
	 */
	UFUNCTION(BlueprintCallable)
	void RegisterUiModeContainer();

	UPROPERTY(
		Category = "Vcard|Container",
	BlueprintReadWrite,EditAnywhere)
	FString ContainerDescriptorName;

	UPROPERTY(
		Category = "Vcard|Container",
		BlueprintReadOnly,VisibleAnywhere)
	FString ContainerFullName;
	UPROPERTY(
	Category = "Vcard|Container",
	BlueprintReadWrite,EditAnywhere)
	bool bAutoRegisterToBridge = true;

protected:

	virtual void NativeConstruct() override;

};


USTRUCT(BlueprintType)
struct VDJMMOBILEUI_API FTVdjmVcardUiModeDynamicItem
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite,EditAnywhere,meta = (MustImplement = "TVdjmVcardInteractable"))
	TSubclassOf<UUserWidget> ItemWidgetClass;
	UPROPERTY(BlueprintReadWrite,EditAnywhere)
	FName ItemWidgetName;
	UPROPERTY(BlueprintReadWrite,EditAnywhere)
	TWeakObjectPtr<UObject> ItemData;
};



UCLASS(BlueprintType)
class VDJMMOBILEUI_API UTVdjmVcardUiModeDynamicContainer : public UTVdjmVcardUiModeContainer
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable)
	void SetUpCreationList(const TArray<FTVdjmVcardUiModeDynamicItem>& itemCreationList);
	UFUNCTION(BlueprintCallable)
	void CreateDynamicItems();

	//virtual void OnVcardSelect_Implementation(UObject* target) override;
	void CreateSpacingWidget();
	virtual UPanelSlot* AddPanelItem_Implementation(UUserWidget* itemWidget) override;


public:
	UPROPERTY(
	Category = "Vcard|Container|Dynamic",
	BlueprintReadWrite,EditAnywhere)
	bool bItemSpacingEnabled = true;
	UPROPERTY(
	Category = "Vcard|Container|Dynamic",
	BlueprintReadWrite,EditAnywhere)
	bool bItemSpacingScalable = false;
	UPROPERTY(
	Category = "Vcard|Container|Dynamic",
	BlueprintReadWrite,EditAnywhere)
	FVector2D ItemSpacing = FVector2D(5.0f,5.0f);
	UPROPERTY(
	Category = "Vcard|Container|Dynamic",
	BlueprintReadWrite,EditAnywhere)
	TSubclassOf<UWidget> ItemSpacingWidgetClass;	//	default spacer

	UPROPERTY(
	Category = "Vcard|Container|Dynamic",
	BlueprintReadOnly,VisibleAnywhere)
	TArray<TScriptInterface<ITVdjmVcardInteractable>> DynamicItemInstanceList;
protected:
	virtual void NativeConstruct() override;

	UPROPERTY()
	TArray<FTVdjmVcardUiModeDynamicItem> mItemCreationList;

};




UCLASS()
class VDJMMOBILEUI_API UTVdjmVcardUiModeButtonItem : public UUserWidget, public ITVdjmVcardInteractable
{
	GENERATED_BODY()


public:

	UFUNCTION()
	void OnClickButton();

	UFUNCTION(BlueprintCallable)
	UTVdjmVcardUiModeContainer* GetOwnerContainer() const
	{
		return mOwnerContainer.Get();
	}

	virtual void OnVcardSelect_Implementation(UObject* target) override;
	virtual void OnVcardDeSelect_Implementation() override;


	UPROPERTY(
		Category="BaseWidget",
		BlueprintReadWrite,EditAnywhere,
		meta=(BindWidget))
	TObjectPtr<UBorder> IconBody;
	UPROPERTY(
	    Category="BaseWidget",
	    BlueprintReadWrite,EditAnywhere,
		meta=(BindWidget))
	TObjectPtr<UImage> IconImage;
	UPROPERTY(
		Category="BaseWidget",
		BlueprintReadWrite,EditAnywhere,
		meta=(BindWidget))
	TObjectPtr<UButton> SelectButton;
protected:
	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;

	UPROPERTY()
	TWeakObjectPtr<ATVdjmVcardMainUiBridge> UiBridgeActor;
private:
	TWeakObjectPtr<UTVdjmVcardUiModeContainer> mOwnerContainer;
	//void OnButtonPress();
};


DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FTVdjmVcardMoveUiSolverSignature, UPanelWidget*, parentPanel,                                               UPanelWidget*, childWidget, int32, moveDirection);

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FTVdjmVcardMoveUiSolverEndSignature);

UENUM(BlueprintType, Meta=(Bitflags,UseEnumValuesAsMaskValuesInEditor = "true" ))
enum class ETVdjmVcardMoveDirection : uint8
{
    ENone   = 0			UMETA(DisplayName="None"),
    ELeft   = 1 << 0	UMETA(DisplayName="Left"),
    ERight	= 1 << 1    UMETA(DisplayName="Right"),
    ETop	= 1 << 2    UMETA(DisplayName="Top"),
    EBottom	= 1 << 3    UMETA(DisplayName="Bottom"),
	EVerticality = ETop | EBottom UMETA(DisplayName="Verticality"),
    EHorizontality = ELeft | ERight UMETA(DisplayName="Horizontality"),
	EAll	= ELeft | ERight | ETop | EBottom UMETA(DisplayName="All"),
};
ENUM_CLASS_FLAGS(ETVdjmVcardMoveDirection);

//  TODO: MoveSolver ->  UUserWidgetExtension
UCLASS(Blueprintable)
class VDJMMOBILEUI_API UTVdjmVcardMobileMoveUiSolver : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable)
	FVector2D GetCurrentInputPoint(UPanelWidget* panel);

	UFUNCTION(BlueprintCallable)
	bool GetInputPoint(FVector2D& outPoint);
	void CalculateMovableArea();

	UFUNCTION(BlueprintCallable)
	void InitializeMoveSolver(UPanelWidget* parentPanel, UPanelWidget* childWidget,float leftBound,float rightBound,float topBound,float bottomBound,bool useInitChildLocalTranslation = false);

	UFUNCTION(BlueprintCallable)
	void StartEdit(
		float holdTime,
		UPARAM(meta=(Bitmask,BitmaskEnum= "ETVdjmVcardMoveDirection"))int32 moveDirection,
		const float updateInterval = 0.016f);

	virtual void BeginDestroy() override;

	UFUNCTION(BlueprintCallable)
	void EndEdit();

	UFUNCTION(BlueprintCallable)
	void StopUpdate ();

	FSlateRect GetWidgetRect(UPanelWidget* widget) const;
	FSlateRect GetChildWidgetRect() const;

	UFUNCTION(BlueprintCallable)
	FVector2D GetLastTranslation() const
	{
		return mLastTranslation;
	}

	FVector2D CalculateRestrictedDelta(FVector2D deltaPoint, int32 moveDirection);

	UPROPERTY(BlueprintAssignable,BlueprintCallable)
	FTVdjmVcardMoveUiSolverSignature OnBottomAndOptStart;
	UPROPERTY(BlueprintAssignable,BlueprintCallable)
	FTVdjmVcardMoveUiSolverSignature OnBottomAndOptUpdate;
	UPROPERTY(BlueprintAssignable,BlueprintCallable)
	FTVdjmVcardMoveUiSolverEndSignature OnBottomAndOptEnd;

	bool CheckCollisionRectArea(const FVector2D& deltaLU, const FVector2D& deltaRD) const;


protected:
	FTimerHandle mUpdateSliderHandle;

	TWeakObjectPtr<UPanelWidget> mParentPanelWidget;
	TWeakObjectPtr<UPanelWidget> mChildWidget;

	//EAxis::Type mRestrictAxis = EAxis::Type::X;
	UPROPERTY(meta = (Bitmask, BitmaskEnum="ETVdjmVcardMoveDirection"))
	int32 mMoveDirection = static_cast<int32>(ETVdjmVcardMoveDirection::ENone);

	FSlateRect mSlideBounds;

	FVector2D mBoundTopBottom;
	FVector2D mBoundLeftRight;

	// 2. 상태 정보
	FVector2D mLastTranslation; // 마지막으로 적용된 이동 값

	UPROPERTY()
	FVector2D mSnapshotAbsPoint;
	FVector2D mSnapshotPointInParent;

	FSlateRect mSnapshotChildRect;
	FVector2D mSnapshotTranslation;  // 처음 클릭 시 자식의 Translation
	
	FVector2D mSnapshotChildPivot;
	FVector2D mSnapshotchildSize;

	float mEvaluationHoldTime  = 0.25f;

	FVector2D mInitChildAbsLocation;
	FVector2D mInitChildLocalTranslation;
	bool mUseInitChildLocalTranslation = false;
	//FVector2D mInitChildLocalLocation;

	FSlateRect mMovableArea;
	FWidgetTransform mCorrectedTransform;

};

USTRUCT(BlueprintType)
struct FTVdjmVcardWidgetDescriptor
{
	GENERATED_BODY()

	UPROPERTY(
		Category = "Vcard|WidgetDescriptor",
		BlueprintReadWrite,EditAnywhere)
	FString WidgetName;
	
	UPROPERTY(
		Category = "Vcard|WidgetDescriptor",
		BlueprintReadWrite,EditAnywhere)
	TSubclassOf<UWidget> WidgetClass;
};

UCLASS(Blueprintable)
class VDJMMOBILEUI_API UTVdjmVcardMobileOptionContentsMain : public UUserWidget, public ITVdjmVcardInteractable
{
	GENERATED_BODY()

public:
	UPROPERTY(
	Category = "Vcard|OptionContents",
	BlueprintReadWrite,EditAnywhere)
	FSoftObjectPath LoadDataAssetPath;
	
	UPROPERTY(
	Category = "Vcard|OptionContents",
	BlueprintReadWrite,EditAnywhere)
	FString OptionContentsName;
	
	UPROPERTY(
	Category = "Vcard|OptionContents",
	BlueprintReadWrite,EditAnywhere,meta=(BindWidget))
	TObjectPtr<UTVdjmVcardUiModeDynamicContainer> OptionContainerWidget;
	
	UPROPERTY(
	Category = "Vcard|OptionContents",
	BlueprintReadWrite,EditAnywhere,meta=(BindWidget))
	TObjectPtr<UWidgetSwitcher>	 WSOptionContents;

	UPROPERTY(
	Category = "Vcard|OptionContents",
	BlueprintReadWrite,EditAnywhere)
	TArray<FTVdjmVcardWidgetDescriptor> OptionWidgetDescriptorList;

	UPROPERTY(Category = "Vcard"
	,BlueprintReadWrite,EditAnywhere)
	TWeakObjectPtr<ATVdjmVcardMainUiBridge> UiBridgeActor;

	UPROPERTY(Category = "Vcard"
	,BlueprintReadOnly)
	FString WSOptionContentsName;
	
protected:
	virtual void NativeConstruct() override;
	void GenerateOptionWidgetsByDescList();
	virtual void NativePreConstruct() override;

};


UCLASS()
class VDJMMOBILEUI_API UTVdjmVcardMobileMainWidget : public UCommonActivatableWidget, public ITVdjmVcardInteractable
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable,Category="Vcard|Config")
	static UTVdjmVcardConfigDataAsset* TryGetLoadedConfigDataAsset();
	
	UFUNCTION(BlueprintCallable)
	void DefaultSetting();

	/*
	 *
	 */
	UPROPERTY(BlueprintReadWrite,EditAnywhere)
	TObjectPtr<ATVdjmVcardMainUiBridge> UiBridgeActor;

	UPROPERTY(BlueprintReadWrite,EditAnywhere,meta=(BindWidget))
	TObjectPtr<UTVdjmVcardUiModeContainer> ModeContainerWidget;

	UPROPERTY(BlueprintReadOnly,VisibleAnywhere)
	TObjectPtr<UTVdjmVcardMobileMoveUiSolver> BottomAndOptionSolver;

	UPROPERTY(BlueprintReadWrite)
	TArray<TObjectPtr<UTVdjmVcardMobileOptionContentsMain>> OptionContentsList;

	UPROPERTY(BlueprintReadOnly)
	TMap<FName,int32> OptionContentsNameToIndexMap;

	//WSMainContent
	UPROPERTY(BlueprintReadWrite,EditAnywhere,meta=(BindWidget))
	TObjectPtr<UWidgetSwitcher> WSMainContent;
	
	UPROPERTY()
	TObjectPtr<UTVdjmVcardConfigDataAsset> LoadedConfigDataAsset;
	
protected:
	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;

	FTimerHandle mHoldOptionControlHandlerHandle;
};

UCLASS(Blueprintable)
class VDJMMOBILEUI_API UTVdjmVcardCommandChangeSwitcher : public UTVdjmVcardCommand
{
	GENERATED_BODY()

public:
	virtual int32 ExecuteCommand_Implementation() override;
	virtual int32 UndoCommand_Implementation() override;

	virtual FName GetCommandName_Implementation() const override
	{
		return FName(TEXT("switcher-control"));
	}
	virtual TArray<FString> GetCommandParamKeys_Implementation() const override
	{
		return {
			TEXT("switcher-name"), 
			TEXT("value-type"),	/*	*/
			TEXT("switcher-value"), 
		};
	}
	TWeakObjectPtr<UWidgetSwitcher> TargetSwitcher;
	
	int32 OldIndex = -1;
	int32 NewIndex = -1;
};

UCLASS(Blueprintable)
class VDJMMOBILEUI_API UTVdjmVcardCommandControl : public UTVdjmVcardCommand
{
	GENERATED_BODY()

public:
	virtual int32 ExecuteCommand_Implementation() override;
	virtual int32 UndoCommand_Implementation() override;
	virtual FName GetCommandName_Implementation() const override
	{
		return FName(TEXT("cmd-control"));
	}
	virtual TArray<FString> GetCommandParamKeys_Implementation() const override
	{
		return {
			TEXT("widget-name")
		};
	}
};

/**
 * @brief 아이템 데이터를 만들기 위한 생성 서술자.
 */
UCLASS(Blueprintable, BlueprintType,EditInlineNew,CollapseCategories,DefaultToInstanced)
class VDJMMOBILEUI_API UTVdjmVcardTileViewDataItemDesc : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(
			Category= "DataItem",
			BlueprintReadWrite,EditAnywhere)
	TMap<FName,FString> StringTable;

	UPROPERTY(
        Category= "DataItem",
        BlueprintReadWrite,EditAnywhere)
	TMap<FName,TSoftObjectPtr<UTexture2D>>	ImageTable;
};

UCLASS(BlueprintType)
class VDJMMOBILEUI_API UTVdjmVcardConfigDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(Blueprintable,EditAnywhere)
	TSubclassOf<ATVdjmVcardMainUiBridge> MainUiBridgeActorClass;
	UPROPERTY(Blueprintable,EditAnywhere)
	TSubclassOf<ATVdjmVcardBackgroundActor> BackgroundActorClass;
	UPROPERTY(Blueprintable,EditAnywhere)
	TSubclassOf<ATVdjmVcardMotionActor> MotionActorClass;

	
	UPROPERTY(Blueprintable,EditAnywhere)
	FTransform BackgroundActorSpawnTransform;
	UPROPERTY(Blueprintable,EditAnywhere)
	FTransform MotionActorSpawnTransform;
	
	UPROPERTY(Blueprintable,EditAnywhere)
	FSoftObjectPath BackgroundTileViewDataAssetPath;

	UPROPERTY(Blueprintable,EditAnywhere)
	FSoftObjectPath MotionTileViewDataAssetPath;
	
	UPROPERTY(Blueprintable,EditAnywhere)
	TSubclassOf<UTVdjmVcardTileViewItemLoader> BackgroundTileViewItemLoaderClass;
	
	UPROPERTY(Blueprintable,EditAnywhere)
	TSubclassOf<UTVdjmVcardTileViewItemLoader> MotionTileViewItemLoaderClass;

	UPROPERTY(Blueprintable,EditAnywhere)
	TSubclassOf<UTVdjmVcardMobileMainWidget> MobileMainWidgetClass;

	UPROPERTY(Blueprintable,EditAnywhere)
	TSubclassOf<APawn> EntryPawnClass;

	UPROPERTY(Blueprintable,EditAnywhere)
	FTransform EntryPawnSpawnTransform;

	UPROPERTY(Blueprintable,EditAnywhere)
	TSubclassOf<AActor> CharacterTargetActorClass;

	UPROPERTY(Blueprintable,EditAnywhere)
	TSubclassOf<UObject> CharacterSearcherClass;
};


UCLASS(BlueprintType)
class VDJMMOBILEUI_API UTVdjmVcardTileViewDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(
			Category= "DataItem",
			BlueprintReadWrite,EditAnywhere)
	TSubclassOf<UTVdjmVcardTileViewItemLoader>	ItemLoaderClass;
	
	UPROPERTY(
			Category= "DataItem",
			BlueprintReadWrite,EditAnywhere)
	TSubclassOf<UTVdjmVcardTileViewItemData>	ItemDataClass;
	UPROPERTY(
		Category= "DataItem",
		BlueprintReadWrite,EditAnywhere)
	TSubclassOf<UTVdjmVcardTileViewItemWidget>	ItemWidgetClass;
	
	UPROPERTY(
			Category= "DataItem",
			BlueprintReadWrite,EditAnywhere,Instanced,
			meta = (TitleProperty="StringTable", ShowOnlyInnerProperties))
	TArray<TObjectPtr<UTVdjmVcardTileViewDataItemDesc>>	 ItemDataList;

	UFUNCTION(BlueprintCallable)
	bool DbcValidDataAssetSetup() const;
};

/**
 * @brief 타일 뷰 아이템 '위젯'의 베이스 클래스.
 */
UCLASS(BlueprintType)
class VDJMMOBILEUI_API UTVdjmVcardTileViewItemWidget : public UUserWidget , public IUserObjectListEntry, public ITVdjmVcardInteractable
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable)
	void CallOnVcardSelect(UObject* target);
	UFUNCTION(BlueprintCallable)
	void CallOnVcardDeSelect();

	virtual void OnVcardDeSelect_Implementation() override;
	virtual void OnVcardSelect_Implementation(UObject* target) override;

	UFUNCTION(BlueprintCallable,BlueprintNativeEvent)
	void OnOffSelectVisuals(bool isSelected);
	

	UPROPERTY(BlueprintReadWrite,EditAnywhere)
	TWeakObjectPtr<UTVdjmVcardTileViewItemData> LinkedTileViewItemData;

	UPROPERTY(BlueprintReadWrite,EditAnywhere)
	bool IsSelected = false;
	
protected:
	virtual void NativeOnListItemObjectSet(UObject* ListItemObject) override;
	virtual void NativeOnItemSelectionChanged(bool bIsSelected) override;
};

/**
 * @brief 
 */
/**
 * @brief 타일 뷰 아이템 '데이터'의 베이스 클래스.
 * @details: 타일뷰 아이템 데이터 디테일
 * - 최초로 
 * - ITVdjmVcardInteractable 을 구현해서, 선택/비선택 이벤트를 받을 수 있음.
 * - OnVcardSelect 에서 자신이 연결된 컨테이너와 위젯,직접 액터를 컨트롤 함.
 * - DataAsset 을 통해서 생성이 되고, tileView에 AddItem:NativeOnListItemObjectSet 이 되면 PostBindToTileViewItemWidget 가 호출됨.
 * 
 */
UCLASS(BlueprintType)
class VDJMMOBILEUI_API UTVdjmVcardTileViewItemData : public UObject, public ITVdjmVcardInteractable
{
	GENERATED_BODY()

public:
	
	virtual void OnVcardDeSelect_Implementation() override;
	virtual void OnVcardSelect_Implementation(UObject* target) override;
	
	/**
	 * @brief Call from TileView item 'widget' in UTVdjmVcardTileViewItemWidget::NativeOnListItemObjectSet when binded. 
	 * @param itemWidget 
	 */
	UFUNCTION(BlueprintCallable,BlueprintNativeEvent)
	void PostBindToTileViewItemWidget(const TScriptInterface<IUserObjectListEntry>& itemWidget);

	UFUNCTION(BlueprintCallable,BlueprintNativeEvent)
	bool IsStoredDataValid() const;
	virtual bool IsStoredDataValid_Implementation() const PURE_VIRTUAL(UTVdjmVcardTileViewItemData::IsStoredDataValid_Implementation,return false; )

	UPROPERTY(BlueprintReadWrite,EditAnywhere)
	TWeakObjectPtr<UTVdjmVcardTileViewMainWidget> LinkedTileViewMainWidget;
	
	UPROPERTY(BlueprintReadWrite,EditAnywhere)
	TWeakObjectPtr<UTVdjmVcardTileViewItemWidget> LinkedTileViewItemWidget;
};

UCLASS(Blueprintable)
class VDJMMOBILEUI_API UTVdjmVcardTileViewItemLoader :	public UObject
{
	GENERATED_BODY()
public:

	//	TODO: 지금은 안바꾸는데 StoreTileViewItems 여기에서 호출해주는거 보면 LoadDataAssetForTileView 이거랑 인자도 같고 비슷한 역할인거 같은데 이거 하나로 합쳐도 될듯. 나중에 합쳐주자. 혹은 기능을 나누던가.
	UFUNCTION(BlueprintCallable,BlueprintNativeEvent)
	void InitTileViewLoader(UTileView* tileView,UTVdjmVcardTileViewDataAsset* dataAsset,UTVdjmVcardTileViewMainWidget*	ownerWidget);
	
	virtual void InitTileViewLoader_Implementation(UTileView* tileView,UTVdjmVcardTileViewDataAsset* dataAsset,UTVdjmVcardTileViewMainWidget*	ownerWidget)
	{
		mLinkedTileViewWidget = tileView;
		mLinkedTileViewMainWidget = ownerWidget;
	}
	
	UFUNCTION(BlueprintCallable,BlueprintNativeEvent)
	bool LoadDataAssetForTileView(UTileView* tileView,UTVdjmVcardTileViewDataAsset* dataAsset,UTVdjmVcardTileViewMainWidget*	ownerWidget);

	//	거진 다 가짐. 
	virtual bool LoadDataAssetForTileView_Implementation(UTileView* tileViewWidget, UTVdjmVcardTileViewDataAsset* dataAsset,UTVdjmVcardTileViewMainWidget* ownerTileViewMainWidget);

	virtual void LoadEndedTileViewItems()
	{
		//	Do nothing
	}
	
	bool CheckSkipTileViewItemKeys(const TArray<FName> stringKeys, const TArray<FName> imageKeys, const TObjectPtr<UTVdjmVcardTileViewDataItemDesc>& tileViewItemDesc);
	
	virtual TArray<FName> GetRequiredParamKeys(int32 type = 0) const
	{
		return {};
	}
	
	bool DbcValidClassesInLoader() const
	{
		return ItemDataClass != nullptr && ItemWidgetClass != nullptr;
	}
	bool DbcValidLinkedTileView() const
	{
		return mLinkedTileViewWidget.IsValid() && mLinkedTileViewMainWidget.IsValid();
	}
	UFUNCTION(BlueprintCallable)
	bool DbcValidDefaultLoaderSetup() const
	{
		return DbcValidClassesInLoader() && DbcValidLinkedTileView();
	}
	
	UPROPERTY(
		Category= "Vcard|TileViewFunctor",
		BlueprintReadWrite,EditAnywhere,
		meta=(MustImplement="TVdjmVcardInteractable"))
	UClass*	ItemDataClass;
	
	UPROPERTY(
		Category= "Vcard|Vcard|TileViewFunctor",
		BlueprintReadWrite,EditAnywhere,
		meta=(MustImplement="UserObjectListEntry"))
	UClass*	ItemWidgetClass;
	
protected:
	TWeakObjectPtr<UTileView> mLinkedTileViewWidget;
	TWeakObjectPtr<UTVdjmVcardTileViewMainWidget> mLinkedTileViewMainWidget;
	TWeakObjectPtr<UTVdjmVcardTileViewDataAsset> mLinkedTileViewDataAsset;
};


UCLASS(Blueprintable)
class VDJMMOBILEUI_API UTVdjmVcardTileViewMainWidget : public UUserWidget, public ITVdjmVcardCommandAction, public ITVdjmVcardInteractable
{
	GENERATED_BODY()

public:
	/*
	 *	TODO: 지금은 안할건데, 상태를 넣어야함
	 *	idle: 모든 기능이 가능한 상태
	 *	loading: 아이템 로딩중. 이 상태에서는 선택 불가
	 *	이걸 당장에 나눈이유는 로더를 만들때 부담을 줄이려고.
	 */
	double LoadStartTime = 0.0f;
	double MaxLoadExceedTime = 15.0f; //	15초 이상 로딩중이면 강제 종료
	
	bool EntrustBeginFlag = true; //	임시로 로딩 상태를 나타내는 변수

	FTimerHandle LoadExceedFallbackTimerHandle;
	
	bool IsEntrustPossible() const
	{
		return EntrustBeginFlag == true;
	}
	bool IsEntrustItems() const
	{
		return EntrustBeginFlag == false;
	}
	
	bool IsLoadingItems() const
	{
		return IsEntrustItems();
	}

	bool IsOverLoadFallbackTime() const
	{
		return (FPlatformTime::Seconds() - LoadStartTime) >= MaxLoadExceedTime;
	}

	UFUNCTION(BlueprintCallable)
	void BeginEntrustItemInstance(UTVdjmVcardTileViewItemLoader* caller);
	UFUNCTION(BlueprintCallable)
	void EndEntrustItemInstance();
	
	UFUNCTION(BlueprintCallable)
	void EntrusNewInstance(UTVdjmVcardTileViewItemData* itemData);
	void EntrustAppendNewInstances(TArray<UTVdjmVcardTileViewItemData*> itemDatas);
	
	virtual ATVdjmVcardMainUiBridge* GetVcardUiBridge_Implementation() const override
	{
		return UiBridgeActor.Get();
	}
	
	virtual void OnVcardDeSelect_Implementation() override;
	virtual void OnVcardSelect_Implementation(UObject* target) override;
	
	virtual ETVdjmVcardCommandManagedTypes GetCommandManagedType_Implementation() const override
	{
		return ETVdjmVcardCommandManagedTypes::EManagedSingleton;
	}
	virtual FName GetCommandName_Implementation() const override
	{
		return TEXT("tileview-select-item");
	}
	virtual TArray<FString> GetCommandParamKeys_Implementation() const override
	{
		return {};
	}
	
	UPROPERTY(
		Category= "Vcard|TileView|Config",
		BlueprintReadWrite,EditAnywhere)
	FString TileViewMainName;
	
	UPROPERTY(
		Category= "Vcard|TileView|Config",
		BlueprintReadWrite,EditAnywhere)
	FSoftObjectPath TileViewDataAssetPath;
	
	UPROPERTY(
		Category= "Vcard|TileView|Config",
		BlueprintReadWrite,EditAnywhere)
	FSoftObjectPath RecentItemDataAssetPath;

	UPROPERTY(
	    Category= "Vcard|TileView",
	    BlueprintReadWrite,EditAnywhere,
	    meta=(BindWidget))
	TObjectPtr<UTileView> TileViewWidget;
	
	UPROPERTY(
	    Category= "Vcard|TileView",
	    BlueprintReadOnly)
	TArray<TScriptInterface<ITVdjmVcardInteractable>> ItemInstanceList;

	UPROPERTY(
		Category= "Vcard|TileView",
		BlueprintReadOnly)
	TMap<FName,int32> ItemNameToIndexMap;
	
	UPROPERTY(	BlueprintReadWrite,EditAnywhere)
	TWeakObjectPtr<UTVdjmVcardTileViewItemLoader> CurrentTileViewItemLoader;

	UPROPERTY(BlueprintReadOnly,VisibleAnywhere)
	int32 LoadRequestUniqueId = 0;

	UPROPERTY(BlueprintReadWrite)
	FTimerHandle LoadDataAssetTimerHandle;

	UPROPERTY(	BlueprintReadWrite)
	TWeakObjectPtr<UTVdjmVcardTileViewItemData> CurrentSelectedItemData;

	UPROPERTY()
	TWeakObjectPtr<UTVdjmVcardTileViewDataAsset> RecentLoadedItemDataAsset;
	
protected:
	UFUNCTION()
	void LoadExceedFallback();

	//	실질적인 작동부
	void OnBeginLoadItems(UTVdjmVcardTileViewItemLoader* caller);
	void OnEndLoadItems();
	/**
	 * 
	 * @param itemDataAssetPath 넣은 itemDataAssetPath 에따라서 다르게 처리됨.
	 * @return 
	 */
	bool StoreTileViewItems(const FSoftObjectPath& itemDataAssetPath);
	void RegisterTileViewWidget(const FSoftObjectPath& itemDataAssetPath);
	//	FLoadSoftObjectPathAsyncDelegate, const FSoftObjectPath&, UObject*);
	virtual void NativeConstruct() override;
	virtual void NativePreConstruct() override;
	
	TWeakObjectPtr<ATVdjmVcardMainUiBridge> UiBridgeActor;
	UPROPERTY()
	TMap<FString,TObjectPtr<UTVdjmVcardTileViewDataAsset>> LoadedDataAssetCache;
	UPROPERTY()
	TMap<FString,TObjectPtr<UTVdjmVcardTileViewItemLoader>> LoadedItemLoaderCache;
};


UCLASS(Blueprintable)
class VDJMMOBILEUI_API UTVdjmVcardTileViewBgItemLoader : public UTVdjmVcardTileViewItemLoader
{
	GENERATED_BODY()

public:
	virtual TArray<FName> GetRequiredParamKeys(int32 type = 0) const override
	{
		switch (type)
		{
		case 0:
			return {TEXT("background-name")};
		case 1:
			return {
				TEXT("thumbnail"),
				TEXT("background")
			};
		default:
			return {};
		}
	}
	
	virtual bool LoadDataAssetForTileView_Implementation(UTileView* tileView, UTVdjmVcardTileViewDataAsset* dataAsset,UTVdjmVcardTileViewMainWidget* ownerMainWidget) override;
	virtual void InitTileViewLoader_Implementation(UTileView* tileView, UTVdjmVcardTileViewDataAsset* dataAsset,
		UTVdjmVcardTileViewMainWidget* ownerWidget) override;
};
UCLASS(BlueprintType)
class VDJMMOBILEUI_API UTVdjmVcardTileViewBgItemWidget : public UTVdjmVcardTileViewItemWidget
{
	GENERATED_BODY()
public:
	UPROPERTY(
		Category= "Vcard|ItemWidget|Background",
		BlueprintReadWrite,EditAnywhere)
	TWeakObjectPtr<UTexture2D> ThumbnailImage;
	UPROPERTY(
		Category= "Vcard|ItemWidget|Background",
		BlueprintReadWrite,EditAnywhere,meta=(BindWidget))
	TObjectPtr<UBorder> ThumbnailImageWidget;
	UPROPERTY(
		Category= "Vcard|ItemWidget|Background",
		BlueprintReadWrite,EditAnywhere,meta=(BindWidget))
	FString BackgroundName;

};


UCLASS(BlueprintType)
class VDJMMOBILEUI_API UTVdjmVcardTileViewBgItemData : public UTVdjmVcardTileViewItemData
{
	GENERATED_BODY()

public:
	virtual void
	PostBindToTileViewItemWidget_Implementation(const TScriptInterface<IUserObjectListEntry>& itemWidget) override;
	virtual bool IsStoredDataValid_Implementation() const override
	{
		if (IsValid(BackgroundThumbnailImage) && IsValid(BackgroundImage))
		{
			return true;
		}
		return false;
	}
	
	virtual void OnVcardDeSelect_Implementation() override;
	virtual void OnVcardSelect_Implementation(UObject* target) override;

	UPROPERTY(BlueprintReadWrite,EditAnywhere)
	FString BackgroundName;
	
	UPROPERTY(BlueprintReadWrite,EditAnywhere)
	TObjectPtr<UTexture2D> BackgroundThumbnailImage;
	
	UPROPERTY(BlueprintReadWrite,EditAnywhere)
	TObjectPtr<UTexture2D> BackgroundImage;
	/*
	 * params를 건들고 싶지만 스탑
	 */
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FTVdjmVcardBackgroundActorEvent, ATVdjmVcardBackgroundActor*, backgroundActor,bool, isSelected);

UENUM(Blueprintable,BlueprintType)
enum  ETVdjmVcardBackgroundPlaneAxis 
{
	EVcardPlaneXYZ		UMETA(DisplayName="X-Y-Z"),
	EVcardPlaneXZY		UMETA(DisplayName="X-Z-Y"),
	EVcardPlaneYXZ		UMETA(DisplayName="Y-X-Z"),
	EVcardPlaneYZX		UMETA(DisplayName="Y-Z-X"),
	EVcardPlaneZXY		UMETA(DisplayName="Z-X-Y"),
	EVcardPlaneZYX		UMETA(DisplayName="Z-Y-X"),
};
USTRUCT(Blueprintable)
struct FVdjmVcardBgMeshAnalysisResult
{
	GENERATED_BODY()

	EAxis::Type ThicknessAxis;
	EAxis::Type WidthAxis;
	EAxis::Type HeightAxis;
	FRotator CorrectionRot;
	FVector MeshSize;

	bool IsValidAnalysisResult() const
	{
		if (ThicknessAxis == EAxis::None)
		{
			return false;
		}
		if (WidthAxis == EAxis::None)
		{
			return false;
		}
		if (HeightAxis == EAxis::None)
		{
			return false;
		}
		if (MeshSize.IsNearlyZero())
		{
			return false;
		}
		
		return true;
	}
	
	float GetMeshSize(EAxis::Type type) const
	{
		switch (ThicknessAxis)
		{
		case EAxis::None:
			break;
		case EAxis::X:
			return MeshSize.X;
		case EAxis::Y:
			return MeshSize.Y;
		case EAxis::Z:
			return MeshSize.Z;
		}
		return 0;
	}
	
	float GetThicknessValue() const
	{
		return GetMeshSize(ThicknessAxis);
	}
	float GetWidthValue() const 
	{
		return GetMeshSize(WidthAxis);
	}
	float GetHeightValue() const 
	{
		return GetMeshSize(HeightAxis);
	}

	static FVdjmVcardBgMeshAnalysisResult AnalyzeMesh(UStaticMesh* inMesh)
	{
		FVdjmVcardBgMeshAnalysisResult result = {};
		if (inMesh)
		{
			FVector meshExtend = inMesh->GetBoundingBox().GetSize(); // BoxExtent * 2
			result.MeshSize = meshExtend;
			 // = FMath::Max(meshExtend.X, 0.001f);
			 // = FMath::Max(meshExtend.Y, 0.001f);
			 // = FMath::Max(meshExtend.Z, 0.001f);
			float exX = FMath::Max(meshExtend.X, KINDA_SMALL_NUMBER);
			float exY = FMath::Max(meshExtend.Y, KINDA_SMALL_NUMBER);
			float exZ = FMath::Max(meshExtend.Z, KINDA_SMALL_NUMBER);
			
			if (exZ <= exX && exZ <= exY) // Z가 제일 얇음 (대부분의 Plane)
			{
				result.ThicknessAxis = EAxis::Z;
				result.WidthAxis = EAxis::X;  // 보통 언리얼은 X가 Forward지만 Plane은 XY평면
				result.HeightAxis = EAxis::Y; 
				// Z가 정면인 놈을 X가 정면이 되게 하려면: Pitch -90
				result.CorrectionRot = FRotator(-90.0f, 0.0f, 0.0f);
			}
			else if (exY <= exX && exY <= exZ) // Y가 제일 얇음 (세워진 벽)
			{
				result.ThicknessAxis = EAxis::Y;
				result.WidthAxis = EAxis::X; 
				result.HeightAxis = EAxis::Z;
				// Y가 정면인 놈을 X가 정면이 되게 하려면: Yaw -90
				result.CorrectionRot = FRotator(0.0f, -90.0f, 0.0f);
			}
			else // X가 제일 얇음 (이미 정면을 봄)
			{
				result.ThicknessAxis = EAxis::X;
				result.WidthAxis = EAxis::Y;
				result.HeightAxis = EAxis::Z;
				// 이미 X가 정면이므로 회전 없음
				result.CorrectionRot = FRotator::ZeroRotator;
			}
		}
		return result;
	}
};
UCLASS(Blueprintable)
class VDJMMOBILEUI_API ATVdjmVcardBackgroundActor : public AActor, public ITVdjmVcardInteractable
{
	GENERATED_BODY()
public:
	ATVdjmVcardBackgroundActor();
	
	static ATVdjmVcardBackgroundActor* TryGetBackgroundActor(UWorld* world);
	
	virtual void OnVcardDeSelect_Implementation() override;
	//	From UTVdjmVcardTileViewBgItemData
	virtual void OnVcardSelect_Implementation(UObject* target) override;

	UFUNCTION(BlueprintCallable)
	void VisibleBackground(bool bVisible);

	UFUNCTION(BlueprintCallable,BlueprintNativeEvent)
	void SizeModify(ATVdjmVcardBackgroundActor* backgroundActor,bool isSelected);
	
	UPROPERTY(BlueprintAssignable,BlueprintCallable)
	FTVdjmVcardBackgroundActorEvent OnBackgroundSelected; 
	
	UPROPERTY(BlueprintReadWrite,EditAnywhere)
	TObjectPtr<USceneComponent> RootComp;

	UPROPERTY(BlueprintReadWrite,EditAnywhere)
	TObjectPtr<UStaticMeshComponent> BackgroundComp;
	
	UPROPERTY(BlueprintReadWrite,EditAnywhere)
	TObjectPtr<UMaterialInstanceDynamic> BackgroundMaterialInstance;

	UPROPERTY(BlueprintReadWrite,EditAnywhere)
	TWeakObjectPtr<UTVdjmVcardTileViewBgItemData> CurrentSelectedBgItemData;

	UPROPERTY(BlueprintReadWrite,EditAnywhere)
	TEnumAsByte< ETVdjmVcardBackgroundPlaneAxis> MeshAxisSwizzle = ETVdjmVcardBackgroundPlaneAxis::EVcardPlaneXYZ;
	
	UPROPERTY(BlueprintReadWrite,EditAnywhere)
	FVector2D BackgroundPlaneRestrictDistance = FVector2D(1.0f,10.0f);
	
	UPROPERTY(BlueprintReadWrite,EditAnywhere)
	bool bBillboardMode = true;
	UPROPERTY(BlueprintReadWrite,EditAnywhere)
	bool bCoverMode = true;
	UPROPERTY(BlueprintReadWrite,EditAnywhere)
	FVector BillboardOffset = FVector(0.0f,0.0f,0.0f);

	UPROPERTY(BlueprintReadWrite,EditAnywhere)
	float BillboardDistance = 500.f;
	
protected:
	virtual void BeginPlay() override;
};

//DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FTVdjmVcardMotion)

UCLASS(Blueprintable)
class VDJMMOBILEUI_API UTVdjmVcardTileViewMotionItemLoader : public UTVdjmVcardTileViewItemLoader
{
	GENERATED_BODY()
public:
	
	
	virtual TArray<FName> GetRequiredParamKeys(int32 type = 0) const override
	{
		switch (type)
		{
		case 0:
			return {
				TEXT("motion-name"),
				TEXT("motion-path")
			};
		case 1:
			return {
				TEXT("thumbnail")
			};
		default:
			return {};
		}
	}
	virtual bool LoadDataAssetForTileView_Implementation(UTileView* tileView, UTVdjmVcardTileViewDataAsset* dataAsset,UTVdjmVcardTileViewMainWidget* ownerWidget) override;
	void RemoveAnimationLoadRequest(const FString& path);
	
	virtual void LoadEndedTileViewItems() override;

	virtual void InitTileViewLoader_Implementation(UTileView* tileViewWidget, UTVdjmVcardTileViewDataAsset* dataAsset,UTVdjmVcardTileViewMainWidget* ownerTileViewMainWidget) override;

	/*	anim != nullptr 이면 UTVdjmVcardTileViewMotionItemWidget 를 통해서 tileView 에 추가.
	 *	anim == nullptr 이면 mASyncLoadRequestIdToMotionItemDataMap 여기에 있는 것을 제거 해서 제거 해줌.
	 */ 
	void TryAddAnimation(const FString& path,UAnimSequence* anim);
	
private:
	UPROPERTY()
	TMap<FString, TObjectPtr<UTVdjmVcardTileViewMotionItemData>> mPendingMotionItemDataMap;
};

UCLASS(BlueprintType)
class VDJMMOBILEUI_API UTVdjmVcardTileViewMotionItemData : public UTVdjmVcardTileViewItemData
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable,BlueprintNativeEvent)
	void ASyncLoadCallback(const FSoftObjectPath& loadPath, UObject* loadedObjResult);
	
	virtual void
	PostBindToTileViewItemWidget_Implementation(const TScriptInterface<IUserObjectListEntry>& itemWidget) override;
	
	virtual bool IsStoredDataValid_Implementation() const override
	{
		if (not MotionName.IsEmpty() && IsValid(MotionThumbnailImage) && IsValid(MotionAnimation))
		{
			return true;
		}
		return false;
	}
	
	virtual void OnVcardDeSelect_Implementation() override;
	virtual void OnVcardSelect_Implementation(UObject* target) override;

	bool IsPlayableMotionAnimation() const
	{
		return LoadedMotionAnimation && IsValid(MotionAnimation);
	}
	
	
	//FLoadSoftObjectPathAsyncDelegate MotionLoadDelegate;


	UPROPERTY(Blueprintable,BlueprintReadWrite)
	FString MotionName;
	UPROPERTY(Blueprintable,BlueprintReadWrite)
	TObjectPtr<UTexture2D> MotionThumbnailImage;
	UPROPERTY(Blueprintable,BlueprintReadWrite)
	TObjectPtr<UAnimSequence> MotionAnimation;
	UPROPERTY(Blueprintable,BlueprintReadWrite)
	TWeakObjectPtr<ATVdjmVcardMotionActor> LinkedMotionActor;
	
	
	UPROPERTY(Blueprintable,BlueprintReadWrite)
	bool LoadedMotionAnimation = false;
	UPROPERTY(Blueprintable,BlueprintReadWrite)
	int32 LoadRequestUniqueId = -1;
	
	//UPROPERTY(Blueprintable,BlueprintReadWrite)
	//FLoadAssetAsyncOptionalParams LoadOptionParams;
};

USTRUCT(Blueprintable)
struct VDJMMOBILEUI_API FTVdjmVcardSkeletonCachedKey
{
	GENERATED_BODY()

	uintptr_t SkeletonA = 0;
	uintptr_t SkeletonB = 0;

	FTVdjmVcardSkeletonCachedKey() = default;
	
	FTVdjmVcardSkeletonCachedKey(uintptr_t inSkelA, uintptr_t inSkelB)
		: SkeletonA(inSkelA), SkeletonB(inSkelB)
	{
		if (inSkelA > inSkelB)
		{
			SkeletonA = inSkelB;
			SkeletonB = inSkelA;
		}
	}

	bool operator==(const FTVdjmVcardSkeletonCachedKey& other) const
	{
		return SkeletonA == other.SkeletonA && SkeletonB == other.SkeletonB;
	}

	friend uint32 GetTypeHash(const FTVdjmVcardSkeletonCachedKey& key)
	{
		return HashCombine(GetTypeHash(key.SkeletonA), GetTypeHash(key.SkeletonB));
	}
};

UCLASS(BlueprintType)
class VDJMMOBILEUI_API UTVdjmVcardTileViewMotionItemWidget : public UTVdjmVcardTileViewItemWidget
{
	GENERATED_BODY()
public:
	
	UPROPERTY(
		Category= "Vcard|ItemWidget|Motion",
		BlueprintReadWrite,EditAnywhere,meta=(BindWidget))
	TObjectPtr<UBorder> ThumbnailImageWidget;
	
	UPROPERTY(
		Category= "Vcard|ItemWidget|Motion",
		BlueprintReadWrite,EditAnywhere,meta=(BindWidget))
	TObjectPtr<UTextBlock> MotionTextWidget;
	
	UPROPERTY(
		Category= "Vcard|ItemWidget|Motion",
		BlueprintReadWrite,EditAnywhere,meta=(BindWidget))
	FString MotionName;

	
};

USTRUCT(BlueprintType)
struct VDJMMOBILEUI_API FTVdjmVcardMotionInfo 
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	TWeakObjectPtr<USkeletalMeshComponent> TargetSkeletalMeshComp;
	
	UPROPERTY(EditAnywhere)
	TEnumAsByte<EAnimationMode::Type> PrevAnimMode;
	UPROPERTY(EditAnywhere)
	FSingleAnimationPlayData PrevAnimPlayData;
	UPROPERTY(EditAnywhere)
	TSubclassOf<UAnimInstance> PrevAnimInstanceClass;
	UPROPERTY(EditAnywhere)
	bool bHasPrevAnimPlayData = false;
	UPROPERTY(EditAnywhere)
	bool bHasPrevAnimInstance = false;

	UPROPERTY(EditAnywhere)
	TWeakObjectPtr<UAnimSequence> CurrentMotionAnimation;
	
	UPROPERTY(EditAnywhere)
	float PrevPositionInSec;
	UPROPERTY(EditAnywhere)
	float PrevPlayRate;
	UPROPERTY(EditAnywhere)
	bool PrevLooping = false;

	UPROPERTY(EditAnywhere)
	bool bPaused = false;
	UPROPERTY(EditAnywhere)
	float PausePlayRate = 1.0f;
	UPROPERTY(EditAnywhere)
	float PausedPositionInSec = 0.0f;
	
	
	void Clear()
    {
        TargetSkeletalMeshComp = nullptr;
        PrevAnimMode = EAnimationMode::Type::AnimationBlueprint;
		PrevAnimPlayData = FSingleAnimationPlayData();
        PrevAnimInstanceClass = nullptr;
        PrevPositionInSec = 0.0f;
        PrevPlayRate = 1.0f;
        PrevLooping = false;
    }
	void RestorePreviousState();
	void CaptureCurrentState(USkeletalMeshComponent* skelComp);

	void PlayMotionAnimation(UAnimSequence* motionAnim,float playRate = 0.0f, float startPositionInSec = 0.0f);
	void StopMotionAnimation();
	void PauseMotionAnimation();
	void PauseOff()
	{
		if (bPaused)
		{
			PausePlayRate = 1.f;
			PausedPositionInSec = 0.f;
			bPaused = false;
		}
	}
};

USTRUCT(BlueprintType)
struct VDJMMOBILEUI_API FTVdjmVcardMotionSnapshot
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere)
	TArray<FTVdjmVcardMotionInfo> TargetSkeletalMeshComps;
	UPROPERTY(EditAnywhere)
	TWeakObjectPtr<UTVdjmVcardTileViewMotionItemData> TriggeredMotionData;
	
	void CaptureSnapshot(const ATVdjmVcardMotionActor* motionActor, const AActor* targetActor, UTVdjmVcardTileViewMotionItemData* motionItemData);
	bool DbcValidSnapshot() const
    {
        return TargetSkeletalMeshComps.Num() > 0 && TriggeredMotionData.IsValid();
    }
	
	void RestoreSnapshot();
	
	void Clear()
    {
        TargetSkeletalMeshComps.Empty();
        TriggeredMotionData = nullptr;
    }
	void PlayMotionAnimation(float playRate = 1.0f, float startPositionInSec = 0.0f);
	void PauseMotionAnimation();
	void StopMotionAnimation();
};

UCLASS(Blueprintable)
class VDJMMOBILEUI_API ATVdjmVcardMotionActor : public AActor, public ITVdjmVcardInteractable, public ITVdjmVcardCommandAction
{
GENERATED_BODY()

public:
	static ATVdjmVcardMotionActor* TryGetMotionActor(UWorld* world);

	ATVdjmVcardMotionActor();
	AActor* TryFindMotionTargetActor() const;
	
	UFUNCTION(BlueprintCallable)
	void SetMotionPlayInfo(float playRate, float startPositionInSec);
	UFUNCTION(BlueprintCallable)
	void PlayMotionAnimation();
	UFUNCTION(BlueprintCallable)
	void PauseMotionAnimation();
	UFUNCTION(BlueprintCallable)
	void StopMotionAnimation();
	
	//	 From UTVdjmVcardTileViewMainWidget
	
	bool IsAnimAndSkelCompatible(UAnimSequence* animSeq, USkeletalMeshComponent* skelComp) const;
	
	UFUNCTION(BlueprintCallable)
	void CaptureVcardMotionActorSnapshot(UTVdjmVcardTileViewMotionItemData* bgItem,AActor* targetActor);
	virtual void OnVcardDeSelect_Implementation() override;
//	From UTVdjmVcardTileViewMotionItemData
	virtual void OnVcardSelect_Implementation(UObject* target) override;

	virtual int32 ExecuteCommand_Implementation() override;
	virtual ETVdjmVcardCommandManagedTypes GetCommandManagedType_Implementation() const override;
	virtual FName GetCommandName_Implementation() const override;
	virtual TArray<FString> GetCommandParamKeys_Implementation() const override;
	virtual int32 UndoCommand_Implementation() override;
	
	TWeakObjectPtr<UTVdjmVcardTileViewMotionItemData> CurrentSelectedMotionItemData;
	FTVdjmVcardMotionSnapshot* CurrentMotionSnapshotPtr;
	
	TWeakObjectPtr<ATVdjmVcardMainUiBridge> UiBridgeActor;
	TWeakObjectPtr<AActor> TargetActorForMotion;
	
	TArray<FTVdjmVcardMotionSnapshot> MotionSnapshotStack;

	float DefaultPlayRate = 1.0f;
	float DefaultPositionInSec = 0.0f;

	UPROPERTY(BlueprintReadWrite,EditAnywhere)
	float CurrentPlayRate = 1.0f;
	UPROPERTY(BlueprintReadWrite,EditAnywhere)
	float CurrentStartPositionInSec = 0.0f;
	UPROPERTY(BlueprintReadWrite,EditAnywhere)
	TObjectPtr<USceneComponent> RootComp;

	UPROPERTY(BlueprintReadWrite,EditAnywhere)
	FTVdjmVcardMotionSnapshot IdleMotionCapture;
protected:
	
	virtual void BeginPlay() override;
	
	mutable TMap<FTVdjmVcardSkeletonCachedKey,bool> mSkeletonAnimCompatibilityCache;
};

// UCLASS(Blueprintable)
// class VDJMMOBILEUI_API UVdjmVcardTileViewGeometryExtension : public UUserWidgetExtension
// {
// 	GENERATED_BODY()
//
// public:
// 	UFUNCTION(BlueprintCallable)
// 	void StartEdit(
// 		float holdTime,
// 		UPARAM(meta=(Bitmask,BitmaskEnum= "ETVdjmVcardMoveDirection"))int32 moveDirection,
// 		const float updateInterval = 0.016f);
// };



/**
 *
 */
UCLASS()
class VDJMMOBILEUI_API UVdjmMobileUiCore : public UObject
{
	GENERATED_BODY()
};
