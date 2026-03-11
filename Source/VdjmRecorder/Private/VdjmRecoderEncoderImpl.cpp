// Fill out your copyright notice in the Description page of Project Settings.


#include "VdjmRecoderEncoderImpl.h"

#if PLATFORM_WINDOWS
#include "VdjmWMF/VdjmRecorderWndEncoder.h"
#elif PLATFORM_ANDROID
#include "VdjmAndroid/VdjmAndroidCore.h"
#elif PLATFORM_IOS
//#include "VdjmRecorderiOSEncoder.h"
#endif

void FVdjmVideoEncoderBase::ChangeEncoderStatus(EVdjmEncoderStatus newStatus)
{
	EVdjmEncoderStatus prevStatus = CurrentStatus;
	CurrentStatus = newStatus;
	if (newStatus == EVdjmEncoderStatus::EError)
	{
		UE_LOG(LogVdjmRecorderCore, Error, TEXT("Encoder Status Changed : %s -> %s"), *UEnum::GetValueAsString(prevStatus), *UEnum::GetValueAsString(newStatus));
	}
	else
	{
		UE_LOG(LogVdjmRecorderCore, Log, TEXT("Encoder Status Changed : %s -> %s"), *UEnum::GetValueAsString(prevStatus), *UEnum::GetValueAsString(newStatus));
	}
	
	// if (bUseStateChangeEvent)
	// {
	// 	FVdjmEncoderStatus::DbcGameThreadTask([
	// 	weakThis = TWeakPtr<FVdjmVideoEncoderBase>(AsShared()),
	// 	prevStatus, newStatus
	// ]
	// {
	// 	if (weakThis.IsValid())
	// 	{
	// 		switch (EStatusResourceChangeType changeType = 
	// 			VDJM_STATUS_RES_CHANGE_TYPE(prevStatus, newStatus)) {
	// 		case EStatusResourceChangeType::ENewToReady:
	// 			weakThis.Pin()->ChangeStatusNewToReady();
	// 			break;
	// 		case EStatusResourceChangeType::EReadyToRunning:
	// 			weakThis.Pin()->ChangeStatusReadyToRunning();
	// 			break;
	// 		case EStatusResourceChangeType::ERunningToWaiting:
	// 			weakThis.Pin()->ChangeStatusRunningToWaiting();
	// 			break;
	// 		case EStatusResourceChangeType::EWaitingToReady:
	// 			weakThis.Pin()->ChangeStatusWaitingToReady();
	// 			break;
	// 		case EStatusResourceChangeType::EWaitingToTerminated:
	// 			break;
	// 		}
	// 	
	// 	}
	// });
	// }
}

