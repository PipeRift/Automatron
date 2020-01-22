// Copyright 2020 Splash Damage, Ltd. - All Rights Reserved.

#include <CoreMinimal.h>
#include <AutomationTest.h>

#include "Automatron.h"


#if WITH_DEV_AUTOMATION_TESTS

SPEC(FAutomatronSpec, FSpecBase, "Automatron",
	EAutomationTestFlags::EngineFilter |
	EAutomationTestFlags::HighPriority |
	EAutomationTestFlags::EditorContext)
{
	It("Can run a test", [this]() {
		// Succeed
	});
}

#endif //WITH_DEV_AUTOMATION_TESTS
