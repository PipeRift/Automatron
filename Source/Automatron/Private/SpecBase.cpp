// Copyright 2020 Splash Damage, Ltd. - All Rights Reserved.

#include "SpecBase.h"

#if WITH_EDITOR
#include <Tests/AutomationEditorPromotionCommon.h>
#include <Editor.h>
#endif


// In order to know how many tests we have defined, we need to access private members of FAutomationSpecBase!
// Thats why we replicate its layout as public for reinterpreting later.
// This is a HACK and should be removed when UE4 API updates.
struct FAutomationSpecBaseLayoutMock : public FAutomationTestBase
	, public TSharedFromThis<FAutomationSpecBase>
{
	struct FSpecIt
	{
		FString Description;
		FString Id;
		FString Filename;
		int32 LineNumber;
		TSharedRef<IAutomationLatentCommand> Command;
	};

	struct FSpecDefinitionScope
	{
		FString Description;

		TArray<TSharedRef<IAutomationLatentCommand>> BeforeEach;
		TArray<TSharedRef<FSpecIt>> It;
		TArray<TSharedRef<IAutomationLatentCommand>> AfterEach;

		TArray<TSharedRef<FSpecDefinitionScope>> Children;
	};

	struct FSpec
	{
		FString Id;
		FString Description;
		FString Filename;
		int32 LineNumber;
		TArray<TSharedRef<IAutomationLatentCommand>> Commands;
	};

	FTimespan DefaultTimeout;
	bool bEnableSkipIfError;
	TArray<FString> Description;
	TMap<FString, TSharedRef<FSpec>> IdToSpecMap;
	TSharedPtr<FSpecDefinitionScope> RootDefinitionScope;
	TArray<TSharedRef<FSpecDefinitionScope>> DefinitionScopeStack;
	bool bHasBeenDefined;
};
static_assert(sizeof(FAutomationSpecBase) == sizeof(FAutomationSpecBaseLayoutMock), "Layout mock has different size than the original. Maybe FAutomationSpecBase changed?");


bool FSpecBase::RunTest(const FString& InParameters)
{
	const bool bResult = FAutomationSpecBase::RunTest(InParameters);

	auto* ExposedThis = reinterpret_cast<FAutomationSpecBaseLayoutMock*>(this);
	TestsRemaining = ExposedThis->IdToSpecMap.Num();

	return bResult;
}

void FSpecBase::PreDefine()
{
	LatentBeforeEach(EAsyncExecution::ThreadPool, [this](const auto & Done)
	{
		PrepareTestWorld(FSpecBaseOnWorldReady::CreateLambda([this, &Done](UWorld * InWorld)
		{
			World = InWorld;
			Done.Execute();
		}));
	});
}

void FSpecBase::PostDefine()
{
	if (!bUsePIEWorld)
	{
		return;
	}

	AfterEach([this]()
	{
		ReleaseTestWorld();

		--TestsRemaining;

#if WITH_EDITOR
		// If this spec initialized a PIE world, tear it down
		if (bInitializedPIE && (TestsRemaining <= 0 || !bReusePIEWorldForAllTests))
		{
			FEditorPromotionTestUtilities::EndPIE();
			bInitializedPIE = false;
		}
#endif
	});

}

void FSpecBase::PrepareTestWorld(FSpecBaseOnWorldReady OnWorldReady)
{
	checkf(!IsInGameThread(), TEXT("PrepareTestWorld can only be done asynchronously. (LatentBeforeEach with ThreadPool or TaskGraph)"));

	UWorld* SelectedWorld = FindGameWorld();
#if WITH_EDITOR
	// If there was no PIE world, start it and try again
	if (bUsePIEWorld && !SelectedWorld && GIsEditor)
	{
		bPIEWorldIsReady = false;

		AsyncTask(ENamedThreads::GameThread, [this]()
		{
			PIEStartedHandle = FEditorDelegates::PostPIEStarted.AddLambda([this](const bool bIsSimulating)
			{
				// Notify the thread about the world being ready
				bPIEWorldIsReady = true;
			});
			FEditorPromotionTestUtilities::StartPIE(false);
		});

		// Wait while PIE initializes
		while (!bPIEWorldIsReady)
		{
			FPlatformProcess::Sleep(0.005f);
		}

		SelectedWorld = FindGameWorld();
		bInitializedPIE = SelectedWorld != nullptr;
	}
	bPIEWorldIsReady = true;
#endif

	if (!SelectedWorld)
	{
		SelectedWorld = GWorld;
#if WITH_EDITOR
		if (GIsEditor)
		{
			UE_LOG(LogTemp, Warning, TEXT("Test using GWorld. Not correct for PIE"));
		}
#endif
	}

	OnWorldReady.ExecuteIfBound(SelectedWorld);
}

void FSpecBase::ReleaseTestWorld()
{
#if WITH_EDITOR
	if (!IsInGameThread())
	{
		AsyncTask(ENamedThreads::GameThread, [this]()
		{
			ReleaseTestWorld();
		});
		return;
	}

	if (PIEStartedHandle.IsValid())
	{
		FEditorDelegates::BeginPIE.Remove(PIEStartedHandle);
	}
#endif
}

UWorld* FSpecBase::FindGameWorld()
{
	const TIndirectArray<FWorldContext>& WorldContexts = GEngine->GetWorldContexts();
	for (const FWorldContext& Context : WorldContexts)
	{
		if (Context.World() != nullptr)
		{
			if (Context.WorldType == EWorldType::PIE /*&& Context.PIEInstance == 0*/)
			{
				return Context.World();
			}

			if (Context.WorldType == EWorldType::Game)
			{
				return Context.World();
			}
		}
	}
	return nullptr;
}
