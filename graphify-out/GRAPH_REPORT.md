# Graph Report - magic-formula  (2026-10-29)

## Corpus Check
- 311 files · ~314,040 words
- Verdict: corpus is large enough that graph structure adds value.
- Unclassified: 46 file(s) not represented in the graph (top: .csv 17, .tir 11, (none) 9)

## Summary
- 5825 nodes · 12805 edges · 254 communities (217 shown, 37 thin omitted)
- Extraction: 95% EXTRACTED · 5% INFERRED · 0% AMBIGUOUS · INFERRED: 587 edges (avg confidence: 0.89)
- Token cost: 0 input · 0 output

## Graph Freshness
- Built from commit: `4989345d`
- Run `git rev-parse HEAD` and compare to check if the graph is stale.
- Run `graphify update .` after code changes (no API cost).

## Community Hubs (Navigation)
- mf61_coefficients
- mf61_equations
- mf5x_coefficients
- TEST
- fixtures.hpp
- pytest
- model.ts
- TEST
- support.ts
- chart-spec.ts
- cn
- TEST
- output_lanes
- evaluation_types.hpp
- TEST
- TEST
- TEST
- TEST
- coordinate_system
- defaults.ts
- mfeval_reference_test.cpp
- cli.cpp
- TEST
- CoordinateSystemPanel.tsx
- TEST
- coordinate_identification_test.cpp
- SweepToolbar.tsx
- TireModel
- __init__.py
- ChartView.tsx
- TEST
- Class hierarchy
- TEST
- TEST
- mf_bench.cpp
- tir_edit_test.cpp
- mf5x_kernels_test.cpp
- tire_model.cpp
- mf5x_kernels.hpp
- mf_wasm.d.ts
- TEST
- mf5x_characteristics_test.cpp
- TYPED_TEST
- input_table.cpp
- test_examples_page.py
- identification_probes.cpp
- vitest
- mf_wasm.cpp
- TEST
- TEST
- TEST
- TEST
- TEST
- TEST
- feature.js
- TEST
- screenshots.spec.ts
- magic-formula for Python
- Coordinate systems
- tire_model
- TEST
- EntryField.tsx
- TEST
- TEST
- error
- location
- TEST
- input_table
- Class hierarchy
- dual_lane_test.cpp
- tire-scene.ts
- dual_lane
- coordinate_identification.cpp
- stub_model
- TEST_P
- result_row
- TEST
- _core
- uniform
- package.json
- values_t
- run_examples_page.py
- combined_method
- configuration
- TEST
- conftest.py
- axis_decision
- indicator_definition
- outputs_t
- indicator_reading
- TEST
- load_state
- plan_probes
- loaded_model
- conditioned_lanes
- input
- diagnostics_input
- tir_document.cpp
- error_record
- test_coordinates.py
- string
- model_handle
- scripts
- compilerOptions
- TEST
- mf5x_parameters
- longitudinal_terms
- load_mf61
- model_pair
- TEST_P
- tir_document
- string_view
- char_curve
- stub_model
- TEST
- lanes_of
- TEST
- route
- data_file
- linear_stub
- outputs_t
- components.json
- compilerOptions
- compilerOptions
- Build and test
- TEST
- Tire characteristics: stiffness, peak factors and slip at peak
- make_model
- read_text_file
- TEST
- pure_scalars
- streambuf
- Tire model domain primer
- review.js
- identification_probes
- Tire coordinate systems: ISO, SAE, conversion and identification
- limit_role
- TEST
- identification_record
- binding_role
- weight_inputs
- ellipse_ratios
- Conventions shared by both models
- mf61_model
- output_index
- dependencies
- devDependencies
- TEST
- Agent workflow
- 5. Details
- Validation
- options
- aligning_terms
- tir_reader
- error_code
- text.cpp
- magic-formula
- mf5x_forms
- ratios_of
- Test data and vectors
- run
- mf5x_bindings.cpp
- mf5x_signs.hpp
- limit_index
- reading_record
- input_table_file
- size_t
- invalid_case
- contract_case
- concurrency_case
- number_test.cpp
- input_of
- expected_file
- coefficient_binding
- limit_pair
- ThemeProvider.tsx
- README.md
- test_package.py
- probe_slot
- expect_curvature_clamp
- output_index
- orchestrator.md
- TEST_P
- indicator_form
- 4. Peak condition and existence
- fz_sign_census
- model_contract
- Web frontend by example
- Project policies
- limit_index
- parse_number
- model_options
- mf61_parameters
- magic-formula
- slot_value
- characteristic_index
- characteristic_index
- 2. Closed-form derivatives (derived)
- expected_table
- values
- guard-master.sh
- C++ conventions
- CLI
- MF 5.2 (FITTYP = 6)
- identify_model
- expect_flips_declared_columns
- input_index
- Model routing: FITTYP and PROPERTY_FILE_FORMAT
- string_view
- longvl_case
- expect_failure
- general-developer.md
- researcher.md
- input_tables_in_data_dir
- append_state
- s52_with
- contains
- version
- tests/data
- bits
- cpp-developer.md
- Coverage
- make_location
- .tir property file
- pull_request_template.md
- nightly-findings.sh
- characteristics_grid
- limits_of
- input_index
- check_combine_value_bit_equal
- architect.md
- planner.md
- project-explorer.md
- tests/data/reference
- ellipse_case
- general-purpose.md
- reviewer.md
- test-runner.md
- Open questions
- coefficient_entry
- unit_entry
- si_unit
- slip_point
- batch_value_matches
- lanes.hpp
- build.sh
- tsconfig.json
- pac2002-vs-mf52.md
- nightly-changes.sh
- resolve-python-deps.sh
- setup-emsdk.sh
- magic-formula

## God Nodes (most connected - your core abstractions)
1. `mf61_coefficients` - 189 edges
2. `error` - 161 edges
3. `TEST()` - 152 edges
4. `mf5x_coefficients` - 120 edges
5. `mf61_equations` - 116 edges
6. `TEST()` - 97 edges
7. `TEST()` - 96 edges
8. `TEST()` - 88 edges
9. `TEST()` - 82 edges
10. `cn()` - 79 edges

## Surprising Connections (you probably didn't know these)
- `1. Evaluation pipeline` --references--> `mf5x_condition()`  [INFERRED]
  docs/research/policies.md → include/magic_formula/detail/mf5x_kernels.hpp
- `9. Characteristics` --references--> `convert_characteristics()`  [INFERRED]
  docs/research/policies.md → src/model/coordinate_system.cpp
- `PR rule (you own it)` --references--> `commit()`  [INFERRED]
  .claude/agents/orchestrator.md → web/e2e/support.ts
- `Class hierarchy` --references--> `options`  [INFERRED]
  docs/architecture.md → include/magic_formula/cli/cli.hpp
- `Coordinate systems` --references--> `indicator_reading`  [INFERRED]
  docs/architecture.md → include/magic_formula/coordinate_identification.hpp

## Import Cycles
- None detected.

## Communities (254 total, 37 thin omitted)

### Community 0 - "mf61_coefficients"
Cohesion: 0.01
Nodes (188): mf61_coefficients, BREFF, DREFF, FNOMIN, FREFF, LCX, LCY, LEX (+180 more)

### Community 1 - "mf61_equations"
Cohesion: 0.07
Nodes (16): Characteristics and limits, Coordinate gauges, Deliberate differences from the source, Evaluation order, Evidence boundaries, MF 6.1, Scope and provenance, Spin-stiffness correction and regression references (+8 more)

### Community 2 - "mf5x_coefficients"
Cohesion: 0.02
Nodes (118): mf5x_coefficients, FNOMIN, LCX, LCY, LEX, LEY, LFZO, LGAX (+110 more)

### Community 4 - "fixtures.hpp"
Cohesion: 0.13
Nodes (4): PrintTo(), eigen_malloc_guard, TEST_P(), validate_coefficients_invalid

### Community 5 - "pytest"
Cohesion: 0.05
Nodes (54): assert_close_nan_aware(), test_characteristics_batch_c1_c2_values(), test_characteristics_batch_invalid_row_raises_with_row(), test_characteristics_batch_no_contact_rows(), test_characteristics_batch_shape_errors(), test_characteristics_batch_shape_n_by_8(), test_characteristics_batch_zero_slip_secant_equals_tangent(), test_characteristics_ellipse_zero_slip_directional_limit() (+46 more)

### Community 6 - "model.ts"
Cohesion: 0.07
Nodes (41): assumed(), CLASSIFICATION_LABELS, convertSpec(), describeIdentification(), MIXED_SCENE, negate(), ReadingRow, readingRows() (+33 more)

### Community 8 - "support.ts"
Cohesion: 0.15
Nodes (19): @playwright/test, loadModel(), loadIso(), columnTexts(), commit(), curvePoints(), curves(), dashes() (+11 more)

### Community 9 - "chart-spec.ts"
Cohesion: 0.05
Nodes (82): useSweepSolutions(), InputRowProps, SweepSettingsProps, AXIS_GROUPS, axisChannel(), axisDomain(), axisKey(), axisLabel() (+74 more)

### Community 10 - "cn"
Cohesion: 0.07
Nodes (55): class-variance-authority, radix-ui, react, AppHeader(), AppHeaderProps, EmptyStateProps, InputRow(), StepValuesInput() (+47 more)

### Community 11 - "TEST"
Cohesion: 0.03
Nodes (3): char_gxa(), char_gyk(), TEST()

### Community 12 - "output_lanes"
Cohesion: 0.06
Nodes (43): characteristic_lanes, dfx_dsr_n, dfy_dsa_n_per_rad, dx_n, dy_n, fx_over_sr_n, fy_over_sa_n_per_rad, sa_at_peak_fy_rad (+35 more)

### Community 13 - "evaluation_types.hpp"
Cohesion: 0.06
Nodes (12): mf52_model, mf5x_model, coefficients_, forms_, PYBIND11_MODULE(), mf52_model::create(), mf52_model::name(), mf5x_model::validate_parameters() (+4 more)

### Community 15 - "TEST"
Cohesion: 0.05
Nodes (30): tir_change, after, before, key, line, section, tir_conversion, applied (+22 more)

### Community 18 - "coordinate_system"
Cohesion: 0.11
Nodes (23): 2. TNO MF-Tyre ISO-W context, coordinate_system, ISO, SAE, find_channel(), model_layout, characteristics, inputs (+15 more)

### Community 19 - "defaults.ts"
Cohesion: 0.08
Nodes (35): EntryFieldProps, ParameterPanelProps, SWEEP_ID_PREFIX, SWEEP_NAME_PREFIX, SweptRange, defaultFixedValue(), defaultSweptRange(), Entries (+27 more)

### Community 20 - "mfeval_reference_test.cpp"
Cohesion: 0.07
Nodes (13): read_text_file(), read_text_stream(), failing_buffer, TEST(), describe(), expect_error(), expect_syntax_error(), parse() (+5 more)

### Community 21 - "cli.cpp"
Cohesion: 0.15
Nodes (22): apply_system_option(), describe(), describe_decision(), execute(), fail(), flag_wording, channel, consequence (+14 more)

### Community 22 - "TEST"
Cohesion: 0.04
Nodes (4): finite_outputs(), nominal_input(), nominal_inputs(), TEST()

### Community 23 - "CoordinateSystemPanel.tsx"
Cohesion: 0.13
Nodes (38): App(), Document, SeriesTable(), CoordinateSystemPanel(), TireScene, EmptyState(), LimitRow, limitRows() (+30 more)

### Community 25 - "coordinate_identification_test.cpp"
Cohesion: 0.12
Nodes (15): coordinate_identification, classification, effective, lateral, readings, reference_input, vertical, warnings (+7 more)

### Community 26 - "SweepToolbar.tsx"
Cohesion: 0.17
Nodes (17): LegendItem(), SeriesLegend(), DashSwatch(), HINTS, LABELS, RoleSwitch(), RoleSwitchProps, SweepToolbarProps (+9 more)

### Community 28 - "__init__.py"
Cohesion: 0.05
Nodes (26): Batch evaluation, Characteristics, Combined slip, Coordinate systems, Edit coefficients in memory, Errors and warnings, Force curves with broadcasting, Load a model (+18 more)

### Community 29 - "ChartView.tsx"
Cohesion: 0.14
Nodes (32): ChartView(), ChartViewProps, InputEditor(), InputEditorProps, SweepToolbar(), Tabs(), TabsContent(), TabsList() (+24 more)

### Community 30 - "TEST"
Cohesion: 0.05
Nodes (10): case_name(), cli_options_position, cli_options_wrong_count, line_case, args, name, positionals, parse() (+2 more)

### Community 31 - "Class hierarchy"
Cohesion: 0.13
Nodes (33): Error handling, Layers, Python bindings, batch_error, cause, index, make_error(), batch_failure() (+25 more)

### Community 34 - "mf_bench.cpp"
Cohesion: 0.13
Nodes (30): BM_ConvertCoordinateSystem(), BM_ConvertTir(), BM_EvaluateBatch_IsoTable(), BM_FindCoordinateSystem(), BM_Mf52_CharacteristicsBatch(), BM_Mf52_EvaluateBatchEigen(), BM_Mf52_EvaluateBatchRaw(), BM_Mf52_EvaluatePointEigen() (+22 more)

### Community 35 - "tir_edit_test.cpp"
Cohesion: 0.19
Nodes (5): load_text(), read_fixture(), sweep_inputs(), sweep_outputs(), TEST()

### Community 36 - "mf5x_kernels_test.cpp"
Cohesion: 0.11
Nodes (26): lane(), lane_of(), coeffs, alpha_eq_of(), cosine_mf_of(), ellipse_coefficients(), fx0_at(), fy0_at() (+18 more)

### Community 37 - "tire_model.cpp"
Cohesion: 0.23
Nodes (10): require(), run_policy(), tire_model::evaluate(), tire_model::evaluate_batch(), tire_model::evaluate_batch_into(), tire_model::evaluate_characteristics(), tire_model::evaluate_characteristics_batch(), tire_model::evaluate_into() (+2 more)

### Community 38 - "mf5x_kernels.hpp"
Cohesion: 0.10
Nodes (52): Naming, MF 5.x pipeline, limit_curvature(), mf5x_planar_forces(), aligning(), alpha_eq(), combine(), combined_forces (+44 more)

### Community 39 - "mf_wasm.d.ts"
Cohesion: 0.05
Nodes (28): MfWasmModule, RawAxisDecision, RawAxisSign, RawBatchWarning, RawCharacteristicChannel, RawCharacteristicsEvaluation, RawClassification, RawConversion (+20 more)

### Community 40 - "TEST"
Cohesion: 0.07
Nodes (16): add_pac2002(), call_allocate(), count_allocations(), counted_allocate(), counted_allocate_or_null(), counted_free(), expect_batch_without_allocation(), expect_characteristics_without_allocation() (+8 more)

### Community 41 - "mf5x_characteristics_test.cpp"
Cohesion: 0.03
Nodes (66): force_lanes, fx_n, fy_n, mx_nm, my_nm, mz_nm, chars, chars_at() (+58 more)

### Community 43 - "input_table.cpp"
Cohesion: 0.16
Nodes (26): Input table, column_def, name, column_spec, def, factor, has_column(), line_cursor (+18 more)

### Community 44 - "test_examples_page.py"
Cohesion: 0.05
Nodes (17): MagicFormulaConan, _page_images(), _page_snippets(), runner(), test_main_fails_and_prints_the_actual_output(), test_page_covers_the_api(), test_page_images_are_committed(), test_page_runs_and_shows_real_outputs() (+9 more)

### Community 45 - "identification_probes.cpp"
Cohesion: 0.45
Nodes (6): clamp_columns_of(), read_probes(), read_slot(), read_value(), row_clamped(), rows_to_matrix()

### Community 46 - "vitest"
Cohesion: 0.09
Nodes (32): recharts, vitest, SweepChart(), ChartContainer(), ChartTooltipContent(), ChartTooltipContentProps, formatLimit(), formatNumber() (+24 more)

### Community 47 - "mf_wasm.cpp"
Cohesion: 0.15
Nodes (20): batch_job, inputs, loaded, rows, convert_model(), EMSCRIPTEN_BINDINGS(), evaluate(), evaluate_turn_slip() (+12 more)

### Community 48 - "TEST"
Cohesion: 0.06
Nodes (3): printed_tol(), cos_prime(), TEST()

### Community 50 - "TEST"
Cohesion: 0.08
Nodes (9): append_channels(), append_row(), normalised_nan(), results_header(), write_results(), filled(), split_lines(), TEST() (+1 more)

### Community 52 - "TEST"
Cohesion: 0.08
Nodes (6): expect_same_bits(), sample_characteristics(), sample_inputs(), sample_outputs(), table_with_fz(), TEST()

### Community 53 - "TEST"
Cohesion: 0.07
Nodes (6): expect_inputs(), expect_limit_pair(), expect_output(), is_negative_infinity(), is_positive_infinity(), TEST()

### Community 54 - "feature.js"
Cohesion: 0.10
Nodes (27): BRANCH_SCHEMA, bullet(), byId, DESIGN_SCHEMA, DEV_SCHEMA, devPrompt(), devRules(), FINAL_REVIEW_SCHEMA (+19 more)

### Community 55 - "TEST"
Cohesion: 0.06
Nodes (46): slip_mode, COMBINED, UNCOMBINED, block_inputs_of(), clamp_to(), coefficients(), condition_on_block(), condition_on_both() (+38 more)

### Community 56 - "screenshots.spec.ts"
Cohesion: 0.10
Nodes (16): @tailwindcss/vite, @vitejs/plugin-react, ALL, capture(), DOCS_IMAGE_DIR, fileName(), settle(), Shot (+8 more)

### Community 57 - "magic-formula for Python"
Cohesion: 0.12
Nodes (15): API reference, Example notebook, Install, License, Linux, Loading, macOS (Homebrew LLVM 21), magic-formula for Python (+7 more)

### Community 58 - "Coordinate systems"
Cohesion: 0.10
Nodes (18): Architecture, CLI flow, Design patterns, Extending to other formulations, Index-mapped inputs, outputs and limits, Loading a `.tir` file, Options and error model, The four evaluate forms (+10 more)

### Community 59 - "tire_model"
Cohesion: 0.06
Nodes (13): model_metadata, fittyp, side, use_mode, tyre_side, LEFT, RIGHT, tire_model (+5 more)

### Community 60 - "TEST"
Cohesion: 0.09
Nodes (5): describe(), expect_invalid_value(), read_fixture(), TEST(), value_in()

### Community 61 - "EntryField.tsx"
Cohesion: 0.17
Nodes (23): AxisSelect(), AxisSelectProps, EntryField(), FieldError(), NumberEntry(), TextEntry(), UnitEntry(), Select() (+15 more)

### Community 62 - "TEST"
Cohesion: 0.07
Nodes (5): expect_scaled(), TEST(), pair_names, lower, upper

### Community 63 - "TEST"
Cohesion: 0.10
Nodes (13): as_mf5x(), characteristic_inputs(), characteristic_matches(), characteristic_skipped(), check_characteristics_plan(), check_characteristics_round_trip(), expect_characteristics_match(), is_ellipse() (+5 more)

### Community 64 - "error"
Cohesion: 0.08
Nodes (22): error, message, where, failure_text(), model_failure::model_failure(), model_failure::row(), built_model, model (+14 more)

### Community 65 - "location"
Cohesion: 0.14
Nodes (9): location, file, key, line, model_failure, cause_, prefixed(), to_string() (+1 more)

### Community 66 - "TEST"
Cohesion: 0.11
Nodes (4): all_bindings(), expect_fixture_s_without_combined(), expect_same_coefficients(), TEST()

### Community 67 - "input_table"
Cohesion: 0.18
Nodes (10): input_table, columns, file, header_line, lines, values, model_inputs, ignored_columns (+2 more)

### Community 68 - "Class hierarchy"
Cohesion: 0.06
Nodes (49): Class hierarchy, Coordinate systems, axis_signs, lateral, vertical, characteristic_channel, name, nan_allowed (+41 more)

### Community 69 - "dual_lane_test.cpp"
Cohesion: 0.16
Nodes (22): add_epsilon_value(), check_combined_slopes(), check_defined_ratio_slope(), combined_coefficients(), cosine_slopes(), ellipse_slopes(), expect_scaled(), expect_slopes_at() (+14 more)

### Community 70 - "tire-scene.ts"
Cohesion: 0.12
Nodes (35): CoordinateSystemPanelProps, buildScene(), disposeAll(), label(), ROLE_TOKENS, TireScene(), token(), vector() (+27 more)

### Community 71 - "dual_lane"
Cohesion: 0.08
Nodes (21): Eigen, Characteristics, Lanes and performance, dual_lane, derivative, MaxRowsAtCompileTime, RowsAtCompileTime, value (+13 more)

### Community 72 - "coordinate_identification.cpp"
Cohesion: 0.14
Nodes (29): identification_axis, CHECK, LATERAL, VERTICAL, warning, message, where, to_reading_record() (+21 more)

### Community 73 - "stub_model"
Cohesion: 0.17
Nodes (3): stub_model, characteristic_values_, emit_nan_

### Community 74 - "TEST_P"
Cohesion: 0.10
Nodes (6): characteristic_agree(), characteristic_scale_of(), is_peak_characteristic(), is_physical_output(), scale_of(), TEST_P()

### Community 75 - "result_row"
Cohesion: 0.10
Nodes (23): expect_all_characteristics_match(), expect_characteristics_match(), expect_flags(), expect_inputs(), expect_no_flags(), expect_zero_outputs(), result_row, below_vxlow (+15 more)

### Community 77 - "_core"
Cohesion: 0.09
Nodes (12): coefficient_sign, key, mf61_coefficients, negated_by, transform_coefficients(), contains(), sign_transform, LATERAL (+4 more)

### Community 78 - "uniform"
Cohesion: 0.14
Nodes (17): all_coefficient_members(), bits(), coefficients_bitwise_equal(), coefficients_value_equal(), draw_plausible_iso_coefficients(), draw_wild_coefficients(), limits_bitwise_equal(), limits_value_equal() (+9 more)

### Community 79 - "package.json"
Cohesion: 0.09
Nodes (23): clsx, @fontsource/ibm-plex-mono, @fontsource/stix-two-text, @fontsource-variable/ibm-plex-sans, react-dom, react-is, tailwind-merge, tailwindcss (+15 more)

### Community 80 - "values_t"
Cohesion: 0.10
Nodes (23): rel_near(), rel_near_result(), check_binary(), check_unary(), dual_slice(), e_grid(), ellipse_force_slopes(), expect_dual() (+15 more)

### Community 81 - "run_examples_page.py"
Cohesion: 0.12
Nodes (7): main(), Mismatch, _normalise(), PageError, parse_page(), run_page(), Snippet

### Community 82 - "combined_method"
Cohesion: 0.21
Nodes (15): input_of(), characteristic_points(), characteristics_batch_of(), characteristics_of(), ellipse_batch_rows(), ellipse_point_rows(), eval(), eval_ellipse() (+7 more)

### Community 83 - "configuration"
Cohesion: 0.09
Nodes (27): check_set(), configuration, mode_index, plan_index, ranges, start_index, variant_index, coordinate_conversion_grid (+19 more)

### Community 85 - "conftest.py"
Cohesion: 0.11
Nodes (12): After opening, Branches, Commits (Conventional Commits), Dependabot PRs, Hard rules, Opening the PR, PR workflow, data_dir() (+4 more)

### Community 86 - "axis_decision"
Cohesion: 0.11
Nodes (22): axis_decision, basis, confidence, negative_weight, positive_weight, sign, axis_sign, NEGATIVE (+14 more)

### Community 87 - "indicator_definition"
Cohesion: 0.14
Nodes (14): indicator_definition, axis, channel, form, load_sweep, meaning, name, polarity (+6 more)

### Community 88 - "outputs_t"
Cohesion: 0.13
Nodes (15): all_finite(), outputs_differ(), outputs_t, alpha_clamped, below_vxlow, fx_n, fy_n, fz_clamped (+7 more)

### Community 89 - "indicator_reading"
Cohesion: 0.09
Nodes (24): indicator_reading, definition, load_n, load_scale, normalised, status, value, vote (+16 more)

### Community 91 - "load_state"
Cohesion: 0.13
Nodes (19): add_final_warnings(), add_warning(), file_location(), load_state, defaulted, kind, method, missing_r (+11 more)

### Community 92 - "plan_probes"
Cohesion: 0.24
Nodes (11): add_rows(), append_row(), plan_slot(), plan_state, rows, slots, resolved_channels, fz (+3 more)

### Community 93 - "loaded_model"
Cohesion: 0.13
Nodes (8): loaded_model, ignored_keys, model, warnings, pac2002_model, pac2002_of(), make_pac2002(), pac2002_of()

### Community 94 - "conditioned_lanes"
Cohesion: 0.22
Nodes (8): conditioned_lanes, alpha_clamped, below_vxlow, contact, fz_clamped, gamma_clamped, kappa_clamped, point

### Community 95 - "input"
Cohesion: 0.15
Nodes (5): input, load_stub, reference_fz_, point_of(), ramp_inputs()

### Community 96 - "diagnostics_input"
Cohesion: 0.14
Nodes (12): model_conversion, applied, identification, model, diagnostics_input, conversion, evaluated, evaluated_id (+4 more)

### Community 97 - "tir_document.cpp"
Cohesion: 0.07
Nodes (51): tir_entry, key, line, quoted, section, value, value_length, value_offset (+43 more)

### Community 98 - "error_record"
Cohesion: 0.10
Nodes (25): error_record, code, file, key, line, message, row, text (+17 more)

### Community 99 - "test_coordinates.py"
Cohesion: 0.10
Nodes (17): iso_model(), _load(), sae_model(), test_convert_coordinate_system_bad_target_raises_invalid_argument(), test_convert_inputs_flips_only_fz(), test_convert_outputs_flips_fy_mz_my(), test_convert_round_trip_exact(), test_evaluate_batch_check_false_also_warns() (+9 more)

### Community 100 - "string"
Cohesion: 0.21
Nodes (15): characteristic_texts(), count_of(), expect_mixed_warning(), expect_no_contact_fields(), expect_printed(), expect_printed_row(), expect_printed_run(), expect_secants_equal_slopes_at_zero_slip() (+7 more)

### Community 101 - "model_handle"
Cohesion: 0.17
Nodes (3): channel_list(), model_handle, loaded_

### Community 102 - "scripts"
Cohesion: 0.11
Nodes (19): scripts, build, dev, docs:screenshots, prebuild, predev, predocs:screenshots, pretest (+11 more)

### Community 103 - "compilerOptions"
Cohesion: 0.11
Nodes (18): compilerOptions, isolatedModules, jsx, lib, module, moduleResolution, noEmit, noFallthroughCasesInSwitch (+10 more)

### Community 104 - "TEST"
Cohesion: 0.09
Nodes (5): block_of(), point_of(), root_tol(), solve_point(), TEST()

### Community 105 - "mf5x_parameters"
Cohesion: 0.13
Nodes (15): mf5x_parameters, coefficients, limits, metadata, mf52_model::with_parameters(), pac2002_forms(), pac2002_model::with_parameters(), selects_my_from_shifts() (+7 more)

### Community 106 - "longitudinal_terms"
Cohesion: 0.06
Nodes (31): planar_forces, fx_n, fy_n, lat, load, lon, lateral_terms, alpha_y (+23 more)

### Community 107 - "load_mf61"
Cohesion: 0.29
Nodes (6): bind_coefficients(), bind_limits(), bind_metadata(), load_mf61(), read_coefficient(), warn()

### Community 108 - "model_pair"
Cohesion: 0.33
Nodes (6): expect_batch_equals_point(), expect_models_bit_equal(), model_pair, mf52, pac2002, same_value()

### Community 109 - "TEST_P"
Cohesion: 0.12
Nodes (14): bit_equal(), concurrent_evaluation, evaluate_all(), evaluation_results, batch, characteristics, per_point, raw_per_point (+6 more)

### Community 110 - "tir_document"
Cohesion: 0.09
Nodes (23): model_kind, MF52, MF61, PAC2002, tir_document, file_name_, entry_location(), mark_consumed() (+15 more)

### Community 111 - "string_view"
Cohesion: 0.18
Nodes (22): append(), describe(), expect_error(), expect_longvl_missing(), expect_longvl_value(), expect_single_warning(), has_key(), insert_after() (+14 more)

### Community 112 - "char_curve"
Cohesion: 0.13
Nodes (18): char_curve, b, c, d, e, char_curve_x(), char_curve_y(), char_ellipse (+10 more)

### Community 113 - "stub_model"
Cohesion: 0.16
Nodes (10): stub_model, batch_calls, characteristic_values_, compute_calls, emit_nan_, rebuilt, rebuilt_plan, reference_value_ (+2 more)

### Community 114 - "TEST"
Cohesion: 0.06
Nodes (22): mf61_characteristics(), mf61_condition(), mf61_conditioned, contact, moved, point, mf61_peak_end(), mf61_pipeline() (+14 more)

### Community 115 - "lanes_of"
Cohesion: 0.18
Nodes (14): lanes_of(), near_scaled(), slip_state_at(), expect_channel_match(), all_forms(), block_inputs_of(), check_evaluate_against_hand_composition(), expect_block_matches_points() (+6 more)

### Community 117 - "route"
Cohesion: 0.21
Nodes (5): model_routing_mf_six_x, model_routing_unsupported_family, route(), route_clean(), TEST_P()

### Community 118 - "data_file"
Cohesion: 0.17
Nodes (12): cli_bad_input_table, expect_failed_run(), expect_usage_run(), file_arguments(), mixed_iso_fz_warning(), run_files(), run_result, code (+4 more)

### Community 119 - "linear_stub"
Cohesion: 0.17
Nodes (4): linear_stub, reference_fz_, stiffness_, with_fz_

### Community 120 - "outputs_t"
Cohesion: 0.12
Nodes (15): all_finite(), expect_same_bits(), outputs_of(), outputs_t, alpha_clamped, below_vxlow, fx_n, fy_n (+7 more)

### Community 121 - "components.json"
Cohesion: 0.12
Nodes (16): aliases, components, hooks, lib, ui, utils, iconLibrary, rsc (+8 more)

### Community 122 - "compilerOptions"
Cohesion: 0.12
Nodes (16): compilerOptions, isolatedModules, lib, module, moduleResolution, noEmit, noFallthroughCasesInSwitch, noUnusedLocals (+8 more)

### Community 123 - "compilerOptions"
Cohesion: 0.12
Nodes (16): compilerOptions, isolatedModules, lib, module, moduleResolution, noEmit, noFallthroughCasesInSwitch, noUnusedLocals (+8 more)

### Community 124 - "Build and test"
Cohesion: 0.12
Nodes (15): Benchmarks, Build and test, Configure and build, Dependencies (Conan), Dynamic analysis (sanitizers, Valgrind), Packet-scoped runs (parallel waves), Python bindings, Registration check (+7 more)

### Community 125 - "TEST"
Cohesion: 0.12
Nodes (3): row_of(), TEST(), wild()

### Community 126 - "Tire characteristics: stiffness, peak factors and slip at peak"
Cohesion: 0.20
Nodes (10): 1.1 The model stiffness parameters Kx and Ky (cited, derived), 1.2 MFeval Kxk and Kya, 1.3 DY and DX, 1. Definitions, 3. Evaluation method, 5. Peak-slip algorithm (policy), 6. Sign parity and ISO/SAE meaning, 7. Edge cases (+2 more)

### Community 127 - "make_model"
Cohesion: 0.27
Nodes (9): formulation, MF52, formulation_name(), PAC_COSINE, PAC_ELLIPSE, identify(), make_mf52(), make_model() (+1 more)

### Community 128 - "read_text_file"
Cohesion: 0.13
Nodes (4): read_text_file, directory_, path_, TEST_F()

### Community 129 - "TEST"
Cohesion: 0.05
Nodes (18): expect_error(), fixture(), load(), TEST(), warns(), with(), without(), agrees() (+10 more)

### Community 130 - "pure_scalars"
Cohesion: 0.19
Nodes (16): pure_scalars, bx, by, cx, cy, dx, dy, ex (+8 more)

### Community 132 - "Tire model domain primer"
Cohesion: 0.18
Nodes (10): Coefficient families (naming scheme), Inputs and outputs, Literature, Pure vs combined slip, Sign conventions and units, Testing implications, The .tir property file, Tire model domain primer (+2 more)

### Community 133 - "review.js"
Cohesion: 0.16
Nodes (13): blocking(), CPP_EXT, DEV_SCHEMA, FINDING, findingsText(), fixes, fixPrompt(), history (+5 more)

### Community 134 - "identification_probes"
Cohesion: 0.20
Nodes (6): identification_probes, inputs, slots, plan(), readings_of(), reference_point()

### Community 135 - "Tire coordinate systems: ISO, SAE, conversion and identification"
Cohesion: 0.13
Nodes (15): 10. Sources, 1. The user's definitions and figures, 3. Transforms A and B, 4.1 Transform A (15 flips plus FZMIN/FZMAX), 4.2 Transform B (15 flips plus ALPMIN/ALPMAX), 4.3 Necessity, 4. The two flip tables, 5. Code generalisations and the Ky = 0 caveat (+7 more)

### Community 136 - "limit_role"
Cohesion: 0.24
Nodes (10): limit_role, LOWER, UPPER, VALUE, make_channels(), sign_row, negated_by, role (+2 more)

### Community 138 - "identification_record"
Cohesion: 0.13
Nodes (15): decision_record, basis, confidence, negative_weight, positive_weight, sign, identification_record, classification (+7 more)

### Community 139 - "binding_role"
Cohesion: 0.18
Nodes (10): binding_role, COEFFICIENT, COMBINED, REQUIRED, SCALING, entry(), is_common_key(), keys_with_role() (+2 more)

### Community 140 - "weight_inputs"
Cohesion: 0.14
Nodes (15): cosine_partial_b(), cosine_partial_x(), cosine_shape(), weight_inputs, b, c, db_dalpha, db_dkappa (+7 more)

### Community 141 - "ellipse_ratios"
Cohesion: 0.40
Nodes (5): ellipse_ratios, fx_n, fy_n, rx, ry

### Community 142 - "Conventions shared by both models"
Cohesion: 0.14
Nodes (13): 10. Modes, 11. Pressure, 12. Units, 1. Axes, 2. Slip, 3. Inclination sign, 4. Output signs, 5. Speed (+5 more)

### Community 143 - "mf61_model"
Cohesion: 0.13
Nodes (3): MF 6.1 implementation, mf61_model, coefficients_

### Community 144 - "output_index"
Cohesion: 0.14
Nodes (14): output_index, BELOW_VXLOW, FX, FY, FZ_CLAMPED, IA_CLAMPED, MX, MY (+6 more)

### Community 145 - "dependencies"
Cohesion: 0.14
Nodes (14): dependencies, class-variance-authority, clsx, @fontsource/ibm-plex-mono, @fontsource/stix-two-text, @fontsource-variable/ibm-plex-sans, lucide-react, radix-ui (+6 more)

### Community 146 - "devDependencies"
Cohesion: 0.14
Nodes (14): devDependencies, @playwright/test, tailwindcss, @tailwindcss/vite, tw-animate-css, @types/node, @types/react, @types/react-dom (+6 more)

### Community 148 - "Agent workflow"
Cohesion: 0.15
Nodes (12): Agent workflow, Build-file ownership, Hand-off contracts, Loop limits, Parallel waves and build isolation, Pipeline, Python bindings verification, Routing table (+4 more)

### Community 149 - "5. Details"
Cohesion: 0.15
Nodes (12): 1. Scope, 2. Evaluation order (authoritative), 3. Equations, 4. Equation choices, 5.1 Moment arm s, 5.2 alpha_eq, 5.3 F'y and pure Mz0, 5.4 Friction ellipse (+4 more)

### Community 150 - "Validation"
Cohesion: 0.14
Nodes (13): 1. Verdict and limits, 2. Comparisons, 3. Method, 4.1 MF 5.2 reference comparison, 4.2 PAC2002 cross-check, 4.3 MF 6.1-family cross-check (limited), 4.4 Property-file sweep, 4. Results per comparison (+5 more)

### Community 151 - "options"
Cohesion: 0.15
Nodes (13): command, HELP, RUN, VERSION, options, action, inputs_path, mode (+5 more)

### Community 152 - "aligning_terms"
Cohesion: 0.15
Nodes (13): aligning_terms, alpha_r, alpha_t, br, bt, cr, ct, dr_nm (+5 more)

### Community 153 - "tir_reader"
Cohesion: 0.18
Nodes (3): tir_reader, consumed_, document_

### Community 154 - "error_code"
Cohesion: 0.13
Nodes (14): error_code, DUPLICATE_KEY, INVALID_ARGUMENT, INVALID_INPUT, INVALID_NUMBER, INVALID_VALUE, IO, MISSING_KEY (+6 more)

### Community 155 - "text.cpp"
Cohesion: 0.27
Nodes (6): equal_char(), iequals_ascii(), join(), to_upper_ascii(), trim(), upper_char()

### Community 156 - "magic-formula"
Cohesion: 0.15
Nodes (13): Benchmarks, Build and test, Code quality and coverage, Development with Claude Code agents, Ignore policy, License, magic-formula, Platform support (+5 more)

### Community 157 - "mf5x_forms"
Cohesion: 0.11
Nodes (21): combined_method, COSINE, FRICTION_ELLIPSE, fy_prime_form, SVYK, SVYK0, mf5x_forms, combined (+13 more)

### Community 158 - "ratios_of"
Cohesion: 0.24
Nodes (9): add_epsilon_of(), combined_at(), make_point(), printed_form_ratios(), ratio_values, rx, ry, ratios_of() (+1 more)

### Community 159 - "Test data and vectors"
Cohesion: 0.17
Nodes (11): 1. Data policy, 2. Fixture S (synthetic PAC2002), 3. Vectors, 4. Property tests, 5. Parser and routing fixtures (synthetic), 6.1 Vectors, 6.2 All eight channels for the vectors, 6.3 CLI rows (+3 more)

### Community 160 - "run"
Cohesion: 0.31
Nodes (6): characteristic_points(), eval(), make_model(), make_pair(), make_point(), run()

### Community 161 - "mf5x_bindings.cpp"
Cohesion: 0.28
Nodes (9): 3. Parameter set (names only), make_binding(), mf52_foreign_keys(), mf52_foreign_sections(), mf5x_common_bindings(), pac2002_camber_bindings(), bind_coefficients(), warn_foreign_keys() (+1 more)

### Community 162 - "mf5x_signs.hpp"
Cohesion: 0.27
Nodes (6): coefficient_keys_unique(), coefficient_sign, key, mf5x_coefficients, negated_by, count_negated_by()

### Community 163 - "limit_index"
Cohesion: 0.17
Nodes (12): limit_index, ALPMAX, ALPMIN, CAMMAX, CAMMIN, FZMAX, FZMIN, KPUMAX (+4 more)

### Community 164 - "reading_record"
Cohesion: 0.17
Nodes (12): reading_record, axis, load, load_scale, meaning, name, normalised, status (+4 more)

### Community 165 - "input_table_file"
Cohesion: 0.18
Nodes (4): input_table_file, directory_, path_, TEST_F()

### Community 166 - "size_t"
Cohesion: 0.20
Nodes (9): bad_table_case, file, line, message, expect_row_matches(), expect_runs_bit_equal(), expect_runs_match(), expect_zero_slope_iff_clamped() (+1 more)

### Community 167 - "invalid_case"
Cohesion: 0.22
Nodes (7): expect_members_equal(), invalid_case, key, member, member_ref, member, name

### Community 168 - "contract_case"
Cohesion: 0.20
Nodes (9): contract_case, kind, mode, name, model_kind, MF52, PAC2002_COSINE, PAC2002_ELLIPSE (+1 more)

### Community 169 - "concurrency_case"
Cohesion: 0.25
Nodes (4): concurrency_case, name, options, tir

### Community 170 - "number_test.cpp"
Cohesion: 0.18
Nodes (8): number_case, expected, text, parse_number_accept, parse_number_reject, reject_case, text, TEST_P()

### Community 171 - "input_of"
Cohesion: 0.18
Nodes (8): inputs_of(), point, alpha_rad, fz_n, gamma_rad, kappa, vx_mps, regime_grid()

### Community 172 - "expected_file"
Cohesion: 0.28
Nodes (6): describe(), expect_outcome(), expected_file, failure, name, load_fixture()

### Community 173 - "coefficient_binding"
Cohesion: 0.29
Nodes (6): coefficient_binding, key, mf5x_coefficients, role, bind_coefficient(), bind_span()

### Community 174 - "limit_pair"
Cohesion: 0.18
Nodes (10): expect_all_pairs_unbounded(), expect_pair_unbounded(), limit_pair, lower_index, lower_key, lower_value, section, upper_index (+2 more)

### Community 175 - "ThemeProvider.tsx"
Cohesion: 0.27
Nodes (12): lucide-react, readStored(), ThemeContext, ThemeContextValue, ThemeProvider(), useTheme(), writeStored(), ThemeToggle() (+4 more)

### Community 176 - "README.md"
Cohesion: 0.25
Nodes (3): docs/research: index and findings, Files, Findings

### Community 177 - "test_package.py"
Cohesion: 0.13
Nodes (7): _read(), test_notebook_executes_without_errors(), test_notebook_is_valid_and_has_no_outputs(), test_notebook_uses_only_public_api(), test_license_file_copies_match_repository_root(), test_license_is_the_canonical_apache_2_0_text(), test_version_matches_cmake_project_version()

### Community 178 - "probe_slot"
Cohesion: 0.18
Nodes (9): probe_slot, definition, input_value, load_n, load_scale, minus_row, missing_channel, output (+1 more)

### Community 179 - "expect_curvature_clamp"
Cohesion: 0.29
Nodes (4): expect_curvature_clamp(), expect_same_bits(), my_from_shifts_parameters(), s_with()

### Community 180 - "output_index"
Cohesion: 0.18
Nodes (11): output_index, BELOW_VXLOW, FX, FY, FZ_CLAMPED, IA_CLAMPED, MX, MY (+3 more)

### Community 181 - "orchestrator.md"
Cohesion: 0.29
Nodes (6): Constraints, Never do the job yourself, PR rule (you own it), Report back, Scope, Verification: your actual job

### Community 182 - "TEST_P"
Cohesion: 0.10
Nodes (16): column_of(), expect_point(), expect_same(), input_table_legacy_header, input_table_missing_column, input_table_units, legacy_case, key (+8 more)

### Community 183 - "indicator_form"
Cohesion: 0.25
Nodes (7): indicator_form, CENTRAL_DIFFERENCE, INPUT_LEVEL, OUTPUT_LEVEL, uses_channel(), uses_response(), definition()

### Community 184 - "4. Peak condition and existence"
Cohesion: 0.33
Nodes (6): 4.1 Stationary point of the pure curve (cited and derived), 4.2 Existence conditions (derived; pure slip, one side, with the E <= 1 clamp of pac2002.md section 4), 4.3 Side asymmetry, 4.4 When the closed-form peak and the search agree, 4.5 Scope of the closed form, 4. Peak condition and existence

### Community 185 - "fz_sign_census"
Cohesion: 0.33
Nodes (5): fz_sign_census, first_negative, first_positive, negative, positive

### Community 187 - "Web frontend by example"
Cohesion: 0.20
Nodes (9): 1. Open an example, 2. Identify and convert the coordinate system, 3. Compare sweeps, 4. Plot a characteristic, 5. Read the values, 6. Edit and save, Regenerating the screenshots, Run the app (+1 more)

### Community 188 - "Project policies"
Cohesion: 0.18
Nodes (11): 10. MF 6.1, 1. Evaluation pipeline, 2. Numerics, 3. Loading a .tir file, 4. Lexical refinements, 5. CLI, 6. Test data, 7. Items that may change (+3 more)

### Community 189 - "limit_index"
Cohesion: 0.20
Nodes (10): limit_index, ALPMAX, ALPMIN, CAMMAX, CAMMIN, FZMAX, FZMIN, KPUMAX (+2 more)

### Community 190 - "parse_number"
Cohesion: 0.28
Nodes (3): parse_integer(), parse_number(), TEST()

### Community 192 - "model_options"
Cohesion: 0.25
Nodes (8): model_options, mode, throw_if_model_failure, turn_slip, mf52_model::mf52_model(), mf5x_model::mf5x_model(), checked_options(), unchecked_options()

### Community 193 - "mf61_parameters"
Cohesion: 0.25
Nodes (7): mf61_parameters, coefficients, limits, metadata, mf61_model::mf61_model(), simple_parameters(), source_parameters()

### Community 194 - "magic-formula"
Cohesion: 0.22
Nodes (8): Agent roster (`.claude/agents/`), Agent workflow, Commands (contract), Layout (contract), magic-formula, Rules, Skills (`.claude/skills/`), Tech stack

### Community 195 - "slot_value"
Cohesion: 0.67
Nodes (3): slot_value, clamped, value

### Community 196 - "characteristic_index"
Cohesion: 0.22
Nodes (9): characteristic_index, DFX_DSR, DFY_DSA, DX, DY, FX_OVER_SR, FY_OVER_SA, SA_AT_PEAK_FY (+1 more)

### Community 197 - "characteristic_index"
Cohesion: 0.22
Nodes (9): characteristic_index, DFX_DSR, DFY_DSA, DX, DY, FX_OVER_SR, FY_OVER_SA, SA_AT_PEAK_FY (+1 more)

### Community 198 - "2. Closed-form derivatives (derived)"
Cohesion: 0.29
Nodes (7): 2.1 Sine and cosine Magic Formula, 2.2 Cosine method (PAC2002 and MF 5.2), 2.3 Friction ellipse (PAC2002 only), Fz-free form, 2.4 Genuine kinks (Rx > 0 or Ry > 0 with the own shifted slip +-0), 2.5 0/0 points of the ratio (value 1, derivative a limit), 2.6 Corrections to the first derivation, 2. Closed-form derivatives (derived)

### Community 199 - "expected_table"
Cohesion: 0.33
Nodes (6): expect_table_outcome(), expected_table, failure, line, name, rows

### Community 200 - "values"
Cohesion: 0.39
Nodes (6): y, values, expect_char(), expect_char_bits(), expect_char_nan(), outputs_of()

### Community 201 - "guard-master.sh"
Cohesion: 0.46
Nodes (7): block(), current_branch(), is_gh_word(), is_git_word(), is_protected(), resolve_dir(), guard-master.sh script

### Community 202 - "C++ conventions"
Cohesion: 0.25
Nodes (7): C++ conventions, Coverage-proof idioms, Formatting and static analysis, Namespaces and layout, Ownership, values, const, Testing (GoogleTest), Units

### Community 203 - "CLI"
Cohesion: 0.33
Nodes (6): CLI, Coordinate systems, Diagnostics and exit codes, Input table, Models, Output

### Community 204 - "MF 5.2 (FITTYP = 6)"
Cohesion: 0.25
Nodes (7): 1. Status and settings, 2. Identification, 4. Evaluation order and equations (authoritative), 5. Behaviours not adopted, 6. Validation status, 7. Implementation guidance, MF 5.2 (FITTYP = 6)

### Community 206 - "expect_flips_declared_columns"
Cohesion: 0.40
Nodes (3): expect_flips_declared_columns(), expect_same_value_bits(), sign_sample()

### Community 207 - "input_index"
Cohesion: 0.25
Nodes (8): input_index, FZ, IA, P, SA, SR, TS, VX

### Community 208 - "Model routing: FITTYP and PROPERTY_FILE_FORMAT"
Cohesion: 0.50
Nodes (3): FITTYP and PROPERTY_FILE_FORMAT values, Model routing: FITTYP and PROPERTY_FILE_FORMAT, Routing table

### Community 209 - "string_view"
Cohesion: 0.10
Nodes (7): axis_weight(), check_weights_zero(), indicator_names_unique(), tire_model, unsupported_fittyp, code, message

### Community 210 - "longvl_case"
Cohesion: 0.25
Nodes (8): expect_same_limits(), longvl_case, expected, label, longvl, missing, qsy3, qsy4

### Community 211 - "expect_failure"
Cohesion: 0.29
Nodes (3): caught_failure(), expect_failure(), expect_invalid_argument()

### Community 212 - "general-developer.md"
Cohesion: 0.29
Nodes (6): Constraints, Delegation, Git / PR mechanics (only you, only on explicit orchestrator instruction), Report back, Scope, You do NOT

### Community 213 - "researcher.md"
Cohesion: 0.29
Nodes (6): Constraints, Never read anything yourself, Report back, Scope, Sources (typical), You do NOT

### Community 215 - "append_state"
Cohesion: 0.67
Nodes (6): append_aln(), append_lane(), append_lat(), append_load(), append_lon(), append_state()

### Community 218 - "version"
Cohesion: 0.33
Nodes (3): version(), TEST(), version

### Community 219 - "tests/data"
Cohesion: 0.29
Nodes (6): Documented line numbers (1-based, comments, header and unit line included), Input tables, Numerics, Origin, tests/data, Tire property files

### Community 220 - "bits"
Cohesion: 0.33
Nodes (6): bits(), expect_bits(), expect_same_samples(), all_bindings(), expect_combined_zero(), expect_same_coefficients()

### Community 221 - "cpp-developer.md"
Cohesion: 0.33
Nodes (5): Constraints, Delegation (via the Agent tool), Report back, Scope, You do NOT

### Community 222 - "Coverage"
Cohesion: 0.33
Nodes (5): Coverage, Exclusion policy, Python bindings, Reporting uncovered code, Run the gate

### Community 224 - ".tir property file"
Cohesion: 0.33
Nodes (5): 1. File structure, 2. Lexical rules, 3. Required keys and defaults, 4. [UNITS], .tir property file

### Community 225 - "pull_request_template.md"
Cohesion: 0.33
Nodes (5): Changes, Checklist, Review verdict, Summary, Tests & coverage

### Community 226 - "nightly-findings.sh"
Cohesion: 0.60
Nodes (5): group_end(), group_start(), report_error(), nightly-findings.sh script, usage()

### Community 229 - "input_index"
Cohesion: 0.33
Nodes (6): input_index, FZ, IA, SA, SR, VX

### Community 233 - "check_combine_value_bit_equal"
Cohesion: 0.40
Nodes (5): check_combine_value_bit_equal(), check_select(), expect_bits(), expect_combine_equal(), expect_finite()

### Community 236 - "architect.md"
Cohesion: 0.40
Nodes (4): Constraints, Report back (fixed structure, handed to `planner`), Scope, You do NOT

### Community 237 - "planner.md"
Cohesion: 0.40
Nodes (4): Dispatch modes, Report back, Scope, You do NOT

### Community 238 - "project-explorer.md"
Cohesion: 0.40
Nodes (3): Read-only, Report back, Scope

### Community 242 - "tests/data/reference"
Cohesion: 0.40
Nodes (4): Files, Format, `mf52_mfeval.csv`, tests/data/reference

### Community 244 - "ellipse_case"
Cohesion: 0.40
Nodes (5): ellipse_case, alpha, dual, plain, plain_state

### Community 245 - "general-purpose.md"
Cohesion: 0.50
Nodes (3): Constraints, Report back, Scope

### Community 246 - "reviewer.md"
Cohesion: 0.50
Nodes (3): Read-only, Report back, Scope

### Community 247 - "test-runner.md"
Cohesion: 0.50
Nodes (3): Report back (exact template, nothing more), Scope, Strictly limited

### Community 249 - "Open questions"
Cohesion: 0.50
Nodes (3): 1. Open items, 2. Settled points, Open questions

### Community 251 - "coefficient_entry"
Cohesion: 0.50
Nodes (3): coefficient_entry, key, mf61_coefficients

### Community 252 - "unit_entry"
Cohesion: 0.50
Nodes (4): unit_entry, column, factor, unit

### Community 254 - "si_unit"
Cohesion: 0.50
Nodes (3): si_unit, key, symbol

### Community 257 - "slip_point"
Cohesion: 0.50
Nodes (4): slip_point, alpha, kappa, seed_alpha

### Community 262 - "lanes.hpp"
Cohesion: 0.29
Nodes (4): clamp_to_limits(), elementwise_hypot(), sgn_plus(), to_flag()

## Knowledge Gaps
- **997 isolated node(s):** `RUN`, `HELP`, `VERSION`, `action`, `inputs_path` (+992 more)
  These have ≤1 connection - possible missing edges or undocumented components. (Counts symbols only; 3159 node(s) total have ≤1 connection when file, concept and rationale nodes are included.)
- **37 thin communities (<3 nodes) omitted from report** — run `graphify query` to explore isolated nodes.

## Suggested Questions
_Questions this graph is uniquely positioned to answer:_

- **Why does `error` connect `error` to `TEST`, `fixtures.hpp`, `evaluation_types.hpp`, `TEST`, `mfeval_reference_test.cpp`, `cli.cpp`, `error_code`, `Class hierarchy`, `mf_bench.cpp`, `tir_edit_test.cpp`, `tire_model.cpp`, `input_table.cpp`, `mf_wasm.cpp`, `Coordinate systems`, `TEST`, `location`, `Class hierarchy`, `coordinate_identification.cpp`, `_core`, `identify_model`, `string_view`, `configuration`, `TEST`, `tir_document.cpp`, `error_record`, `load_mf61`, `tir_document`, `string_view`, `route`?**
  _High betweenness centrality (0.112) - this node is a cross-community bridge._
- **Are the 5 inferred relationships involving `TEST()` (e.g. with `lane()` and `lanes_of()`) actually correct?**
  _`TEST()` has 5 INFERRED edges - model-reasoned connections that need verification._
- **What connects `RUN`, `HELP`, `VERSION` to the rest of the system?**
  _997 weakly-connected nodes found - possible documentation gaps or missing edges._
- **Should `mf61_coefficients` be split into smaller, more focused modules?**
  _Cohesion score 0.010638297872340425 - nodes in this community are weakly interconnected._
- **Why does `version` connect `version` to `fixtures.hpp`, `evaluation_types.hpp`, `package.json`, `mf_wasm.cpp`, `cli.cpp`?**
  _High betweenness centrality (0.108) - this node is a cross-community bridge._
- **Should `mf61_equations` be split into smaller, more focused modules?**
  _Cohesion score 0.07074311023622047 - nodes in this community are weakly interconnected._
- **Why does `Layers` connect `Class hierarchy` to `dual_lane`, `identification_probes.cpp`, `TEST`, `coordinate_system`, `CoordinateSystemPanel.tsx`, `tir_reader`, `Coordinate systems`, `TireModel`, `__init__.py`?**
  _High betweenness centrality (0.103) - this node is a cross-community bridge._