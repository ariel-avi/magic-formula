#include <Eigen/Core>
#include <algorithm>
#include <format>
#include <iterator>
#include <magic_formula/io/result_writer.hpp>
#include <magic_formula/model_layout.hpp>
#include <ostream>
#include <span>
#include <string>
#include <string_view>
#include <utility>

namespace mf::io {
  namespace {
    /// Appends "NAME[unit]" of every channel to `text`, each preceded by `separator`; `separator` becomes "," after the
    /// first field, so the join needs no branch.
    template <typename Channel_T>
    void append_channels(std::string &text, std::string_view &separator, std::span<const Channel_T> channels) {
      for (const Channel_T &channel : channels) {
        text += separator;
        std::format_to(std::back_inserter(text), "{}[{}]", channel.name, channel.unit);
        separator = ",";
      }
    }

    /// Appends the values of row `r` of `values` to `line`, comma-separated like `append_channels`.
    void append_row(std::string &line, std::string_view &separator, const Eigen::Ref<const Eigen::MatrixXd> &values,
                    Eigen::Index r) {
      for (Eigen::Index c = 0; c < values.cols(); ++c) {
        line += separator;
        std::format_to(std::back_inserter(line), "{}", values(r, c));
        separator = ",";
      }
    }
  } // namespace

  std::string results_header(const model_layout &layout) {
    std::string text;
    std::string_view separator;
    append_channels(text, separator, layout.inputs);
    append_channels(text, separator, layout.outputs);
    // Returned through a move into a prvalue: a returned local would add an exception-cleanup-only line.
    // The braced form would trip -Wpessimizing-move, so the tidy check is suppressed here.
    return std::string(std::move(text)); // NOLINT(modernize-return-braced-init-list): see above
  }

  void write_results(std::ostream &out, const model_layout &layout, const Eigen::Ref<const Eigen::MatrixXd> &inputs,
                     const Eigen::Ref<const Eigen::MatrixXd> &outputs) {
    out << results_header(layout) << '\n';
    const Eigen::Index rows = std::min(inputs.rows(), outputs.rows());
    std::string line;
    for (Eigen::Index r = 0; r < rows; ++r) {
      line.clear();
      std::string_view separator;
      append_row(line, separator, inputs, r);
      append_row(line, separator, outputs, r);
      line += '\n';
      out << line;
    }
  }
} // namespace mf::io
