#include <unity.h>

enum TestState { TEST_IDLE = 0, TEST_RUNNING, TEST_PULSE, TEST_STEP, TEST_JOG, TEST_TIMER, TEST_STOPPING, TEST_ESTOP };

static TestState currentState = TEST_IDLE;

void test_state_transitions() {
    TEST_ASSERT_EQUAL(TEST_IDLE, currentState);

    currentState = TEST_RUNNING;
    TEST_ASSERT_EQUAL(TEST_RUNNING, currentState);

    currentState = TEST_ESTOP;
    TEST_ASSERT_EQUAL(TEST_ESTOP, currentState);

    TEST_ASSERT_EQUAL(TEST_IDLE, currentState);

    currentState = TEST_ESTOP;
    TEST_ASSERT_EQUAL(TEST_ESTOP, currentState);

    currentState = TEST_IDLE;
    TEST_ASSERT_EQUAL(TEST_ESTOP, currentState);
}

int main(int argc, char** argv) {
    UNITY_BEGIN();
    RUN_TEST(test_state_transitions);
    UNITY_END();
    return 0;
}
