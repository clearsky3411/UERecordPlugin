#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

class UVdjmRecordEventBase;

namespace VdjmRecordEventJson
{
	VDJMRECORDER_API bool SerializeEventNodeToJsonObject(
		const UVdjmRecordEventBase* EventNode,
		TSharedPtr<FJsonObject>& OutJsonObject,
		FString& OutError);

	VDJMRECORDER_API bool DeserializeEventNodeFromJsonObject(
		const TSharedPtr<FJsonObject>& EventJsonObject,
		UObject* Outer,
		UVdjmRecordEventBase*& OutEventNode,
		FString& OutError);
}

