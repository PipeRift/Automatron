// Copyright 2020 Splash Damage, Ltd. - All Rights Reserved.

#include "Base/TestSpecBase.h"


bool FTestSpecBase::RunTest(const FString& InParameters)
{
	EnsureDefinitions();

	if (!InParameters.IsEmpty())
	{
		const TSharedRef<FSpec>* SpecToRun = IdToSpecMap.Find(InParameters);
		if (SpecToRun != nullptr)
		{
			for (int32 Index = 0; Index < (*SpecToRun)->Commands.Num(); ++Index)
			{
				FAutomationTestFramework::GetInstance().EnqueueLatentCommand((*SpecToRun)->Commands[Index]);
			}
		}
	}
	else
	{
		TArray<TSharedRef<FSpec>> Specs;
		IdToSpecMap.GenerateValueArray(Specs);

		for (int32 SpecIndex = 0; SpecIndex < Specs.Num(); SpecIndex++)
		{
			for (int32 CommandIndex = 0; CommandIndex < Specs[SpecIndex]->Commands.Num(); ++CommandIndex)
			{
				FAutomationTestFramework::GetInstance().EnqueueLatentCommand(Specs[SpecIndex]->Commands[CommandIndex]);
			}
		}
	}

	TestsRemaining = GetNumTests();
	return true;
}

FString FTestSpecBase::GetTestSourceFileName(const FString& InTestName) const
{
	FString TestId = InTestName;
	if (TestId.StartsWith(TestName + TEXT(" ")))
	{
		TestId = InTestName.RightChop(TestName.Len() + 1);
	}

	const TSharedRef<FSpec>* Spec = IdToSpecMap.Find(TestId);
	if (Spec != nullptr)
	{
		return (*Spec)->Filename;
	}

	return FAutomationTestBase::GetTestSourceFileName();
}

int32 FTestSpecBase::GetTestSourceFileLine(const FString& InTestName) const
{
	FString TestId = InTestName;
	if (TestId.StartsWith(TestName + TEXT(" ")))
	{
		TestId = InTestName.RightChop(TestName.Len() + 1);
	}

	const TSharedRef<FSpec>* Spec = IdToSpecMap.Find(TestId);
	if (Spec != nullptr)
	{
		return (*Spec)->LineNumber;
	}

	return FAutomationTestBase::GetTestSourceFileLine();
}

void FTestSpecBase::GetTests(TArray<FString>& OutBeautifiedNames, TArray<FString>& OutTestCommands) const
{
	EnsureDefinitions();

	TArray<TSharedRef<FSpec>> Specs;
	IdToSpecMap.GenerateValueArray(Specs);

	for (int32 Index = 0; Index < Specs.Num(); Index++)
	{
		OutTestCommands.Push(Specs[Index]->Id);
		OutBeautifiedNames.Push(Specs[Index]->Description);
	}
}

void FTestSpecBase::Describe(const FString& InDescription, TFunction<void()> DoWork)
{
	const TSharedRef<FSpecDefinitionScope> ParentScope = DefinitionScopeStack.Last();
	const TSharedRef<FSpecDefinitionScope> NewScope = MakeShared<FSpecDefinitionScope>();
	NewScope->Description = InDescription;
	ParentScope->Children.Push(NewScope);

	DefinitionScopeStack.Push(NewScope);
	PushDescription(InDescription);
	DoWork();
	PopDescription(InDescription);
	DefinitionScopeStack.Pop();

	if (NewScope->It.Num() == 0 && NewScope->Children.Num() == 0)
	{
		ParentScope->Children.Remove(NewScope);
	}
}

void FTestSpecBase::PostDefine()
{
	TArray<TSharedRef<FSpecDefinitionScope>> Stack;
	Stack.Push(RootDefinitionScope.ToSharedRef());

	TArray<TSharedRef<IAutomationLatentCommand>> BeforeEach;
	TArray<TSharedRef<IAutomationLatentCommand>> AfterEach;

	while (Stack.Num() > 0)
	{
		const TSharedRef<FSpecDefinitionScope> Scope = Stack.Last();

		BeforeEach.Append(Scope->BeforeEach);
		AfterEach.Append(Scope->AfterEach); // iterate in reverse

		for (int32 ItIndex = 0; ItIndex < Scope->It.Num(); ItIndex++)
		{
			TSharedRef<FSpecIt> It = Scope->It[ItIndex];

			TSharedRef<FSpec> Spec = MakeShareable(new FSpec());
			Spec->Id = It->Id;
			Spec->Description = It->Description;
			Spec->Filename = It->Filename;
			Spec->LineNumber = It->LineNumber;
			Spec->Commands.Append(BeforeEach);
			Spec->Commands.Add(It->Command);

			for (int32 AfterEachIndex = AfterEach.Num() - 1; AfterEachIndex >= 0; AfterEachIndex--)
			{
				Spec->Commands.Add(AfterEach[AfterEachIndex]);
			}

			check(!IdToSpecMap.Contains(Spec->Id));
			IdToSpecMap.Add(Spec->Id, Spec);
		}
		Scope->It.Empty();

		if (Scope->Children.Num() > 0)
		{
			Stack.Append(Scope->Children);
			Scope->Children.Empty();
		}
		else
		{
			while (Stack.Num() > 0 && Stack.Last()->Children.Num() == 0 && Stack.Last()->It.Num() == 0)
			{
				const TSharedRef<FSpecDefinitionScope> PoppedScope = Stack.Pop();

				if (PoppedScope->BeforeEach.Num() > 0)
				{
					BeforeEach.RemoveAt(BeforeEach.Num() - PoppedScope->BeforeEach.Num(), PoppedScope->BeforeEach.Num());
				}

				if (PoppedScope->AfterEach.Num() > 0)
				{
					AfterEach.RemoveAt(AfterEach.Num() - PoppedScope->AfterEach.Num(), PoppedScope->AfterEach.Num());
				}
			}
		}
	}

	RootDefinitionScope.Reset();
	DefinitionScopeStack.Reset();
	bHasBeenDefined = true;
}

void FTestSpecBase::Redefine()
{
	Description.Empty();
	IdToSpecMap.Empty();
	RootDefinitionScope.Reset();
	DefinitionScopeStack.Empty();
	bHasBeenDefined = false;
}