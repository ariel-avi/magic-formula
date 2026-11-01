#include "../support/fixtures.hpp"

#include <Eigen/Core>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <expected>
#include <gtest/gtest.h>
#include <limits>
#include <magic_formula/detail/lanes.hpp>
#include <magic_formula/detail/mf5x_characteristics.hpp>
#include <magic_formula/error.hpp>
#include <magic_formula/evaluation_types.hpp>
#include <magic_formula/mf52_model.hpp>
#include <magic_formula/mf5x_coefficients.hpp>
#include <magic_formula/mf5x_forms.hpp>
#include <magic_formula/mf5x_layout.hpp>
#include <magic_formula/pac2002_model.hpp>
#include <magic_formula/tire_model.hpp>
#include <memory>
#include <new>
#include <string>
#include <utility>
#include <vector>
#if defined(_MSC_VER)
#include <malloc.h>
#endif

// Two independent detectors guard the raw (pointer) evaluate forms, which are documented as never allocating:
//  - the global allocation functions are replaced by counting versions (catches operator new: std::vector,
//    std::string, std::format results, std::expected payloads, ...);
//  - Eigen's runtime malloc check (EIGEN_RUNTIME_NO_MALLOC, set by the build for Debug and coverage configurations)
//    is switched off around each measured call, so any Eigen heap allocation (dynamic vectors or matrices, temporaries
//    inside the compute functions) aborts the test process. Eigen allocates through std::malloc, which the counting
//    operator new does not see.
// A death test (EigenGuard_DetectsEigenHeapAllocation) is the positive control of the Eigen detector, compiled where
// Eigen's assert is live.
// Guarded: the raw point and raw batch forms, unchecked and checked-success, for both slip modes and for the PAC2002
// cosine, MF 5.2, PAC2002 friction-ellipse and PAC2002 FITTYP-5 shift-form models, so every form branch of the raw hot
// path runs under both detectors. Not guarded: the forms that return an
// Eigen::VectorXd / Eigen::MatrixXd (they allocate their result by design); their compute path is the same code as the
// raw forms, which run through the same Eigen::Ref hooks.
// In a Release build (NDEBUG) Eigen's check is not compiled in and only the operator new counter runs; every Debug
// build must have it, otherwise this file fails to compile.
// Under ThreadSanitizer the global allocator cannot be replaced (the runtime owns it), so the counter is fed by the
// sanitizer's allocation hook instead.
#if defined(__SANITIZE_THREAD__)
#define MF_ALLOCATION_TEST_TSAN 1
#elif defined(__has_feature)
#if __has_feature(thread_sanitizer)
#define MF_ALLOCATION_TEST_TSAN 1
#endif
#endif
#if !defined(MF_ALLOCATION_TEST_TSAN)
#define MF_ALLOCATION_TEST_TSAN 0
#endif

#if !defined(EIGEN_RUNTIME_NO_MALLOC) && !defined(NDEBUG)
#error "allocation_test needs EIGEN_RUNTIME_NO_MALLOC in non-NDEBUG builds (set by mf_configure_target)"
#endif

namespace {
  // NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables): the state of the counting global allocator
  // Number of allocations made while `tracking` is set.
  std::atomic<long> allocation_count{0};
  std::atomic<bool> tracking{false};

  // A sink that makes a pointer escape, so that the compiler cannot elide an allocation.
  double *volatile pointer_sink = nullptr;
  // NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables)

  // A sink that makes a raw block escape, so that the compiler cannot elide a direct call of an allocation function.
  // NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): see above
  void *volatile block_sink = nullptr;
} // namespace

#if MF_ALLOCATION_TEST_TSAN
// The TSan runtime defines the whole allocation family as strong symbols (statically linked, whole archive), so the
// global allocator cannot be replaced there. The runtime reports every allocation (operator new and malloc forms)
// to this hook instead, which feeds the same counter.
extern "C" void __sanitizer_malloc_hook(const volatile void * /*pointer*/, std::size_t /*size*/) {
  if (tracking.load(std::memory_order_relaxed)) {
    allocation_count.fetch_add(1, std::memory_order_relaxed);
  }
}
#else
namespace {
  // The alignment that the plain forms of operator new guarantee.
  constexpr std::size_t DEFAULT_ALIGNMENT = __STDCPP_DEFAULT_NEW_ALIGNMENT__;

  // Allocates `size` bytes (at least 1) aligned to `alignment`; nullptr on failure. MSVC pairs every block with
  // _aligned_free, elsewhere the blocks are released by free.
  void *call_allocate(std::size_t size, std::size_t alignment) {
#if defined(_MSC_VER)
    return _aligned_malloc(size, alignment);
#else
    if (alignment <= DEFAULT_ALIGNMENT) {
      // NOLINTNEXTLINE(cppcoreguidelines-no-malloc,cppcoreguidelines-owning-memory): counting allocator
      return std::malloc(size);
    }
    // aligned_alloc requires the size to be a multiple of the alignment
    if (size > std::numeric_limits<std::size_t>::max() - alignment) {
      return nullptr;
    }
    // NOLINTNEXTLINE(cppcoreguidelines-no-malloc,cppcoreguidelines-owning-memory): counting allocator
    return std::aligned_alloc(alignment, ((size + alignment - 1) / alignment) * alignment);
#endif
  }

  // The allocation function is called through a pointer so that the static analyser does not follow the allocation of
  // the replaced operator new into GoogleTest's matcher storage, where it reports a false leak.
  // NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): volatile function pointer, see above
  void *(*const volatile ALLOCATE_FUNCTION)(std::size_t, std::size_t) = &call_allocate;

  // The one counting allocator behind every replaced allocation form; nullptr on failure.
  void *counted_allocate_or_null(std::size_t size, std::size_t alignment) noexcept {
    if (tracking.load(std::memory_order_relaxed)) {
      allocation_count.fetch_add(1, std::memory_order_relaxed);
    }
    return ALLOCATE_FUNCTION(size == 0 ? 1 : size, alignment);
  }

  void *counted_allocate(std::size_t size, std::size_t alignment = DEFAULT_ALIGNMENT) {
    void *memory = counted_allocate_or_null(size, alignment);
    if (memory == nullptr) {
      throw std::bad_alloc();
    }
    return memory;
  }

  void counted_free(void *memory) noexcept {
#if defined(_MSC_VER)
    _aligned_free(memory);
#else
    std::free(memory); // NOLINT(cppcoreguidelines-no-malloc,cppcoreguidelines-owning-memory): counting allocator
#endif
  }
} // namespace

// NOLINTBEGIN(cppcoreguidelines-no-malloc,cppcoreguidelines-owning-memory,misc-new-delete-overloads): counting
// replacement of the global allocator
// The complete replaceable family is defined, so that no sanitizer or Valgrind runtime allocator (or static runtime
// archive member defining the family) is used for any form, and a block never reaches a release function of another
// allocator. Every form goes through the one counting helper.
void *operator new(std::size_t size) {
  return counted_allocate(size);
}

void *operator new[](std::size_t size) {
  return counted_allocate(size);
}

void *operator new(std::size_t size, const std::nothrow_t &) noexcept {
  return counted_allocate_or_null(size, DEFAULT_ALIGNMENT);
}

void *operator new[](std::size_t size, const std::nothrow_t &) noexcept {
  return counted_allocate_or_null(size, DEFAULT_ALIGNMENT);
}

void *operator new(std::size_t size, std::align_val_t alignment) {
  return counted_allocate(size, static_cast<std::size_t>(alignment));
}

void *operator new[](std::size_t size, std::align_val_t alignment) {
  return counted_allocate(size, static_cast<std::size_t>(alignment));
}

void *operator new(std::size_t size, std::align_val_t alignment, const std::nothrow_t &) noexcept {
  return counted_allocate_or_null(size, static_cast<std::size_t>(alignment));
}

void *operator new[](std::size_t size, std::align_val_t alignment, const std::nothrow_t &) noexcept {
  return counted_allocate_or_null(size, static_cast<std::size_t>(alignment));
}

void operator delete(void *memory) noexcept {
  counted_free(memory);
}

void operator delete[](void *memory) noexcept {
  counted_free(memory);
}

void operator delete(void *memory, [[maybe_unused]] std::size_t size) noexcept {
  counted_free(memory);
}

void operator delete[](void *memory, [[maybe_unused]] std::size_t size) noexcept {
  counted_free(memory);
}

void operator delete(void *memory, const std::nothrow_t &) noexcept {
  counted_free(memory);
}

void operator delete[](void *memory, const std::nothrow_t &) noexcept {
  counted_free(memory);
}

void operator delete(void *memory, [[maybe_unused]] std::align_val_t alignment) noexcept {
  counted_free(memory);
}

void operator delete[](void *memory, [[maybe_unused]] std::align_val_t alignment) noexcept {
  counted_free(memory);
}

void operator delete(void *memory, [[maybe_unused]] std::size_t size,
                     [[maybe_unused]] std::align_val_t alignment) noexcept {
  counted_free(memory);
}

void operator delete[](void *memory, [[maybe_unused]] std::size_t size,
                       [[maybe_unused]] std::align_val_t alignment) noexcept {
  counted_free(memory);
}

void operator delete(void *memory, [[maybe_unused]] std::align_val_t alignment, const std::nothrow_t &) noexcept {
  counted_free(memory);
}

void operator delete[](void *memory, [[maybe_unused]] std::align_val_t alignment, const std::nothrow_t &) noexcept {
  counted_free(memory);
}
// NOLINTEND(cppcoreguidelines-no-malloc,cppcoreguidelines-owning-memory,misc-new-delete-overloads)
#endif

namespace {
  using model_ptr = std::unique_ptr<const mf::tire_model>;

  constexpr int ROWS = 67;

  // Forbids Eigen heap allocations for its lifetime (a no-op when the runtime check is not compiled in).
  class eigen_malloc_guard {
  public:
    eigen_malloc_guard() {
#ifdef EIGEN_RUNTIME_NO_MALLOC
      Eigen::internal::set_is_malloc_allowed(false);
#endif
    }
    eigen_malloc_guard(const eigen_malloc_guard &) = delete;
    eigen_malloc_guard &operator=(const eigen_malloc_guard &) = delete;
    eigen_malloc_guard(eigen_malloc_guard &&) = delete;
    eigen_malloc_guard &operator=(eigen_malloc_guard &&) = delete;
    ~eigen_malloc_guard() {
#ifdef EIGEN_RUNTIME_NO_MALLOC
      Eigen::internal::set_is_malloc_allowed(true);
#endif
    }
  };

  // Runs `body` and returns the number of operator new allocations it made; Eigen heap allocations abort (see the file
  // comment). `body` must not use GoogleTest.
  template <typename Body_T> long count_allocations(const Body_T &body) {
    allocation_count.store(0);
    const eigen_malloc_guard guard;
    tracking.store(true);
    body();
    tracking.store(false);
    return allocation_count.load();
  }

  constexpr std::array<mf::slip_mode, 2> SLIP_MODES{mf::slip_mode::COMBINED, mf::slip_mode::UNCOMBINED};
  constexpr std::size_t MODEL_COUNT = 4;

  // Adds a PAC2002 model on `parameters` with `method`; a failure is reported and the model is left out.
  void add_pac2002(std::vector<model_ptr> &models, const mf::mf5x_parameters &parameters, mf::combined_method method,
                   const mf::model_options &options) {
    std::expected<mf::pac2002_model, mf::error> pac = mf::pac2002_model::create(parameters, method, options);
    if (pac.has_value()) {
      models.push_back(std::make_unique<mf::pac2002_model>(std::move(*pac)));
    } else {
      ADD_FAILURE() << mf::to_string(pac.error());
    }
  }

  // PAC2002 cosine, MF 5.2, PAC2002 friction ellipse and PAC2002 FITTYP-5 shift form on fixture S-rng, so every form
  // branch of the raw hot path runs; failures are reported and the model is left out.
  std::vector<model_ptr> make_models(bool throw_if_model_failure, mf::slip_mode mode) {
    std::vector<model_ptr> models;
    const mf::model_options options{.mode = mode, .throw_if_model_failure = throw_if_model_failure};
    add_pac2002(models, mf::test::fixture_s_rng(), mf::combined_method::COSINE, options);
    mf::mf5x_parameters parameters = mf::test::fixture_s_rng();
    parameters.metadata.fittyp = 6;
    std::expected<mf::mf52_model, mf::error> mf52 = mf::mf52_model::create(parameters, options);
    if (mf52.has_value()) {
      models.push_back(std::make_unique<mf::mf52_model>(std::move(*mf52)));
    } else {
      ADD_FAILURE() << mf::to_string(mf52.error());
    }
    add_pac2002(models, mf::test::fixture_s_rng(), mf::combined_method::FRICTION_ELLIPSE, options);
    mf::mf5x_parameters shifts = mf::test::fixture_s_rng();
    shifts.metadata.fittyp = 5;
    shifts.coefficients.QSY1 = 0.0;
    shifts.coefficients.QSY2 = 0.0;
    add_pac2002(models, shifts, mf::combined_method::COSINE, options);
    return models;
  }

  // The regime grid with Vx raised to at least 0.5 m/s, so that every row is inside both model domains.
  Eigen::MatrixXd valid_grid() {
    Eigen::MatrixXd grid = mf::test::regime_grid(ROWS);
    grid.col(mf::mf5x::VX) = grid.col(mf::mf5x::VX).cwiseMax(0.5);
    return grid;
  }

  // One nominal point inside both model domains.
  std::array<double, mf::mf5x::INPUT_LENGTH> nominal_input() {
    std::array<double, mf::mf5x::INPUT_LENGTH> input{};
    input.at(mf::mf5x::FZ) = 4000.0;
    input.at(mf::mf5x::SA) = 0.05;
    input.at(mf::mf5x::SR) = 0.1;
    input.at(mf::mf5x::IA) = 0.02;
    input.at(mf::mf5x::VX) = 16.6;
    return input;
  }

  void expect_point_without_allocation(bool throw_if_model_failure) {
    const std::array<double, mf::mf5x::INPUT_LENGTH> input = nominal_input();
    std::array<double, mf::mf5x::OUTPUT_LENGTH> output{};
    for (const mf::slip_mode mode : SLIP_MODES) {
      const std::vector<model_ptr> models = make_models(throw_if_model_failure, mode);
      ASSERT_EQ(models.size(), MODEL_COUNT);
      for (const model_ptr &model : models) {
        SCOPED_TRACE(std::string(model->name()) + (mode == mf::slip_mode::COMBINED ? " combined" : " uncombined"));
        const long count = count_allocations(
            [&] { model->evaluate(input.data(), mf::mf5x::INPUT_LENGTH, output.data(), mf::mf5x::OUTPUT_LENGTH); });
        EXPECT_EQ(count, 0);
      }
    }
  }

  void expect_batch_without_allocation(bool throw_if_model_failure) {
    const Eigen::MatrixXd inputs = valid_grid();
    std::vector<double> data(static_cast<std::size_t>(ROWS * mf::mf5x::OUTPUT_LENGTH), 0.0);
    for (const mf::slip_mode mode : SLIP_MODES) {
      const std::vector<model_ptr> models = make_models(throw_if_model_failure, mode);
      ASSERT_EQ(models.size(), MODEL_COUNT);
      for (const model_ptr &model : models) {
        SCOPED_TRACE(std::string(model->name()) + (mode == mf::slip_mode::COMBINED ? " combined" : " uncombined"));
        const long count = count_allocations(
            [&] { model->evaluate_batch(inputs.data(), ROWS, data.data(), ROWS * mf::mf5x::OUTPUT_LENGTH); });
        EXPECT_EQ(count, 0);
      }
    }
  }

  // The characteristics kernel on `rows` rows of the valid grid (offset by 3 rows), in both slip modes and with both
  // combined methods, on fixture S-rng: it must not allocate (it also runs the dual-lane kernels and the root finder).
  template <class Lane_T> void expect_characteristics_without_allocation(Eigen::Index rows) {
    constexpr Eigen::Index offset = 3;
    const Eigen::MatrixXd grid = valid_grid();
    const mf::mf5x_parameters parameters = mf::test::fixture_s_rng();
    mf::detail::input_lanes<Lane_T> raw;
    raw.fz_n = Lane_T::Zero(rows);
    raw.alpha_rad = Lane_T::Zero(rows);
    raw.kappa = Lane_T::Zero(rows);
    raw.gamma_rad = Lane_T::Zero(rows);
    raw.vx_mps = Lane_T::Zero(rows);
    for (Eigen::Index i = 0; i < rows; ++i) {
      raw.fz_n(i) = grid(i + offset, mf::mf5x::FZ);
      raw.alpha_rad(i) = grid(i + offset, mf::mf5x::SA);
      raw.kappa(i) = grid(i + offset, mf::mf5x::SR);
      raw.gamma_rad(i) = grid(i + offset, mf::mf5x::IA);
      raw.vx_mps(i) = grid(i + offset, mf::mf5x::VX);
    }
    for (const mf::slip_mode mode : SLIP_MODES) {
      for (const mf::combined_method method : {mf::combined_method::COSINE, mf::combined_method::FRICTION_ELLIPSE}) {
        mf::mf5x_forms forms;
        forms.combined = method;
        const long count = count_allocations([&] {
          mf::detail::characteristic_lanes<Lane_T> out =
              mf::detail::mf5x_characteristics<Lane_T>(parameters.coefficients, forms, mode, parameters.limits, raw);
          pointer_sink = out.dy_n.data();
        });
        EXPECT_EQ(count, 0);
      }
    }
  }
} // namespace

TEST(allocation, Mf5xCharacteristicsPoint_BothModesBothMethods_NoAllocation) {
  expect_characteristics_without_allocation<mf::detail::point_lane>(1);
}

TEST(allocation, Mf5xCharacteristicsBlock_BothModesBothMethods_NoAllocation) {
  expect_characteristics_without_allocation<mf::detail::block_lane>(mf::detail::BLOCK_ROWS);
}

TEST(allocation, Counter_DetectsStdVectorAllocation) {
  const long count = count_allocations([] {
    std::vector<double> values(64, 1.0);
    pointer_sink = values.data();
  });
  EXPECT_GE(count, 1);
}

// The nothrow and aligned forms go through the same counter and release; the blocks are released by the matching
// delete forms (a mismatch would be reported by the sanitizers and Valgrind).
TEST(allocation, Counter_DetectsNothrowAndAlignedForms) {
  constexpr std::size_t alignment = 64;
  const std::align_val_t aligned{alignment};
  const long count = count_allocations([&] {
    block_sink = ::operator new(16, std::nothrow);
    ::operator delete(block_sink, std::nothrow);
    block_sink = ::operator new[](16, std::nothrow);
    ::operator delete[](block_sink, std::nothrow);
    block_sink = ::operator new(16, aligned);
    ::operator delete(block_sink, aligned);
    block_sink = ::operator new[](16, aligned);
    ::operator delete[](block_sink, 16, aligned);
    block_sink = ::operator new(16, aligned, std::nothrow);
    ::operator delete(block_sink, 16, aligned);
    block_sink = ::operator new[](16, aligned, std::nothrow);
    ::operator delete[](block_sink, aligned, std::nothrow);
  });
  EXPECT_EQ(count, 6);
}

TEST(allocation, AlignedNew_OverAligned_ReturnsAlignedBlock) {
  constexpr std::size_t alignment = 128;
  void *block = ::operator new(1, std::align_val_t{alignment});
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast): address arithmetic for the alignment check
  EXPECT_EQ(reinterpret_cast<std::uintptr_t>(block) % alignment, 0U);
  ::operator delete(block, std::align_val_t{alignment});
}

// Positive control for the Eigen guard: an Eigen heap allocation under eigen_malloc_guard must abort the process.
// Compiled only where eigen_assert is live (the runtime check is an eigen_assert, compiled out by NDEBUG or
// EIGEN_NO_DEBUG); there the build always defines EIGEN_RUNTIME_NO_MALLOC (see the #error above).
#if defined(EIGEN_RUNTIME_NO_MALLOC) && !defined(NDEBUG) && !defined(EIGEN_NO_DEBUG)
TEST(allocation, EigenGuard_DetectsEigenHeapAllocation) {
  EXPECT_DEATH(
      {
        const eigen_malloc_guard guard;
        Eigen::VectorXd values = Eigen::VectorXd::Zero(64);
        pointer_sink = values.data();
      },
      "");
}
#endif

TEST(allocation, EvaluateRawPoint_Unchecked_NoAllocation) {
  expect_point_without_allocation(false);
}

TEST(allocation, EvaluateRawPoint_CheckedSuccess_NoAllocation) {
  expect_point_without_allocation(true);
}

TEST(allocation, EvaluateRawBatch_Unchecked_NoAllocation) {
  expect_batch_without_allocation(false);
}

TEST(allocation, EvaluateRawBatch_CheckedSuccess_NoAllocation) {
  expect_batch_without_allocation(true);
}
