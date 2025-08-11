#include <gtest/gtest.h>

#include "opflow/transform.hpp"

using namespace opflow;
using namespace opflow::trans;

// Conversion functors
struct IdentityConv {
  template <typename T>
  T operator()(T v) const noexcept {
    return v;
  }
};

struct DoubleConv {
  template <typename T>
  T operator()(T v) const noexcept {
    return v * 2;
  }
};

TEST(LaggedDeltaT, SingleInputBasic) {
  with_time_delta<int, int, IdentityConv> op(1);
  int in;
  int out[2];

  in = 100;
  bool ready = op.on_data(10, &in);
  EXPECT_FALSE(ready);

  in = 101;
  ready = op.on_data(15, &in);
  ASSERT_TRUE(ready);
  int t = op.value(out);
  EXPECT_EQ(t, 15);
  EXPECT_EQ(out[0], 15 - 10); // dT
  EXPECT_EQ(out[1], 100);     // previous data

  in = 102;
  ready = op.on_data(25, &in);
  ASSERT_TRUE(ready);
  t = op.value(out);
  EXPECT_EQ(t, 25);
  EXPECT_EQ(out[0], 25 - 15); // dT
  EXPECT_EQ(out[1], 101);     // previous data (value from second tick)
}

TEST(LaggedDeltaT, MultipleInputsVector) {
  constexpr size_t N = 3;
  with_time_delta<int, int, IdentityConv> op(N);
  int in[N];
  int out[N + 1];

  int v1[N] = {1, 2, 3};
  std::copy(v1, v1 + N, in);
  bool ready = op.on_data(100, in);
  EXPECT_FALSE(ready);

  int v2[N] = {4, 5, 6};
  std::copy(v2, v2 + N, in);
  ready = op.on_data(130, in);
  ASSERT_TRUE(ready);
  int t = op.value(out);
  EXPECT_EQ(t, 130);
  EXPECT_EQ(out[0], 30); // dT = 130 - 100
  EXPECT_EQ(out[1], 1);
  EXPECT_EQ(out[2], 2);
  EXPECT_EQ(out[3], 3);

  int v3[N] = {7, 8, 9};
  std::copy(v3, v3 + N, in);
  ready = op.on_data(160, in);
  ASSERT_TRUE(ready);
  t = op.value(out);
  EXPECT_EQ(t, 160);
  EXPECT_EQ(out[0], 30); // dT = 160 - 130
  EXPECT_EQ(out[1], 4);
  EXPECT_EQ(out[2], 5);
  EXPECT_EQ(out[3], 6);
}

TEST(LaggedDeltaT, ResetBehavior) {
  with_time_delta<int, int, IdentityConv> op(1);
  int in;
  int out[2];

  in = 10;
  EXPECT_FALSE(op.on_data(5, &in));
  in = 11;
  EXPECT_TRUE(op.on_data(9, &in));
  op.value(out);
  EXPECT_EQ(out[0], 4); // dT
  EXPECT_EQ(out[1], 10);

  op.reset();
  in = 20;
  EXPECT_FALSE(op.on_data(30, &in)); // first after reset -> no output
  in = 21;
  EXPECT_TRUE(op.on_data(40, &in));
  int t = op.value(out);
  EXPECT_EQ(t, 40);
  EXPECT_EQ(out[0], 10); // 40 - 30
  EXPECT_EQ(out[1], 20);
}

TEST(LaggedDeltaT, TimeConversionApplied) {
  // With DoubleConv the stored / delta times are doubled, so delta should be 2 * (tn - t(n-1))
  with_time_delta<int, int, DoubleConv> op(1);
  int in;
  int out[2];

  in = 1;
  EXPECT_FALSE(op.on_data(10, &in));
  in = 2;
  EXPECT_TRUE(op.on_data(16, &in));
  op.value(out);
  EXPECT_EQ(out[0], (16 - 10) * 2); // converted delta
  EXPECT_EQ(out[1], 1);
}
