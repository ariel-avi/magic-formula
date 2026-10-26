#include <Eigen/Core>
#include <algorithm>
#include <cmath>
#include <format>
#include <iterator>
#include <limits>
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

    /// The NaN every NaN is printed as, whatever its sign bit ("nan", never "-nan").
    constexpr double QUIET_NAN = std::numeric_limits<double>::quiet_NaN();

    /// `value`, except that a NaN of either sign becomes the positive quiet NaN.
    double normalised_nan(double value) noexcept {
      return std::isnan(value) ? QUIET_NAN : value;
    }

    /// Appends the values of row `r` of `values` to `line`, comma-separated like `append_channels`; any NaN prints as
    /// "nan".
    void append_row(std::string &line, std::string_view &separator, const Eigen::Ref<const Eigen::MatrixXd> &values,
                    Eigen::Index r) {
      for (Eigen::Index c = 0; c < values.cols(); ++c) {
        line += separator;
        std::format_to(std::back_inserter(line), "{}", normalised_nan(values(r, c)));
        separator = ",";
      }
    }
  } // namespace

  std::string results_header(const model_layout &layout) {
    std::string text;
    std::string_view separator;
    append_channels(text, separator, layout.inputs);
    append_channels(text, separator, layout.outputs);
    append_channels(text, separator, layout.characteristics);
    // Returned through a move into a prvalue: a returned local would add an exception-cleanup-only line.
    // The braced form would trip -Wpessimizing-move, so the tidy check is suppressed here.
    return std::string(std::move(text)); // NOLINT(modernize-return-braced-init-list): see above
  }

  void write_results(std::ostream &out, const model_layout &layout, const Eigen::Ref<const Eigen::MatrixXd> &inputs,
                     const Eigen::Ref<const Eigen::MatrixXd> &outputs,
                     const Eigen::Ref<const Eigen::MatrixXd> &characteristics) {
    out << results_header(layout) << '\n';
    const Eigen::Index rows = std::min({inputs.rows(), outputs.rows(), characteristics.rows()});
    std::string line;
    for (Eigen::Index r = 0; r < rows; ++r) {
      line.clear();
      std::string_view separator;
      append_row(line, separator, inputs, r);
      append_row(line, separator, outputs, r);
      append_row(line, separator, characteristics, r);
      line += '\n';
      out << line;
    }
  }
} // namespace mf::io
