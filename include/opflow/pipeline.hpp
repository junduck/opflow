#pragma once

#include <memory>
#include <variant>
#include <vector>

#include "def.hpp"
#include "fn_exec.hpp"
#include "op_exec.hpp"
#include "tumble_exec.hpp"

#include "detail/utils.hpp"
#include "detail/vector_store.hpp"

namespace opflow {

/**
 * @brief Pipeline for chaining op_exec, fn_exec, and tumble_exec
 *
 * Allows data to flow through multiple execution stages sequentially.
 * Each stage can be an operator (op_exec), function (fn_exec), or
 * tumbling window aggregator (tumble_exec).
 */
template <typename T, typename Alloc = std::allocator<T>>
class pipeline {
public:
  using data_type = T;
  using op_exec_type = op_exec<T, Alloc>;
  using fn_exec_type = fn_exec<T, Alloc>;
  using tumble_exec_type = tumble_exec<T, Alloc>;
  using stage_type = std::variant<op_exec_type *, fn_exec_type *, tumble_exec_type *>;
  using buffer_type = detail::vector_store<data_type, Alloc>;
  using buffer_alloc = detail::rebind_alloc<Alloc, buffer_type>;

  explicit pipeline(size_t num_groups, Alloc const &alloc = Alloc{})
      : ngrp(num_groups), alloc(alloc), stages(), buffers(alloc), stage_outputs() {}

  /**
   * @brief Add an op_exec stage to the pipeline
   * @param exec Pointer to op_exec instance (must outlive pipeline)
   * @return Reference to this pipeline for chaining
   */
  pipeline &add_stage(op_exec_type *exec) {
    validate_stage_connection(exec);
    stages.push_back(exec);
    stage_outputs.push_back(exec->num_outputs());
    allocate_buffer_if_needed();
    return *this;
  }

  /**
   * @brief Add a fn_exec stage to the pipeline
   * @param exec Pointer to fn_exec instance (must outlive pipeline)
   * @return Reference to this pipeline for chaining
   */
  pipeline &add_stage(fn_exec_type *exec) {
    validate_stage_connection(exec);
    stages.push_back(exec);
    stage_outputs.push_back(exec->num_outputs());
    allocate_buffer_if_needed();
    return *this;
  }

  /**
   * @brief Add a tumble_exec stage to the pipeline
   * @param exec Pointer to tumble_exec instance (must outlive pipeline)
   * @return Reference to this pipeline for chaining
   */
  pipeline &add_stage(tumble_exec_type *exec) {
    validate_stage_connection(exec);
    stages.push_back(exec);
    stage_outputs.push_back(exec->num_outputs());
    allocate_buffer_if_needed();
    return *this;
  }

  /**
   * @brief Process data through all pipeline stages
   * @param timestamp Timestamp of the data
   * @param in Input data array
   * @param out Output data array (populated if pipeline emits)
   * @param igrp Group index
   * @return Optional timestamp if pipeline emits, nullopt otherwise
   *
   * Note: If any tumble_exec stage doesn't emit, the entire pipeline
   * doesn't emit and returns nullopt.
   */
  std::optional<data_type> on_data(data_type timestamp,            //
                                   data_type const *in,            //
                                   data_type *OPFLOW_RESTRICT out, //
                                   size_t igrp) {
    if (stages.empty()) {
      return std::nullopt;
    }

    data_type const *current_in = in;
    data_type *current_out = nullptr;
    std::optional<data_type> result_timestamp = timestamp;

    for (size_t i = 0; i < stages.size(); ++i) {
      // Determine output buffer for this stage
      if (i == stages.size() - 1) {
        // Last stage writes to final output
        current_out = out;
      } else {
        current_out = get_buffer(i, igrp);
      }

      // Execute stage
      auto emit_result = std::visit(
          [&](auto *exec) -> std::optional<data_type> {
            using ExecType = std::decay_t<decltype(*exec)>;

            if constexpr (std::is_same_v<ExecType, op_exec_type>) {
              // op_exec: on_data + value
              exec->on_data(timestamp, current_in, igrp);
              exec->value(current_out, igrp);
              return timestamp; // op_exec always emits
            } else if constexpr (std::is_same_v<ExecType, fn_exec_type>) {
              // fn_exec: direct on_data
              exec->on_data(current_in, current_out, igrp);
              return timestamp; // fn_exec always emits
            } else if constexpr (std::is_same_v<ExecType, tumble_exec_type>) {
              // tumble_exec: returns optional timestamp
              return exec->on_data(timestamp, current_in, current_out, igrp);
            }
          },
          stages[i]);

      // If this stage didn't emit, stop processing
      if (!emit_result.has_value()) {
        return std::nullopt;
      }

      result_timestamp = emit_result;

      // Setup input for next stage
      if (i < stages.size() - 1) {
        current_in = get_buffer(i, igrp);
      }
    }

    return result_timestamp;
  }

  /**
   * @brief Update parameters for a specific stage
   * @param stage_idx Index of the stage to update
   * @param in Parameter input data
   * @param igrp Group index
   */
  void on_param(size_t stage_idx, data_type const *in, size_t igrp) {
    if (stage_idx >= stages.size()) {
      throw std::out_of_range("Stage index out of range");
    }

    std::visit(
        [&](auto *exec) {
          using ExecType = std::decay_t<decltype(*exec)>;
          if constexpr (std::is_same_v<ExecType, tumble_exec_type>) {
            exec->op_param(in, igrp);
          } else {
            exec->on_param(in, igrp);
          }
        },
        stages[stage_idx]);
  }

  /**
   * @brief Get the number of input features for the pipeline
   */
  size_t num_inputs() const noexcept {
    if (stages.empty())
      return 0;
    return std::visit([](auto *exec) { return exec->num_inputs(); }, stages[0]);
  }

  /**
   * @brief Get the number of output features from the pipeline
   */
  size_t num_outputs() const noexcept {
    if (stages.empty())
      return 0;
    return std::visit([](auto *exec) { return exec->num_outputs(); }, stages.back());
  }

  /**
   * @brief Get the number of groups
   */
  size_t num_groups() const noexcept { return ngrp; }

  /**
   * @brief Get the number of stages in the pipeline
   */
  size_t num_stages() const noexcept { return stages.size(); }

private:
  template <typename ExecType>
  void validate_stage_connection(ExecType *exec) {
    if (exec->num_groups() != ngrp) {
      throw std::runtime_error("Stage num_groups must match pipeline num_groups");
    }

    if (!stages.empty()) {
      size_t prev_outputs = stage_outputs.back();
      size_t curr_inputs = exec->num_inputs();
      if (prev_outputs != curr_inputs) {
        throw std::runtime_error("Stage input/output size mismatch: previous stage outputs " +
                                 std::to_string(prev_outputs) + " but current stage expects " +
                                 std::to_string(curr_inputs));
      }
    }
  }

  void allocate_buffer_if_needed() {
    // We need N-1 buffers for N stages (last stage writes to user output)
    // Each buffer stores the output of one stage (except the last)
    if (stages.size() > 1 && buffers.size() < stages.size() - 1) {
      size_t buffer_idx = buffers.size();
      size_t buffer_size = stage_outputs[buffer_idx];
      buffers.emplace_back(buffer_size, ngrp, alloc);
    }
  }

  data_type *get_buffer(size_t stage_idx, size_t igrp) { return buffers[stage_idx][igrp].data(); }

  size_t const ngrp;
  Alloc alloc;
  std::vector<stage_type> stages;
  std::vector<buffer_type, buffer_alloc> buffers; // Intermediate buffers between stages
  std::vector<size_t> stage_outputs;              // Track output sizes for buffer allocation
};

} // namespace opflow
