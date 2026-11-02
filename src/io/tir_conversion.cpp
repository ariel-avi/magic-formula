#include <algorithm>
#include <cstddef>
#include <expected>
#include <format>
#include <functional>
#include <magic_formula/coordinate_identification.hpp>
#include <magic_formula/coordinate_system.hpp>
#include <magic_formula/error.hpp>
#include <magic_formula/io/model_loader.hpp>
#include <magic_formula/io/number.hpp>
#include <magic_formula/io/tir_conversion.hpp>
#include <magic_formula/io/tir_document.hpp>
#include <magic_formula/mf5x_signs.hpp>
#include <magic_formula/mf6x_signs.hpp>
#include <magic_formula/model_layout.hpp>
#include <magic_formula/sign_transform.hpp>
#include <magic_formula/tire_model.hpp>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace mf::io {
  namespace {
    /// One value replacement: the byte span of the value as written plus the change record (`change.after` is the
    /// replacement text).
    struct edit {
      std::size_t offset = 0;
      std::size_t length = 0;
      tir_change change;
    };

    /// True when `value` parses to zero. Zeros stay as written (negating them changes nothing).
    bool is_zero_value(const std::string &value) {
      return parse_number(value).value_or(1.0) == 0.0;
    }

    /// Records the replacement of `entry`'s value by `replacement`; dropped when it equals the text as written.
    void add_edit(std::vector<edit> &edits, const tir_entry &entry, std::string replacement) {
      if (replacement == entry.value) {
        return;
      }
      tir_change change;
      change.section = entry.section;
      change.key = entry.key;
      change.line = entry.line;
      change.before = entry.value;
      change.after = std::move(replacement);
      edits.push_back(edit{.offset = entry.value_offset, .length = entry.value_length, .change = std::move(change)});
    }

    /// Edits for every present, non-zero coefficient of the sign table whose transform is planned. Today both MF 5.x
    /// kinds use the same table; MF 6.x will select its table by model kind.
    template <typename Rows_T>
    void collect_coefficient_edits(const tir_document &document, const sign_transforms &plan, std::vector<edit> &edits,
                                   const Rows_T &rows) {
      for (const auto &row : rows) {
        if (!contains(plan, row.negated_by)) {
          continue;
        }
        const tir_entry *const entry = document.find(row.key);
        if (entry == nullptr || is_zero_value(entry->value)) {
          continue;
        }
        add_edit(edits, *entry, negate_number_text(entry->value));
      }
    }

    /// Edits for every limit pair whose transform is planned and whose two keys are both present: the pair becomes
    /// (-upper, -lower), written from the texts as they are.
    void collect_limit_edits(const tir_document &document, std::span<const limit_channel> limits,
                             const sign_transforms &plan, std::vector<edit> &edits) {
      for (std::size_t i = 0; i + 1 < limits.size(); ++i) {
        if (limits[i].role != limit_role::LOWER || !contains(plan, limits[i].negated_by)) {
          continue;
        }
        const tir_entry *const lower = document.find(limits[i].key);
        const tir_entry *const upper = document.find(limits[i + 1].key);
        if (lower == nullptr || upper == nullptr) {
          continue;
        }
        add_edit(edits, *lower, negate_number_text(upper->value));
        add_edit(edits, *upper, negate_number_text(lower->value));
      }
    }

    /// `text` with every edit applied. Reorders `edits` (descending offset) so that earlier splices do not move later
    /// ones.
    std::string splice_edits(std::string_view text, std::vector<edit> &edits) {
      std::ranges::sort(edits, std::greater<>{}, &edit::offset);
      std::string out{text};
      for (const edit &item : edits) {
        out.replace(item.offset, item.length, item.change.after);
      }
      return std::string{std::move(out)};
    }

    /// The change records of `edits`, in file order.
    std::vector<tir_change> change_records(std::vector<edit> &edits) {
      std::vector<tir_change> changes;
      changes.reserve(edits.size());
      for (edit &item : edits) {
        changes.push_back(std::move(item.change));
      }
      std::ranges::sort(changes, std::less<>{}, &tir_change::line);
      return std::vector<tir_change>{std::move(changes)};
    }
  } // namespace

  std::string negate_number_text(std::string_view text) {
    if (text.starts_with('-')) {
      return std::string{text.substr(1)};
    }
    if (text.starts_with('+')) {
      text.remove_prefix(1);
    }
    return std::format("-{}", text);
  }

  std::expected<tir_conversion, error> convert_tir(std::string_view text, coordinate_system target,
                                                   std::string file_name) {
    const auto document = tir_document::parse(text, std::move(file_name));
    if (!document.has_value()) {
      return std::unexpected<error>(document.error());
    }
    const auto loaded = load_tire_model(*document);
    if (!loaded.has_value()) {
      return std::unexpected<error>(loaded.error());
    }
    const tire_model &model = *loaded->model;
    const axis_signs effective = model.find_coordinate_system().effective;
    const sign_transforms plan = plan_conversion(effective, target);

    std::vector<edit> edits;
    if (model.metadata().fittyp == 61) {
      collect_coefficient_edits(*document, plan, edits, mf6x::COEFFICIENT_SIGNS);
    } else {
      collect_coefficient_edits(*document, plan, edits, mf5x::COEFFICIENT_SIGNS);
    }
    collect_limit_edits(*document, model.layout().limits, plan, edits);
    tir_conversion result;
    result.text = splice_edits(text, edits);
    result.applied = plan;
    result.changes = change_records(edits);
    return result;
  }
} // namespace mf::io
