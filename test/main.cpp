#include "gtest/gtest.h"

// This is just a dummy to keep clangd happy

int main(int argc, char *argv[]) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
