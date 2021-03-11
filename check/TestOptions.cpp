#include <cstdio>

#include "HMPSIO.h"
#include "Highs.h"
#include "LoadOptions.h"
#include "catch.hpp"

const bool dev_run = false;

TEST_CASE("internal-options", "[highs_options]") {
  HighsOptions options;
  options.output_flag = false;
  OptionStatus return_status =
      checkOptions(options.log_options, options.records);
  REQUIRE(return_status == OptionStatus::OK);

  options.options_file = std::string(HIGHS_DIR) + "/check/sample_options_file";

  bool success = loadOptionsFromFile(options);
  REQUIRE(success == true);
  REQUIRE(options.presolve == on_string);
  REQUIRE(options.small_matrix_value == 0.001);
  REQUIRE(options.mps_parser_type_free);

  if (dev_run) reportOptions(stdout, options.records, true);

  return_status = checkOptions(options.log_options, options.records);
  REQUIRE(return_status == OptionStatus::OK);

  // Check setting boolean options
  std::string setting_string = "fixed";
  return_status = setOptionValue(options.log_options, "mps_parser_type_free",
                                 options.records, setting_string);
  REQUIRE(return_status == OptionStatus::ILLEGAL_VALUE);

  return_status = setOptionValue(options.log_options, "mps_parser_type_free",
                                 options.records, "fixed");
  REQUIRE(return_status == OptionStatus::ILLEGAL_VALUE);

  return_status = setOptionValue(options.log_options, "mps_parser_type_free",
                                 options.records, "False");
  REQUIRE(return_status == OptionStatus::OK);

  return_status = setOptionValue(options.log_options, "mps_parser_type_free",
                                 options.records, "F");
  REQUIRE(return_status == OptionStatus::OK);

  bool mps_parser_type_free = false;
  return_status = setOptionValue(options.log_options, "mps_parser_type_free",
                                 options.records, mps_parser_type_free);
  REQUIRE(return_status == OptionStatus::OK);

  return_status = setOptionValue(options.log_options, "mps_parser_type",
                                 options.records, true);
  REQUIRE(return_status == OptionStatus::UNKNOWN_OPTION);

  // Check setting int options

  return_status =
      setOptionValue(options.log_options, "allowed_simplex_matrix_scale_factor",
                     options.records, -1);
  REQUIRE(return_status == OptionStatus::ILLEGAL_VALUE);

  return_status =
      setOptionValue(options.log_options, "allowed_simplex_matrix_scale_factor",
                     options.records, 25);
  REQUIRE(return_status == OptionStatus::ILLEGAL_VALUE);

  std::string allowed_simplex_matrix_scale_factor_string = "1e-7";
  return_status = setOptionValue(
      options.log_options, "allowed_simplex_matrix_scale_factor",
      options.records, allowed_simplex_matrix_scale_factor_string);
  REQUIRE(return_status == OptionStatus::ILLEGAL_VALUE);

  return_status =
      setOptionValue(options.log_options, "allowed_simplex_matrix_scale_factor",
                     options.records, "3.14159");
  REQUIRE(return_status == OptionStatus::ILLEGAL_VALUE);

  if (dev_run) {
    printf("\nAfter setting allowed_simplex_matrix_scale_factor to 1\n");
    reportOptions(stdout, options.records);
  }

  double allowed_simplex_matrix_scale_factor_double = 1e-7;
  return_status = setOptionValue(
      options.log_options, "allowed_simplex_matrix_scale_factor",
      options.records, allowed_simplex_matrix_scale_factor_double);
  REQUIRE(return_status == OptionStatus::ILLEGAL_VALUE);

  int allowed_simplex_matrix_scale_factor = 12;
  return_status =
      setOptionValue(options.log_options, "allowed_simplex_matrix_scale_factor",
                     options.records, allowed_simplex_matrix_scale_factor);
  REQUIRE(return_status == OptionStatus::OK);

  if (dev_run) {
    printf("\nAfter testing int options\n");
    reportOptions(stdout, options.records);
  }

  // Check setting double options

  return_status = setOptionValue(options.log_options, "large_matrix_value",
                                 options.records, -1);
  REQUIRE(return_status == OptionStatus::ILLEGAL_VALUE);

  return_status = setOptionValue(options.log_options, "large_matrix_value",
                                 options.records, "1");
  REQUIRE(return_status == OptionStatus::OK);

  return_status = setOptionValue(options.log_options, "small_matrix_value",
                                 options.records, -1);
  REQUIRE(return_status == OptionStatus::ILLEGAL_VALUE);

  return_status = setOptionValue(options.log_options, "small_matrix_value",
                                 options.records, "1e-6");
  REQUIRE(return_status == OptionStatus::OK);

  double small_matrix_value = 1e-7;
  return_status = setOptionValue(options.log_options, "small_matrix_value",
                                 options.records, small_matrix_value);
  REQUIRE(return_status == OptionStatus::OK);

  // Check setting string options

  return_status = setOptionValue(options.log_options, presolve_string,
                                 options.records, "ml.mps");
  REQUIRE(return_status == OptionStatus::ILLEGAL_VALUE);

  std::string model_file = "ml.mps";
  return_status = setOptionValue(options.log_options, presolve_string,
                                 options.records, model_file);
  REQUIRE(return_status == OptionStatus::ILLEGAL_VALUE);

  return_status = setOptionValue(options.log_options, presolve_string,
                                 options.records, "off");
  REQUIRE(return_status == OptionStatus::OK);

  std::string presolve = "choose";
  return_status = setOptionValue(options.log_options, presolve_string,
                                 options.records, presolve);
  REQUIRE(return_status == OptionStatus::OK);

  return_status = setOptionValue(options.log_options, model_file_string,
                                 options.records, model_file);
  REQUIRE(return_status == OptionStatus::OK);

  if (dev_run) reportOptions(stdout, options.records);

  bool get_mps_parser_type_free;
  return_status = getOptionValue(options.log_options, "mps_parser_type_free",
                                 options.records, get_mps_parser_type_free);
  REQUIRE(return_status == OptionStatus::OK);
  REQUIRE(get_mps_parser_type_free == false);

  int get_allowed_simplex_matrix_scale_factor;
  return_status =
      getOptionValue(options.log_options, "allowed_simplex_matrix_scale_factor",
                     options.records, get_allowed_simplex_matrix_scale_factor);
  REQUIRE(return_status == OptionStatus::OK);
  REQUIRE(get_allowed_simplex_matrix_scale_factor ==
          allowed_simplex_matrix_scale_factor);

  double get_small_matrix_value;
  return_status = getOptionValue(options.log_options, "small_matrix_value",
                                 options.records, get_small_matrix_value);
  REQUIRE(return_status == OptionStatus::OK);
  REQUIRE(get_small_matrix_value == small_matrix_value);

  std::string get_model_file;
  return_status = getOptionValue(options.log_options, "model_file",
                                 options.records, get_model_file);
  REQUIRE(return_status == OptionStatus::OK);
  REQUIRE(get_model_file == model_file);

  return_status = checkOptions(options.log_options, options.records);
  REQUIRE(return_status == OptionStatus::OK);
}

TEST_CASE("highs-options", "[highs_options]") {
  Highs highs;
  if (!dev_run) {
    highs.setHighsOptionValue("output_flag", false);
  }
  HighsStatus return_status = highs.writeHighsOptions("Highs.set");
  REQUIRE(return_status == HighsStatus::OK);

  // Check setting boolean options
  std::string setting_string = "fixed";
  return_status =
      highs.setHighsOptionValue("mps_parser_type_free", setting_string);
  REQUIRE(return_status == HighsStatus::Error);

  return_status = highs.setHighsOptionValue("mps_parser_type_free", "fixed");
  REQUIRE(return_status == HighsStatus::Error);

  return_status = highs.setHighsOptionValue("mps_parser_type_free", "False");
  REQUIRE(return_status == HighsStatus::OK);

  return_status = highs.setHighsOptionValue("mps_parser_type_free", "F");
  REQUIRE(return_status == HighsStatus::OK);

  bool mps_parser_type_free = false;
  return_status =
      highs.setHighsOptionValue("mps_parser_type_free", mps_parser_type_free);
  REQUIRE(return_status == HighsStatus::OK);

  return_status = highs.setHighsOptionValue("mps_parser_type", true);
  REQUIRE(return_status == HighsStatus::Error);

  // Check setting int options

  return_status =
      highs.setHighsOptionValue("allowed_simplex_matrix_scale_factor", -1);
  REQUIRE(return_status == HighsStatus::Error);

  return_status =
      highs.setHighsOptionValue("allowed_simplex_matrix_scale_factor", 25);
  REQUIRE(return_status == HighsStatus::Error);

  std::string allowed_simplex_matrix_scale_factor_string = "1e-7";
  return_status =
      highs.setHighsOptionValue("allowed_simplex_matrix_scale_factor",
                                allowed_simplex_matrix_scale_factor_string);
  REQUIRE(return_status == HighsStatus::Error);

  return_status = highs.setHighsOptionValue(
      "allowed_simplex_matrix_scale_factor", "3.14159");
  REQUIRE(return_status == HighsStatus::Error);

  if (dev_run)
    printf("\nAfter setting allowed_simplex_matrix_scale_factor to 1\n");
  return_status = highs.writeHighsOptions("Highs.set");
  REQUIRE(return_status == HighsStatus::OK);

  double allowed_simplex_matrix_scale_factor_double = 1e-7;
  return_status =
      highs.setHighsOptionValue("allowed_simplex_matrix_scale_factor",
                                allowed_simplex_matrix_scale_factor_double);
  REQUIRE(return_status == HighsStatus::Error);

  int allowed_simplex_matrix_scale_factor = 12;
  return_status =
      highs.setHighsOptionValue("allowed_simplex_matrix_scale_factor",
                                allowed_simplex_matrix_scale_factor);
  REQUIRE(return_status == HighsStatus::OK);

  if (dev_run) printf("\nAfter testing int options\n");
  return_status = highs.writeHighsOptions("Highs.set");
  REQUIRE(return_status == HighsStatus::OK);

  // Check setting double options

  return_status = highs.setHighsOptionValue("large_matrix_value", -1);
  REQUIRE(return_status == HighsStatus::Error);

  return_status = highs.setHighsOptionValue("large_matrix_value", "1");
  REQUIRE(return_status == HighsStatus::OK);

  return_status = highs.setHighsOptionValue("small_matrix_value", -1);
  REQUIRE(return_status == HighsStatus::Error);

  return_status = highs.setHighsOptionValue("small_matrix_value", "1e-6");
  REQUIRE(return_status == HighsStatus::OK);

  double small_matrix_value = 1e-7;
  return_status =
      highs.setHighsOptionValue("small_matrix_value", small_matrix_value);
  REQUIRE(return_status == HighsStatus::OK);

  // Check setting string options

  return_status = highs.setHighsOptionValue(presolve_string, "ml.mps");
  REQUIRE(return_status == HighsStatus::Error);

  std::string model_file = "ml.mps";
  return_status = highs.setHighsOptionValue(presolve_string, model_file);
  REQUIRE(return_status == HighsStatus::Error);

  return_status = highs.setHighsOptionValue(presolve_string, "off");
  REQUIRE(return_status == HighsStatus::OK);

  std::string presolve = "choose";
  return_status = highs.setHighsOptionValue(presolve_string, presolve);
  REQUIRE(return_status == HighsStatus::OK);

  return_status = highs.setHighsOptionValue(model_file_string, model_file);
  REQUIRE(return_status == HighsStatus::OK);

  return_status = highs.writeHighsOptions("Highs.set");
  REQUIRE(return_status == HighsStatus::OK);

  HighsOptionType highs_option_type;

  bool get_mps_parser_type_free;
  return_status = highs.getHighsOptionValue("mps_parser_type_free",
                                            get_mps_parser_type_free);
  REQUIRE(return_status == HighsStatus::OK);
  REQUIRE(get_mps_parser_type_free == false);

  return_status =
      highs.getHighsOptionType("mps_parser_type_free", highs_option_type);
  REQUIRE(return_status == HighsStatus::OK);
  REQUIRE(highs_option_type == HighsOptionType::BOOL);

  int get_allowed_simplex_matrix_scale_factor;
  return_status =
      highs.getHighsOptionValue("allowed_simplex_matrix_scale_factor",
                                get_allowed_simplex_matrix_scale_factor);
  REQUIRE(return_status == HighsStatus::OK);
  REQUIRE(get_allowed_simplex_matrix_scale_factor ==
          allowed_simplex_matrix_scale_factor);

  return_status = highs.getHighsOptionType(
      "allowed_simplex_matrix_scale_factor", highs_option_type);
  REQUIRE(return_status == HighsStatus::OK);
  REQUIRE(highs_option_type == HighsOptionType::INT);

  double get_small_matrix_value;
  return_status =
      highs.getHighsOptionValue("small_matrix_value", get_small_matrix_value);
  REQUIRE(return_status == HighsStatus::OK);
  REQUIRE(get_small_matrix_value == small_matrix_value);

  return_status =
      highs.getHighsOptionType("small_matrix_value", highs_option_type);
  REQUIRE(return_status == HighsStatus::OK);
  REQUIRE(highs_option_type == HighsOptionType::DOUBLE);

  std::string get_model_file;
  return_status = highs.getHighsOptionValue("model_file", get_model_file);
  REQUIRE(return_status == HighsStatus::OK);
  REQUIRE(get_model_file == model_file);

  return_status = highs.getHighsOptionType("model_file", highs_option_type);
  REQUIRE(return_status == HighsStatus::OK);
  REQUIRE(highs_option_type == HighsOptionType::STRING);

  HighsOptions options = highs.getHighsOptions();
  REQUIRE(options.model_file == model_file);
  REQUIRE(options.small_matrix_value == small_matrix_value);
  REQUIRE(options.allowed_simplex_matrix_scale_factor ==
          allowed_simplex_matrix_scale_factor);
  REQUIRE(options.mps_parser_type_free == mps_parser_type_free);
}
