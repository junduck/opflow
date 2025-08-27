#include <cmath>
#include <gtest/gtest.h>
#include <tuple>

#include "opflow/fn/functor.hpp"

using namespace opflow::fn;

double multiply(double x, double y) { return x * y; }

struct stateful_multiple {
  double count;

  stateful_multiple(double init) : count(init) {}

  double operator()(double x, double y) {
    count += x * y;
    return count;
  }
};

class FunctorTest : public ::testing::Test {
protected:
  void SetUp() override {}
  void TearDown() override {}
};

// Test single input, single output
TEST_F(FunctorTest, SingleInputSingleOutput) {
  auto square_fn = [](double x) { return x * x; };
  functor<double, decltype(square_fn)> square_functor;

  EXPECT_EQ(square_functor.num_inputs(), 1);
  EXPECT_EQ(square_functor.num_outputs(), 1);

  double input = 3.0;
  double output = 0.0;

  square_functor.on_data(&input, &output);
  EXPECT_DOUBLE_EQ(output, 9.0);
}

// Test multiple inputs, single output
TEST_F(FunctorTest, MultipleInputsSingleOutput) {
  auto add_fn = [](double x, double y) { return x + y; };
  functor<double, decltype(add_fn)> add_functor;

  EXPECT_EQ(add_functor.num_inputs(), 2);
  EXPECT_EQ(add_functor.num_outputs(), 1);

  double inputs[] = {3.5, 2.5};
  double output = 0.0;

  add_functor.on_data(inputs, &output);
  EXPECT_DOUBLE_EQ(output, 6.0);
}

// Test three inputs, single output
TEST_F(FunctorTest, ThreeInputsSingleOutput) {
  auto sum3_fn = [](double x, double y, double z) { return x + y + z; };
  functor<double, decltype(sum3_fn)> sum3_functor;

  EXPECT_EQ(sum3_functor.num_inputs(), 3);
  EXPECT_EQ(sum3_functor.num_outputs(), 1);

  double inputs[] = {1.0, 2.0, 3.0};
  double output = 0.0;

  sum3_functor.on_data(inputs, &output);
  EXPECT_DOUBLE_EQ(output, 6.0);
}

// Test multiple inputs with mathematical operations
TEST_F(FunctorTest, MultipleInputsMathOperations) {
  auto distance_fn = [](double x, double y) { return std::sqrt(x * x + y * y); };
  functor<double, decltype(distance_fn)> distance_functor;

  EXPECT_EQ(distance_functor.num_inputs(), 2);
  EXPECT_EQ(distance_functor.num_outputs(), 1);

  double inputs[] = {3.0, 4.0};
  double output = 0.0;

  distance_functor.on_data(inputs, &output);
  EXPECT_DOUBLE_EQ(output, 5.0);
}

// Test single input, multiple outputs (tuple)
TEST_F(FunctorTest, SingleInputMultipleOutputs) {
  auto sin_cos_fn = [](double x) { return std::make_tuple(std::sin(x), std::cos(x)); };
  functor<double, decltype(sin_cos_fn)> sin_cos_functor;

  EXPECT_EQ(sin_cos_functor.num_inputs(), 1);
  EXPECT_EQ(sin_cos_functor.num_outputs(), 2);

  double input = 0.0;
  double outputs[2] = {0.0, 0.0};

  sin_cos_functor.on_data(&input, outputs);
  EXPECT_DOUBLE_EQ(outputs[0], std::sin(0.0)); // sin(0) = 0
  EXPECT_DOUBLE_EQ(outputs[1], std::cos(0.0)); // cos(0) = 1
}

// Test multiple inputs, multiple outputs (tuple)
TEST_F(FunctorTest, MultipleInputsMultipleOutputs) {
  auto polar_to_cartesian_fn = [](double r, double theta) {
    return std::make_tuple(r * std::cos(theta), r * std::sin(theta));
  };
  functor<double, decltype(polar_to_cartesian_fn)> polar_functor;
  opflow::fn_base<double> *base_ptr = &polar_functor;

  EXPECT_EQ(base_ptr->num_inputs(), 2);
  EXPECT_EQ(base_ptr->num_outputs(), 2);

  double inputs[] = {1.0, 0.0}; // r=1, theta=0
  double outputs[2] = {0.0, 0.0};

  base_ptr->on_data(inputs, outputs);
  EXPECT_DOUBLE_EQ(outputs[0], 1.0); // x = r*cos(0) = 1
  EXPECT_DOUBLE_EQ(outputs[1], 0.0); // y = r*sin(0) = 0
}

// Test three outputs tuple
TEST_F(FunctorTest, ThreeOutputsTuple) {
  auto xyz_fn = [](double t) { return std::make_tuple(t, t * t, t * t * t); };
  functor<double, decltype(xyz_fn)> xyz_functor;

  EXPECT_EQ(xyz_functor.num_inputs(), 1);
  EXPECT_EQ(xyz_functor.num_outputs(), 3);

  double input = 2.0;
  double outputs[3] = {0.0, 0.0, 0.0};

  xyz_functor.on_data(&input, outputs);
  EXPECT_DOUBLE_EQ(outputs[0], 2.0); // t
  EXPECT_DOUBLE_EQ(outputs[1], 4.0); // t²
  EXPECT_DOUBLE_EQ(outputs[2], 8.0); // t³
}

// Test multiple inputs, three outputs
TEST_F(FunctorTest, MultipleInputsThreeOutputs) {
  auto stats_fn = [](double x, double y, double z) {
    double sum = x + y + z;
    double mean = sum / 3.0;
    double variance = ((x - mean) * (x - mean) + (y - mean) * (y - mean) + (z - mean) * (z - mean)) / 3.0;
    return std::make_tuple(sum, mean, variance);
  };
  functor<double, decltype(stats_fn)> stats_functor;

  EXPECT_EQ(stats_functor.num_inputs(), 3);
  EXPECT_EQ(stats_functor.num_outputs(), 3);

  double inputs[] = {1.0, 2.0, 3.0};
  double outputs[3] = {0.0, 0.0, 0.0};

  stats_functor.on_data(inputs, outputs);
  EXPECT_DOUBLE_EQ(outputs[0], 6.0);         // sum
  EXPECT_DOUBLE_EQ(outputs[1], 2.0);         // mean
  EXPECT_NEAR(outputs[2], 2.0 / 3.0, 1e-10); // variance
}

// Helper function for function pointer test
double multiply_fn(double x, double y) { return x * y; }

// Test with function pointer
TEST_F(FunctorTest, FunctionPointer) {
  functor<double, double (*)(double, double)> multiply_functor(&multiply_fn);

  EXPECT_EQ(multiply_functor.num_inputs(), 2);
  EXPECT_EQ(multiply_functor.num_outputs(), 1);

  double inputs[] = {3.0, 4.0};
  double output = 0.0;

  multiply_functor.on_data(inputs, &output);
  EXPECT_DOUBLE_EQ(output, 12.0);
}

// Test edge case with zero input and output
TEST_F(FunctorTest, ConstantFunction) {
  auto constant_fn = []() { return 42.0; };
  functor<double, decltype(constant_fn)> constant_functor;

  EXPECT_EQ(constant_functor.num_inputs(), 0);
  EXPECT_EQ(constant_functor.num_outputs(), 1);

  double output = 0.0;
  constant_functor.on_data(nullptr, &output);
  EXPECT_DOUBLE_EQ(output, 42.0);
}

// Test with mixed tuple types (testing the template)
TEST_F(FunctorTest, MixedTupleTypes) {
  auto mixed_fn = [](double x) { return std::make_tuple(x, x + 1.0, x * 2.0, x / 2.0); };
  functor<double, decltype(mixed_fn)> mixed_functor;

  EXPECT_EQ(mixed_functor.num_inputs(), 1);
  EXPECT_EQ(mixed_functor.num_outputs(), 4);

  double input = 10.0;
  double outputs[4] = {0.0, 0.0, 0.0, 0.0};

  mixed_functor.on_data(&input, outputs);
  EXPECT_DOUBLE_EQ(outputs[0], 10.0);
  EXPECT_DOUBLE_EQ(outputs[1], 11.0);
  EXPECT_DOUBLE_EQ(outputs[2], 20.0);
  EXPECT_DOUBLE_EQ(outputs[3], 5.0);
}

// Test with stateful functor
TEST_F(FunctorTest, StatefulFunctor) {
  functor<double, stateful_multiple> stateful_functor(1);

  EXPECT_EQ(stateful_functor.num_inputs(), 2);
  EXPECT_EQ(stateful_functor.num_outputs(), 1);

  double inputs[] = {3.0, 4.0};
  double output = 0.0;

  stateful_functor.on_data(inputs, &output);
  EXPECT_DOUBLE_EQ(output, 13.0); // init 1 -> 1 + 12

  inputs[0] = 1.0;
  inputs[1] = 2.0;
  stateful_functor.on_data(inputs, &output);
  EXPECT_DOUBLE_EQ(output, 15.0); // 13 + 2
}
