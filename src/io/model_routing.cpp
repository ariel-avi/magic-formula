#include <algorithm>
#include <array>
#include <expected>
#include <format>
#include <magic_formula/detail/text.hpp>
#include <magic_formula/error.hpp>
#include <magic_formula/io/model_routing.hpp>
#include <magic_formula/io/tir_reader.hpp>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace mf::io {
  namespace {
    /// PROPERTY_FILE_FORMAT value of the MSC ADAMS files (docs/research/model-routing.md).
    constexpr std::string_view PAC2002_FORMAT = "PAC2002";

    /// PROPERTY_FILE_FORMAT value of MF-Tyre 5.x files observed in practice (docs/research/model-routing.md).
    constexpr std::string_view MF05_FORMAT = "MF_05";

    /// PROPERTY_FILE_FORMAT value of the TNO MF-Tyre ADAMS user-tire form (USER_SUB_ID), used by the MFEVAL 4.0 example
    /// files (docs/research/model-routing.md). Routes by FITTYP like an absent key.
    constexpr std::string_view USER_FORMAT = "USER";

    /// PROPERTY_FILE_FORMAT value of TNO MF-Tyre files observed in simscape (docs/research/model-routing.md). Routes by
    /// FITTYP like an absent key.
    constexpr std::string_view MF_TYRE_FORMAT = "MF-TYRE";

    /// Every PROPERTY_FILE_FORMAT value that is accepted; any other value is an error (rule 1).
    constexpr std::array<std::string_view, 4> KNOWN_FORMATS{PAC2002_FORMAT, MF05_FORMAT, USER_FORMAT, MF_TYRE_FORMAT};

    /// The only FITTYP that MSC gives a meaning to in PAC2002 files (eq (88), MSC p.33): no warning for it.
    constexpr int PAC2002_NATIVE_FITTYP = 5;

    /// FITTYP of MF-Tyre 5.2 (SIM20 p.36).
    constexpr int MF52_FITTYP = 6;

    /// FITTYP values of MF-Tyre 6.x: 6.0, 6.1, 6.2 and 6.2 with the temperature and velocity model (SIM20 p.36).
    constexpr std::array<int, 4> MF6X_FITTYP{60, 61, 62, 70};

    /// A FITTYP of an unsupported family and the error message that names it.
    struct unsupported_fittyp {
      int code;
      std::string_view message;
    };

    // clang-format off
    /// FITTYP values of families that are recognised but not implemented (SIM20 p.36-37).
    constexpr std::array<unsupported_fittyp, 4> UNSUPPORTED_FITTYP{
        unsupported_fittyp{.code = 5,
                           .message = "FITTYP 5 (MF-Tyre 5.0/5.1) is not supported; set PROPERTY_FILE_FORMAT = "
                                      "'PAC2002' to evaluate the file with PAC2002 (AD-11)"},
        unsupported_fittyp{.code = 21, .message = "FITTYP 21 (MF-Swift 1.x) is not supported (AD-11)"},
        unsupported_fittyp{.code = 51, .message = "FITTYP 51 (MF-MCTyre 1.0) is not supported (AD-11)"},
        unsupported_fittyp{.code = 52, .message = "FITTYP 52 (MF-MCTyre 1.1) is not supported (AD-11)"},
    };

    /// Format strings of the other diagnostics.
    constexpr std::string_view UNKNOWN_FORMAT_MESSAGE =
        "unknown PROPERTY_FILE_FORMAT '{}' (expected 'PAC2002', 'MF_05', 'USER' or 'MF-TYRE') (AD-11)";
    constexpr std::string_view MF6X_MESSAGE = "FITTYP {}: MF-Tyre 6.x is not implemented (AD-11)";
    constexpr std::string_view PAC2002_FITTYP_WARNING = "FITTYP {} ignored for PAC2002 (MSC p.33)";
    constexpr std::string_view CANNOT_IDENTIFY_MESSAGE =
        "cannot identify the tire model: set PROPERTY_FILE_FORMAT = 'PAC2002' or FITTYP = 6 (AD-11)";
    // clang-format on

    /// Appends a warning built member by member.
    void add_warning(std::vector<warning> &warnings, std::string message, location where) {
      warning item;
      item.message = std::move(message);
      item.where = std::move(where);
      warnings.push_back(std::move(item));
    }
  } // namespace

  std::expected<model_kind, error> identify_model(tir_reader &reader, std::vector<warning> &warnings) {
    // Rule 1: an unknown PROPERTY_FILE_FORMAT.
    const std::optional<std::string_view> format_text = reader.text("PROPERTY_FILE_FORMAT");
    const std::string_view file_format = detail::trim(format_text.value_or(std::string_view{}));
    const bool is_pac2002 = detail::iequals_ascii(file_format, PAC2002_FORMAT);
    const bool is_known = std::ranges::any_of(
        KNOWN_FORMATS, [file_format](std::string_view known) { return detail::iequals_ascii(file_format, known); });
    if (format_text.has_value() && !is_known) {
      return make_error(error_code::UNSUPPORTED_MODEL, std::format(UNKNOWN_FORMAT_MESSAGE, file_format),
                        reader.locate("PROPERTY_FILE_FORMAT"));
    }

    // Rule 2: FITTYP must be an integer when present.
    const std::expected<std::optional<int>, error> fittyp_result = reader.integer("FITTYP");
    if (!fittyp_result.has_value()) {
      return std::unexpected<error>(fittyp_result.error());
    }
    const std::optional<int> fittyp = *fittyp_result;
    const int code = fittyp.value_or(0);

    // Rule 3: MF-Tyre 6.x, even in a file that declares PAC2002.
    if (std::ranges::contains(MF6X_FITTYP, code)) {
      return make_error(error_code::UNSUPPORTED_MODEL, std::format(MF6X_MESSAGE, code), reader.locate("FITTYP"));
    }

    // Rule 4: PAC2002 takes precedence over every other FITTYP.
    if (is_pac2002) {
      if (fittyp.has_value() && code != PAC2002_NATIVE_FITTYP) {
        add_warning(warnings, std::format(PAC2002_FITTYP_WARNING, code), reader.locate("FITTYP"));
      }
      return model_kind::PAC2002;
    }

    // Rule 5: MF-Tyre 5.2.
    if (code == MF52_FITTYP) {
      return model_kind::MF52;
    }

    // Rule 6: recognised families that are not implemented.
    const std::span<const unsupported_fittyp> unsupported_codes(UNSUPPORTED_FITTYP);
    const auto unsupported = std::ranges::find(unsupported_codes, code, &unsupported_fittyp::code);
    if (unsupported != unsupported_codes.end()) {
      return make_error(error_code::UNSUPPORTED_MODEL, std::string(unsupported->message), reader.locate("FITTYP"));
    }

    // Rule 7: nothing identifies the file.
    return make_error(error_code::UNSUPPORTED_MODEL, std::string(CANNOT_IDENTIFY_MESSAGE), reader.locate("FITTYP"));
  }
} // namespace mf::io
