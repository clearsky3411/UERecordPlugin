#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "VdjmVcardDescriptorTypes.h"
#include "VdjmVcardDescriptorReceiver.generated.h"

UINTERFACE(BlueprintType)
class VDJMVCARD_API UVcardDescriptorReceiver : public UInterface
{
	GENERATED_BODY()
};

class VDJMVCARD_API IVcardDescriptorReceiver
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Vcard|Descriptor")
	void ApplyVcardWidgetAttachment(const FVcardWidgetAttachDescriptor& attachmentDescriptor, UObject* payloadData);
};
