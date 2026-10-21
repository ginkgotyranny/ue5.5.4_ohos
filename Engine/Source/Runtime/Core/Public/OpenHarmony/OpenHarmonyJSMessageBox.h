#pragma once

#include "CoreMinimal.h"

#if USE_OPENHARMONY_NAPI

#include "aki/jsbind.h"

// Wrapper for com/epicgames/ue4/MessageBox*.java.
class FJSOpenHarmonyMessageBox
{
public:
	void SetCaption(const FString & Text);
	void SetText(const FString & Text);
	void AddButton(const FString & Text);
	void Clear();
	int32 Show();
};

#endif