// Copyright 2020 Splash Damage, Ltd. - All Rights Reserved.

#include <CoreMinimal.h>
#include <Misc/AutomationTest.h>


class AUTOMATRON_API FTestSpecBase
	: public FAutomationTestBase
	, public TSharedFromThis<FTestSpecBase>
{
private:

	class FSingleExecuteLatentCommand : public IAutomationLatentCommand
	{
	public:
		FSingleExecuteLatentCommand(const FTestSpecBase* const InSpec, TFunction<void()> InPredicate, bool bInSkipIfErrored = false)
			: Spec(InSpec)
			, Predicate(MoveTemp(InPredicate))
			, bSkipIfErrored(bInSkipIfErrored)
		{ }

		virtual ~FSingleExecuteLatentCommand()
		{ }

		virtual bool Update() override
		{
			if (bSkipIfErrored && Spec->HasAnyErrors())
			{
				return true;
			}

			Predicate();
			return true;
		}

	private:

		const FTestSpecBase* const Spec;
		const TFunction<void()> Predicate;
		const bool bSkipIfErrored;
	};

	class FUntilDoneLatentCommand : public IAutomationLatentCommand
	{
	public:

		FUntilDoneLatentCommand(FTestSpecBase* const InSpec, TFunction<void(const FDoneDelegate&)> InPredicate, const FTimespan& InTimeout, bool bInSkipIfErrored = false)
			: Spec(InSpec)
			, Predicate(MoveTemp(InPredicate))
			, Timeout(InTimeout)
			, bSkipIfErrored(bInSkipIfErrored)
			, bIsRunning(false)
			, bDone(false)
		{ }

		virtual ~FUntilDoneLatentCommand()
		{ }

		virtual bool Update() override
		{
			if (!bIsRunning)
			{
				if (bSkipIfErrored && Spec->HasAnyErrors())
				{
					return true;
				}

				Predicate(FDoneDelegate::CreateSP(this, &FUntilDoneLatentCommand::Done));
				bIsRunning = true;
				StartedRunning = FDateTime::UtcNow();
			}

			if (bDone)
			{
				Reset();
				return true;
			}
			else if (FDateTime::UtcNow() >= StartedRunning + Timeout)
			{
				Reset();
				Spec->AddError(TEXT("Latent command timed out."), 0);
				return true;
			}

			return false;
		}

	private:

		void Done()
		{
			bDone = true;
		}

		void Reset()
		{
			// Reset the done for the next potential run of this command
			bDone = false;
			bIsRunning = false;
		}

	private:

		FTestSpecBase* const Spec;
		const TFunction<void(const FDoneDelegate&)> Predicate;
		const FTimespan Timeout;
		const bool bSkipIfErrored;

		bool bIsRunning;
		FDateTime StartedRunning;
		FThreadSafeBool bDone;
	};

	class FAsyncUntilDoneLatentCommand : public IAutomationLatentCommand
	{
	public:

		FAsyncUntilDoneLatentCommand(FTestSpecBase* const InSpec, EAsyncExecution InExecution, TFunction<void(const FDoneDelegate&)> InPredicate, const FTimespan& InTimeout, bool bInSkipIfErrored = false)
			: Spec(InSpec)
			, Execution(InExecution)
			, Predicate(MoveTemp(InPredicate))
			, Timeout(InTimeout)
			, bSkipIfErrored(bInSkipIfErrored)
			, bDone(false)
		{ }

		virtual ~FAsyncUntilDoneLatentCommand()
		{ }

		virtual bool Update() override
		{
			if (!Future.IsValid())
			{
				if (bSkipIfErrored && Spec->HasAnyErrors())
				{
					return true;
				}

				Future = Async(Execution, [this]() {
					Predicate(FDoneDelegate::CreateRaw(this, &FAsyncUntilDoneLatentCommand::Done));
				});

				StartedRunning = FDateTime::UtcNow();
			}

			if (bDone)
			{
				Reset();
				return true;
			}
			else if (FDateTime::UtcNow() >= StartedRunning + Timeout)
			{
				Reset();
				Spec->AddError(TEXT("Latent command timed out."), 0);
				return true;
			}

			return false;
		}

	private:

		void Done()
		{
			bDone = true;
		}

		void Reset()
		{
			// Reset the done for the next potential run of this command
			bDone = false;
			Future = TFuture<void>();
		}

	private:

		FTestSpecBase* const Spec;
		const EAsyncExecution Execution;
		const TFunction<void(const FDoneDelegate&)> Predicate;
		const FTimespan Timeout;
		const bool bSkipIfErrored;

		FThreadSafeBool bDone;
		FDateTime StartedRunning;
		TFuture<void> Future;
	};

	class FAsyncLatentCommand : public IAutomationLatentCommand
	{
	public:

		FAsyncLatentCommand(FTestSpecBase* const InSpec, EAsyncExecution InExecution, TFunction<void()> InPredicate, const FTimespan& InTimeout, bool bInSkipIfErrored = false)
			: Spec(InSpec)
			, Execution(InExecution)
			, Predicate(MoveTemp(InPredicate))
			, Timeout(InTimeout)
			, bSkipIfErrored(bInSkipIfErrored)
			, bDone(false)
		{ }

		virtual ~FAsyncLatentCommand()
		{ }

		virtual bool Update() override
		{
			if (!Future.IsValid())
			{
				if (bSkipIfErrored && Spec->HasAnyErrors())
				{
					return true;
				}

				Future = Async(Execution, [this]() {
					Predicate();
					bDone = true;
				});

				StartedRunning = FDateTime::UtcNow();
			}

			if (bDone)
			{
				--Spec->TestsRemaining;
				Reset();
				return true;
			}
			else if (FDateTime::UtcNow() >= StartedRunning + Timeout)
			{
				Reset();
				Spec->AddError(TEXT("Latent command timed out."), 0);
				return true;
			}

			return false;
		}

	private:

		void Done()
		{
			bDone = true;
		}

		void Reset()
		{
			// Reset the done for the next potential run of this command
			bDone = false;
			Future = TFuture<void>();
		}

	private:

		FTestSpecBase* const Spec;
		const EAsyncExecution Execution;
		const TFunction<void()> Predicate;
		const FTimespan Timeout;
		const bool bSkipIfErrored;

		FThreadSafeBool bDone;
		FDateTime StartedRunning;
		TFuture<void> Future;
	};

	struct FSpecIt
	{
		FString Description;
		FString Id;
		FString Filename;
		int32 LineNumber;
		TSharedRef<IAutomationLatentCommand> Command;

		FSpecIt(FString InDescription, FString InId, FString InFilename, int32 InLineNumber, TSharedRef<IAutomationLatentCommand> InCommand)
			: Description(MoveTemp(InDescription))
			, Id(MoveTemp(InId))
			, Filename(InFilename)
			, LineNumber(MoveTemp(InLineNumber))
			, Command(MoveTemp(InCommand))
		{ }
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


protected:

	/* The timespan for how long a block should be allowed to execute before giving up and failing the test */
	FTimespan DefaultTimeout = FTimespan::FromSeconds(30);

	/* Whether or not BeforeEach and It blocks should skip execution if the test has already failed */
	bool bEnableSkipIfError = true;

private:

	int32 TestsRemaining = 0;

	TArray<FString> Description;

	TMap<FString, TSharedRef<FSpec>> IdToSpecMap;

	TSharedPtr<FSpecDefinitionScope> RootDefinitionScope;

	TArray<TSharedRef<FSpecDefinitionScope>> DefinitionScopeStack;

	bool bHasBeenDefined = false;


public:

	FTestSpecBase(const FString& InName, const bool bInComplexTask)
		: FAutomationTestBase(InName, bInComplexTask)
		, RootDefinitionScope(MakeShared<FSpecDefinitionScope>())
	{
		DefinitionScopeStack.Push(RootDefinitionScope.ToSharedRef());
	}

	virtual ~FTestSpecBase() {}

	virtual bool RunTest(const FString& InParameters) override;

	virtual bool IsStressTest() const { return false; }
	virtual uint32 GetRequiredDeviceNum() const override { return 1; }

	virtual FString GetTestSourceFileName(const FString& InTestName) const override;

	virtual int32 GetTestSourceFileLine(const FString& InTestName) const override;

	virtual void GetTests(TArray<FString>& OutBeautifiedNames, TArray<FString>& OutTestCommands) const override;


	// BEGIN Disabled Scopes
	void xDescribe(const FString& InDescription, TFunction<void()> DoWork) {}

	void xIt(const FString& InDescription, TFunction<void()> DoWork) {}
	void xIt(const FString& InDescription, EAsyncExecution Execution, TFunction<void()> DoWork) {}
	void xIt(const FString& InDescription, EAsyncExecution Execution, const FTimespan& Timeout, TFunction<void()> DoWork) {}

	void xLatentIt(const FString& InDescription, TFunction<void(const FDoneDelegate&)> DoWork) {}
	void xLatentIt(const FString& InDescription, const FTimespan& Timeout, TFunction<void(const FDoneDelegate&)> DoWork) {}
	void xLatentIt(const FString& InDescription, EAsyncExecution Execution, TFunction<void(const FDoneDelegate&)> DoWork) {}
	void xLatentIt(const FString& InDescription, EAsyncExecution Execution, const FTimespan& Timeout, TFunction<void(const FDoneDelegate&)> DoWork) {}

	void xBeforeEach(TFunction<void()> DoWork) {}
	void xBeforeEach(EAsyncExecution Execution, TFunction<void()> DoWork) {}
	void xBeforeEach(EAsyncExecution Execution, const FTimespan& Timeout, TFunction<void()> DoWork) {}

	void xLatentBeforeEach(TFunction<void(const FDoneDelegate&)> DoWork) {}
	void xLatentBeforeEach(const FTimespan& Timeout, TFunction<void(const FDoneDelegate&)> DoWork) {}
	void xLatentBeforeEach(EAsyncExecution Execution, TFunction<void(const FDoneDelegate&)> DoWork) {}
	void xLatentBeforeEach(EAsyncExecution Execution, const FTimespan& Timeout, TFunction<void(const FDoneDelegate&)> DoWork) {}

	void xAfterEach(TFunction<void()> DoWork) {}
	void xAfterEach(EAsyncExecution Execution, TFunction<void()> DoWork) {}
	void xAfterEach(EAsyncExecution Execution, const FTimespan& Timeout, TFunction<void()> DoWork) {}

	void xLatentAfterEach(TFunction<void(const FDoneDelegate&)> DoWork) {}
	void xLatentAfterEach(const FTimespan& Timeout, TFunction<void(const FDoneDelegate&)> DoWork) {}
	void xLatentAfterEach(EAsyncExecution Execution, TFunction<void(const FDoneDelegate&)> DoWork) {}
	void xLatentAfterEach(EAsyncExecution Execution, const FTimespan& Timeout, TFunction<void(const FDoneDelegate&)> DoWork) {}
	// END Disabled Scopes


	// BEGIN Enabled Scopes
	void Describe(const FString& InDescription, TFunction<void()> DoWork);

	void It(const FString& InDescription, TFunction<void()> DoWork)
	{
		const TSharedRef<FSpecDefinitionScope> CurrentScope = DefinitionScopeStack.Last();
		const TArray<FProgramCounterSymbolInfo> Stack = FPlatformStackWalk::GetStack(1, 1);

		PushDescription(InDescription);
		CurrentScope->It.Push(MakeShared<FSpecIt>(GetDescription(), GetId(), Stack[0].Filename, Stack[0].LineNumber, MakeShared<FSingleExecuteLatentCommand>(this, DoWork, bEnableSkipIfError)));
		PopDescription(InDescription);
	}

	void It(const FString& InDescription, EAsyncExecution Execution, TFunction<void()> DoWork)
	{
		const TSharedRef<FSpecDefinitionScope> CurrentScope = DefinitionScopeStack.Last();
		const TArray<FProgramCounterSymbolInfo> Stack = FPlatformStackWalk::GetStack(1, 1);

		PushDescription(InDescription);
		CurrentScope->It.Push(MakeShared<FSpecIt>(GetDescription(), GetId(), Stack[0].Filename, Stack[0].LineNumber, MakeShared<FAsyncLatentCommand>(this, Execution, DoWork, DefaultTimeout, bEnableSkipIfError)));
		PopDescription(InDescription);
	}

	void It(const FString& InDescription, EAsyncExecution Execution, const FTimespan& Timeout, TFunction<void()> DoWork)
	{
		const TSharedRef<FSpecDefinitionScope> CurrentScope = DefinitionScopeStack.Last();
		const TArray<FProgramCounterSymbolInfo> Stack = FPlatformStackWalk::GetStack(1, 1);

		PushDescription(InDescription);
		CurrentScope->It.Push(MakeShared<FSpecIt>(GetDescription(), GetId(), Stack[0].Filename, Stack[0].LineNumber, MakeShared<FAsyncLatentCommand>(this, Execution, DoWork, Timeout, bEnableSkipIfError)));
		PopDescription(InDescription);
	}

	void LatentIt(const FString& InDescription, TFunction<void(const FDoneDelegate&)> DoWork)
	{
		const TSharedRef<FSpecDefinitionScope> CurrentScope = DefinitionScopeStack.Last();
		const TArray<FProgramCounterSymbolInfo> Stack = FPlatformStackWalk::GetStack(1, 1);

		PushDescription(InDescription);
		CurrentScope->It.Push(MakeShared<FSpecIt>(GetDescription(), GetId(), Stack[0].Filename, Stack[0].LineNumber, MakeShared<FUntilDoneLatentCommand>(this, DoWork, DefaultTimeout, bEnableSkipIfError)));
		PopDescription(InDescription);
	}

	void LatentIt(const FString& InDescription, const FTimespan& Timeout, TFunction<void(const FDoneDelegate&)> DoWork)
	{
		const TSharedRef<FSpecDefinitionScope> CurrentScope = DefinitionScopeStack.Last();
		const TArray<FProgramCounterSymbolInfo> Stack = FPlatformStackWalk::GetStack(1, 1);

		PushDescription(InDescription);
		CurrentScope->It.Push(MakeShared<FSpecIt>(GetDescription(), GetId(), Stack[0].Filename, Stack[0].LineNumber, MakeShared<FUntilDoneLatentCommand>(this, DoWork, Timeout, bEnableSkipIfError)));
		PopDescription(InDescription);
	}

	void LatentIt(const FString& InDescription, EAsyncExecution Execution, TFunction<void(const FDoneDelegate&)> DoWork)
	{
		const TSharedRef<FSpecDefinitionScope> CurrentScope = DefinitionScopeStack.Last();
		const TArray<FProgramCounterSymbolInfo> Stack = FPlatformStackWalk::GetStack(1, 1);

		PushDescription(InDescription);
		CurrentScope->It.Push(MakeShared<FSpecIt>(GetDescription(), GetId(), Stack[0].Filename, Stack[0].LineNumber, MakeShared<FAsyncUntilDoneLatentCommand>(this, Execution, DoWork, DefaultTimeout, bEnableSkipIfError)));
		PopDescription(InDescription);
	}

	void LatentIt(const FString& InDescription, EAsyncExecution Execution, const FTimespan& Timeout, TFunction<void(const FDoneDelegate&)> DoWork)
	{
		const TSharedRef<FSpecDefinitionScope> CurrentScope = DefinitionScopeStack.Last();
		const TArray<FProgramCounterSymbolInfo> Stack = FPlatformStackWalk::GetStack(1, 1);

		PushDescription(InDescription);
		CurrentScope->It.Push(MakeShared<FSpecIt>(GetDescription(), GetId(), Stack[0].Filename, Stack[0].LineNumber, MakeShared<FAsyncUntilDoneLatentCommand>(this, Execution, DoWork, Timeout, bEnableSkipIfError)));
		PopDescription(InDescription);
	}

	void BeforeEach(TFunction<void()> DoWork)
	{
		const TSharedRef<FSpecDefinitionScope> CurrentScope = DefinitionScopeStack.Last();
		CurrentScope->BeforeEach.Push(MakeShareable(new FSingleExecuteLatentCommand(this, DoWork, bEnableSkipIfError)));
	}

	void BeforeEach(EAsyncExecution Execution, TFunction<void()> DoWork)
	{
		const TSharedRef<FSpecDefinitionScope> CurrentScope = DefinitionScopeStack.Last();
		CurrentScope->BeforeEach.Push(MakeShared<FAsyncLatentCommand>(this, Execution, DoWork, DefaultTimeout, bEnableSkipIfError));
	}

	void BeforeEach(EAsyncExecution Execution, const FTimespan& Timeout, TFunction<void()> DoWork)
	{
		const TSharedRef<FSpecDefinitionScope> CurrentScope = DefinitionScopeStack.Last();
		CurrentScope->BeforeEach.Push(MakeShared<FAsyncLatentCommand>(this, Execution, DoWork, Timeout, bEnableSkipIfError));
	}

	void LatentBeforeEach(TFunction<void(const FDoneDelegate&)> DoWork)
	{
		const TSharedRef<FSpecDefinitionScope> CurrentScope = DefinitionScopeStack.Last();
		CurrentScope->BeforeEach.Push(MakeShared<FUntilDoneLatentCommand>(this, DoWork, DefaultTimeout, bEnableSkipIfError));
	}

	void LatentBeforeEach(const FTimespan& Timeout, TFunction<void(const FDoneDelegate&)> DoWork)
	{
		const TSharedRef<FSpecDefinitionScope> CurrentScope = DefinitionScopeStack.Last();
		CurrentScope->BeforeEach.Push(MakeShared<FUntilDoneLatentCommand>(this, DoWork, Timeout, bEnableSkipIfError));
	}

	void LatentBeforeEach(EAsyncExecution Execution, TFunction<void(const FDoneDelegate&)> DoWork)
	{
		const TSharedRef<FSpecDefinitionScope> CurrentScope = DefinitionScopeStack.Last();
		CurrentScope->BeforeEach.Push(MakeShared<FAsyncUntilDoneLatentCommand>(this, Execution, DoWork, DefaultTimeout, bEnableSkipIfError));
	}

	void LatentBeforeEach(EAsyncExecution Execution, const FTimespan& Timeout, TFunction<void(const FDoneDelegate&)> DoWork)
	{
		const TSharedRef<FSpecDefinitionScope> CurrentScope = DefinitionScopeStack.Last();
		CurrentScope->BeforeEach.Push(MakeShared<FAsyncUntilDoneLatentCommand>(this, Execution, DoWork, Timeout, bEnableSkipIfError));
	}

	void AfterEach(TFunction<void()> DoWork)
	{
		const TSharedRef<FSpecDefinitionScope> CurrentScope = DefinitionScopeStack.Last();
		CurrentScope->AfterEach.Push(MakeShareable(new FSingleExecuteLatentCommand(this, DoWork)));
	}

	void AfterEach(EAsyncExecution Execution, TFunction<void()> DoWork)
	{
		const TSharedRef<FSpecDefinitionScope> CurrentScope = DefinitionScopeStack.Last();
		CurrentScope->AfterEach.Push(MakeShared<FAsyncLatentCommand>(this, Execution, DoWork, DefaultTimeout));
	}

	void AfterEach(EAsyncExecution Execution, const FTimespan& Timeout, TFunction<void()> DoWork)
	{
		const TSharedRef<FSpecDefinitionScope> CurrentScope = DefinitionScopeStack.Last();
		CurrentScope->AfterEach.Push(MakeShared<FAsyncLatentCommand>(this, Execution, DoWork, Timeout));
	}

	void LatentAfterEach(TFunction<void(const FDoneDelegate&)> DoWork)
	{
		const TSharedRef<FSpecDefinitionScope> CurrentScope = DefinitionScopeStack.Last();
		CurrentScope->AfterEach.Push(MakeShared<FUntilDoneLatentCommand>(this, DoWork, DefaultTimeout));
	}

	void LatentAfterEach(const FTimespan& Timeout, TFunction<void(const FDoneDelegate&)> DoWork)
	{
		const TSharedRef<FSpecDefinitionScope> CurrentScope = DefinitionScopeStack.Last();
		CurrentScope->AfterEach.Push(MakeShared<FUntilDoneLatentCommand>(this, DoWork, Timeout));
	}

	void LatentAfterEach(EAsyncExecution Execution, TFunction<void(const FDoneDelegate&)> DoWork)
	{
		const TSharedRef<FSpecDefinitionScope> CurrentScope = DefinitionScopeStack.Last();
		CurrentScope->AfterEach.Push(MakeShared<FAsyncUntilDoneLatentCommand>(this, Execution, DoWork, DefaultTimeout));
	}

	void LatentAfterEach(EAsyncExecution Execution, const FTimespan& Timeout, TFunction<void(const FDoneDelegate&)> DoWork)
	{
		const TSharedRef<FSpecDefinitionScope> CurrentScope = DefinitionScopeStack.Last();
		CurrentScope->AfterEach.Push(MakeShared<FAsyncUntilDoneLatentCommand>(this, Execution, DoWork, Timeout));
	}

	int32 GetNumTests() const { return IdToSpecMap.Num(); }
	int32 GetTestsRemaining() const { return TestsRemaining; }

protected:

	void EnsureDefinitions() const;

	virtual void Define() = 0;

	void PostDefine();

	void Redefine();

private:

	void PushDescription(const FString& InDescription)
	{
		Description.Add(InDescription);
	}

	void PopDescription(const FString& InDescription)
	{
		Description.RemoveAt(Description.Num() - 1);
	}

	FString GetDescription() const
	{
		FString CompleteDescription;
		for (int32 Index = 0; Index < Description.Num(); ++Index)
		{
			if (Description[Index].IsEmpty())
			{
				continue;
			}

			if (CompleteDescription.IsEmpty())
			{
				CompleteDescription = Description[Index];
			}
			else if (FChar::IsWhitespace(CompleteDescription[CompleteDescription.Len() - 1]) || FChar::IsWhitespace(Description[Index][0]))
			{
				CompleteDescription = CompleteDescription + TEXT(".") + Description[Index];
			}
			else
			{
				CompleteDescription = FString::Printf(TEXT("%s.%s"), *CompleteDescription, *Description[Index]);
			}
		}

		return CompleteDescription;
	}

	FString GetId() const
	{
		if (Description.Last().EndsWith(TEXT("]")))
		{
			FString ItDescription = Description.Last();
			ItDescription.RemoveAt(ItDescription.Len() - 1);

			int32 StartingBraceIndex = INDEX_NONE;
			if (ItDescription.FindLastChar(TEXT('['), StartingBraceIndex) && StartingBraceIndex != ItDescription.Len() - 1)
			{
				FString CommandId = ItDescription.RightChop(StartingBraceIndex + 1);
				return CommandId;
			}
		}

		FString CompleteId;
		for (int32 Index = 0; Index < Description.Num(); ++Index)
		{
			if (Description[Index].IsEmpty())
			{
				continue;
			}

			if (CompleteId.IsEmpty())
			{
				CompleteId = Description[Index];
			}
			else if (FChar::IsWhitespace(CompleteId[CompleteId.Len() - 1]) || FChar::IsWhitespace(Description[Index][0]))
			{
				CompleteId = CompleteId + Description[Index];
			}
			else
			{
				CompleteId = FString::Printf(TEXT("%s %s"), *CompleteId, *Description[Index]);
			}
		}

		return CompleteId;
	}
};

inline void FTestSpecBase::EnsureDefinitions() const
{
	if (!bHasBeenDefined)
	{
		const_cast<FTestSpecBase*>(this)->Define();
		const_cast<FTestSpecBase*>(this)->PostDefine();
	}
}
