// Copyright 2020 Splash Damage, Ltd. - All Rights Reserved.

#pragma once

#include <CoreMinimal.h>
#include <Engine/Engine.h>
#include <AutomationTest.h>
#include <AutomationCommon.h>
#include <UnrealTypeTraits.h>


DECLARE_DELEGATE_OneParam(FSpecBaseOnWorldReady, UWorld*);

template<typename T>
struct TSpecRegister
{
    static TSpecRegister<T> Register;

    T Instance;

    TSpecRegister() : Instance{}
    {
        Instance.Setup();
    }
};

template<typename T>
TSpecRegister<T> TSpecRegister<T>::Register{};


class AUTOMATRON_API FSpecBase : public FAutomationSpecBase
{
public:

	bool bUseWorld = true;
	bool bUsePIEWorld = true;
	bool bReusePIEWorldForAllTests = true;

private:

	FString PrettyName;
	FString FileName;
	int32 LineNumber = -1;
	uint32 Flags = 0;
	int32 TestsRemaining = 0;

#if WITH_EDITOR
	bool bInitializedPIE = false;
	bool bPIEWorldIsReady = false;
	FDelegateHandle PIEStartedHandle;
#endif

	TWeakObjectPtr<UWorld> World;


public:

	FSpecBase() : FAutomationSpecBase("", false) {}
	virtual ~FSpecBase() {}

	virtual bool RunTest(const FString& InParameters) override;

	virtual FString GetTestSourceFileName() const override { return FileName; }
	virtual int32 GetTestSourceFileLine() const override { return LineNumber; }
	virtual uint32 GetTestFlags() const override { return Flags; }

protected:

	virtual FString GetBeautifiedTestName() const override { return PrettyName; }

	template<uint32 TFlags>
	void Setup(FString&& InName, FString&& InPrettyName, FString&& InFileName, int32 InLineNumber);

	// Used to indicate a test is pending to be implemented.
	void TestNotImplemented()
	{
		AddWarning(TEXT("Test not implemented"), 1);
	}

	// Runs before
	void PreDefine();
	void PostDefine();

	void PrepareTestWorld(FSpecBaseOnWorldReady OnWorldReady);
	void ReleaseTestWorld();

	UWorld* GetWorld() const { return World.Get(); }

private:

	void Reregister(const FString& NewName)
	{
		FAutomationTestFramework::Get().UnregisterAutomationTest(TestName);
		TestName = NewName;
		FAutomationTestFramework::Get().RegisterAutomationTest(TestName, this);
	}

	// Finds the first available game world (Standalone or PIE)
	static UWorld* FindGameWorld();
};


template<uint32 TFlags>
inline void FSpecBase::Setup(FString&& InName, FString&& InPrettyName, FString&& InFileName, int32 InLineNumber)
{
	static_assert(TFlags & EAutomationTestFlags::ApplicationContextMask, "AutomationTest has no application flag. It shouldn't run. See AutomationTest.h."); \
	static_assert(((TFlags & EAutomationTestFlags::FilterMask) == EAutomationTestFlags::SmokeFilter) ||
		((TFlags & EAutomationTestFlags::FilterMask) == EAutomationTestFlags::EngineFilter) ||
		((TFlags & EAutomationTestFlags::FilterMask) == EAutomationTestFlags::ProductFilter) ||
		((TFlags & EAutomationTestFlags::FilterMask) == EAutomationTestFlags::PerfFilter) ||
		((TFlags & EAutomationTestFlags::FilterMask) == EAutomationTestFlags::StressFilter) ||
		((TFlags & EAutomationTestFlags::FilterMask) == EAutomationTestFlags::NegativeFilter),
		"All AutomationTests must have exactly 1 filter type specified.  See AutomationTest.h.");

	PrettyName = MoveTemp(InPrettyName);
	FileName = MoveTemp(InFileName);
	LineNumber = InLineNumber;
	Flags = TFlags;

	Reregister(InName);
}