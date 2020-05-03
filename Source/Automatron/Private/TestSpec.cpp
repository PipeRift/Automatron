// Copyright 2020 Splash Damage, Ltd. - All Rights Reserved.

#include "TestSpec.h"

#if WITH_EDITOR
#include <Tests/AutomationEditorPromotionCommon.h>
#include <Editor.h>
#endif


void FTestSpec::PreDefine()
{
	BeforeEach([this]()
	{
		CurrentContext = CurrentContext.NextContext();
	});

	if (!bUseWorld)
	{
		return;
	}

	LatentBeforeEach(EAsyncExecution::ThreadPool, [this](const auto & Done)
	{
		PrepareTestWorld(FSpecBaseOnWorldReady::CreateLambda([this, &Done](UWorld * InWorld)
		{
			World = InWorld;
			Done.Execute();
		}));
	});
}

void FTestSpec::PostDefine()
{
	AfterEach([this]()
	{
		if (!bUsePIEWorld)
		{
			return;
		}

		// If this spec initialized a PIE world, tear it down
		if (GetTestsRemaining() <= 0 || !bReusePIEWorldForAllTests)
		{
#if WITH_EDITOR
			if (bInitializedPIE)
			{
				FEditorPromotionTestUtilities::EndPIE();
				bInitializedPIE = false;
			}
			else
#endif
			{
				ReleaseTestWorld();
			}
		}
	});
}

void FTestSpec::PrepareTestWorld(FSpecBaseOnWorldReady OnWorldReady)
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

void FTestSpec::ReleaseTestWorld()
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

UWorld* FTestSpec::FindGameWorld()
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
