#include <unity.h>
#include "control/control.h"

// Mock internal state for testing
void test_state_transitions() {
    // Initial state should be IDLE
    TEST_ASSERT_EQUAL(STATE_IDLE, control_get_state());

    // Transition to RUNNING
    control_transition_to(STATE_RUNNING);
    TEST_ASSERT_EQUAL(STATE_RUNNING, control_get_state());

    // Transition to ESTOP
    control_transition_to(STATE_ESTOP);
    TEST_ASSERT_EQUAL(STATE_ESTOP, control_get_state());

    // Should not be able to transition out of ESTOP without reset
    control_transition_to(STATE_IDLE);
    TEST_ASSERT_EQUAL(STATE_ESTOP, control_get_state());
}

void setup() {
    delay(2000); // Service delay
    UNITY_BEGIN();
    RUN_TEST(test_state_transitions);
    UNITY_END();
}

void loop() {}
