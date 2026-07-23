#include <unity.h>

void test_scaffold_smoke(void) {
  TEST_ASSERT_EQUAL_INT(1, 1);
}

void setUp(void) {}
void tearDown(void) {}

int main(int argc, char **argv) {
  UNITY_BEGIN();
  RUN_TEST(test_scaffold_smoke);
  return UNITY_END();
}
